#include "search.hpp"
#include "position.hpp"
#include "usi.hpp"
#include "evaluate.hpp"
#include "movePicker.hpp"
#include "tt.hpp"
#include "generateMoves.hpp"
#include "thread.hpp"
#include "timeManager.hpp"
#include "book.hpp"

// 一箇所でしか呼ばないので、FORCE_INLINE
FORCE_INLINE void ThreadPool::wakeUp(Searcher* s) {
	for (size_t i = 0; i < size(); ++i) {
		(*this)[i]->maxPly = 0;
	}
	sleepWhileIdle_ = s->options["Use_Sleeping_Threads"];
}
// 一箇所でしか呼ばないので、FORCE_INLINE
FORCE_INLINE void ThreadPool::sleep() {
	sleepWhileIdle_ = true;
}

#if defined USE_GLOBAL
volatile SignalsType Searcher::signals;
LimitsType Searcher::limits;
std::vector<Move> Searcher::searchMoves;
Time Searcher::searchTimer;
StateStackPtr Searcher::setUpStates;
std::vector<RootMove> Searcher::rootMoves;
size_t Searcher::pvSize;
size_t Searcher::pvIdx;
TimeManager Searcher::timeManager;
Ply Searcher::bestMoveChanges;
History Searcher::history;
Gains Searcher::gains;
TranspositionTable Searcher::tt;
#if defined INANIWA_SHIFT
InaniwaFlag Searcher::inaniwaFlag;
#endif
Position Searcher::rootPosition(nullptr);
ThreadPool Searcher::threads;
OptionsMap Searcher::options;
Searcher* Searcher::thisptr;
#endif

void Searcher::init() {
#if defined USE_GLOBAL
#else
	thisptr = this;
#endif
	options.init(thisptr);
	threads.init(thisptr);
	tt.setSize(options["USI_Hash"]);
}

namespace {
	// true にすると、シングルスレッドで動作する。デバッグ用。
	const bool FakeSplit = false;

	inline Score razorMargin(const Depth d) {
		return static_cast<Score>(512 + 16 * static_cast<int>(d));
	}

	Score FutilityMargins[16][64]; // [depth][moveCount]
	inline Score futilityMargin(const Depth depth, const int moveCount) {
		return (depth < 7 * OnePly ?
				FutilityMargins[std::max(depth, Depth1)][std::min(moveCount, 63)]
				: 2 * ScoreInfinite);
	}

	int FutilityMoveCounts[32];    // [depth]

	s8 Reductions[2][64][64]; // [pv][depth][moveNumber]
	template <bool PVNode> inline Depth reduction(const Depth depth, const int moveCount) {
		return static_cast<Depth>(Reductions[PVNode][std::min(Depth(depth/OnePly), Depth(63))][std::min(moveCount, 63)]);
	}

	// checkTime() を呼び出す最大間隔(msec)
	const int TimerResolution = 5;

	struct Skill {
		Skill(const int l, const int mr, Searcher* s)
			: level(l),
			  max_random_score_diff(static_cast<Score>(mr)),
			  best(Move::moveNone()) {}
		~Skill() {}
		void swapIfEnabled(Searcher* s) {
			if (enabled()) {
				auto it = std::find(s->rootMoves.begin(),
									s->rootMoves.end(),
									(!best.isNone() ? best : pickMove(s)));
				if (s->rootMoves.begin() != it)
					SYNCCOUT << "info string swap multipv 1, " << it - s->rootMoves.begin() + 1 << SYNCENDL;
				std::swap(s->rootMoves[0], *it);
			}
		}
		bool enabled() const { return level < 20 || max_random_score_diff != ScoreZero; }
		bool timeToPick(const int depth) const { return depth == 1 + level; }
		Move pickMove(Searcher* s) {
			// level については未対応。max_random_score_diff についてのみ対応する。
			if (max_random_score_diff != ScoreZero) {
				size_t i = 1;
				for (; i < s->pvSize; ++i) {
					if (max_random_score_diff < s->rootMoves[0].score_ - s->rootMoves[i].score_)
						break;
				}
				// 0 から i-1 までの間でランダムに選ぶ。
				std::uniform_int_distribution<size_t> dist(0, i-1);
				best = s->rootMoves[dist(g_randomTimeSeed)].pv_[0];
				return best;
			}
			best = s->rootMoves[0].pv_[0];
			return best;
		}

		int level;
		Score max_random_score_diff;
		Move best;
	};

	inline bool checkIsDangerous() {
		// not implement
		// 使用しないで良いかも知れない。
		return false;
	}

	// 1 ply前の first move によって second move が合法手にするか。
	bool allows(const Position& pos, const Move first, const Move second) {
		const Square m1to   = first.to();
		const Square m1from = first.from();
		const Square m2from = second.from();
		const Square m2to   = second.to();
		if (m1to == m2from || m2to == m1from) {
			return true;
		}

		if (second.isDrop() && first.isDrop()) {
			return false;
		}

		if (!second.isDrop() && !first.isDrop()) {
			if (betweenBB(m2from, m2to).isSet(m1from)) {
				return true;
			}
		}

		const PieceType m1pt = first.pieceTypeFromOrDropped();
		const Color us = pos.turn();
		const Bitboard occ = (second.isDrop() ? pos.occupiedBB() : pos.occupiedBB() ^ setMaskBB(m2from));
		const Bitboard m1att = pos.attacksFrom(m1pt, us, m1to, occ);
		if (m1att.isSet(m2to)) {
			return true;
		}

		if (m1att.isSet(pos.kingSquare(us))) {
			return true;
		}

		return false;
	}

	Score scoreToTT(const Score s, const Ply ply) {
		assert(s != ScoreNone);

		return (ScoreMateInMaxPly <= s ? s + static_cast<Score>(ply)
				: s <= ScoreMatedInMaxPly ? s - static_cast<Score>(ply)
				: s);
	}

	Score scoreFromTT(const Score s, const Ply ply) {
		return (s == ScoreNone ? ScoreNone
				: ScoreMateInMaxPly <= s ? s - static_cast<Score>(ply)
				: s <= ScoreMatedInMaxPly ? s + static_cast<Score>(ply)
				: s);
	}

	// fitst move によって、first move の相手側の second move を違法手にするか。
	bool refutes(const Position& pos, const Move first, const Move second) {
		assert(pos.isOK());

		const Square m2to = second.to();
		const Square m1from = first.from(); // 駒打でも今回はこれで良い。

		if (m1from == m2to) {
			return true;
		}

		const PieceType m2ptFrom = second.pieceTypeFrom();
		if (second.isCaptureOrPromotion()
			&& ((pos.pieceScore(second.cap()) <= pos.pieceScore(m2ptFrom))
				|| m2ptFrom == King))
		{
			// first により、新たに m2to に当たりになる駒があるなら true
			assert(!second.isDrop());

			const Color us = pos.turn();
			const Square m1to = first.to();
			const Square m2from = second.from();
			Bitboard occ = pos.occupiedBB() ^ setMaskBB(m2from) ^ setMaskBB(m1to);
			PieceType m1ptTo;

			if (first.isDrop()) {
				m1ptTo = first.pieceTypeDropped();
			}
			else {
				m1ptTo = first.pieceTypeTo();
				occ ^= setMaskBB(m1from);
			}

			if (pos.attacksFrom(m1ptTo, us, m1to, occ).isSet(m2to)) {
				return true;
			}

			const Color them = oppositeColor(us);
			// first で動いた後、sq へ当たりになっている遠隔駒
			const Bitboard xray =
				(pos.attacksFrom<Lance>(them, m2to, occ) & pos.bbOf(Lance, us))
				| (pos.attacksFrom<Rook  >(m2to, occ) & pos.bbOf(Rook, Dragon, us))
				| (pos.attacksFrom<Bishop>(m2to, occ) & pos.bbOf(Bishop, Horse, us));

			// sq へ当たりになっている駒のうち、first で動くことによって新たに当たりになったものがあるなら true
			if (xray.isNot0() && (xray ^ (xray & queenAttack(m2to, pos.occupiedBB()))).isNot0()) {
				return true;
			}
		}

		if (!second.isDrop()
			&& isSlider(m2ptFrom)
			&& betweenBB(second.from(), m2to).isSet(first.to())
			&& ScoreZero <= pos.seeSign(first))
		{
			return true;
		}

		return false;
	}

	std::string scoreToUSI(const Score score, const Score alpha, const Score beta) {
		std::stringstream ss;

		if (abs(score) < ScoreMateInMaxPly) {
			// cp は centi pawn の略
			ss << "cp " << score * 100 / PawnScore;
		}
		else {
			// mate の後には、何手で詰むかを表示する。
			ss << "mate " << (0 < score ? ScoreMate0Ply - score : -ScoreMate0Ply - score);
		}

		ss << (beta <= score ? " lowerbound" : score <= alpha ? " upperbound" : "");

		return ss.str();
	}

	inline std::string scoreToUSI(const Score score) {
		return scoreToUSI(score, -ScoreInfinite, ScoreInfinite);
	}

	void pvInfoToLog(Position& pos, const Ply d, const Score bestScore,
					 const int elapsedTime, const Move pv[])
	{
		// not implemented
	}
}

std::string Searcher::pvInfoToUSI(Position& pos, const Ply depth, const Score alpha, const Score beta) {
	const int t = searchTimer.elapsed();
	const size_t usiPVSize = pvSize;
	Ply selDepth = 0; // 選択的に読んでいる部分の探索深さ。
	std::stringstream ss;

	for (size_t i = 0; i < threads.size(); ++i) {
		if (selDepth < threads[i]->maxPly) {
			selDepth = threads[i]->maxPly;
		}
	}

	for (size_t i = usiPVSize-1; 0 <= static_cast<int>(i); --i) {
		const bool update = (i <= pvIdx);

		if (depth == 1 && !update) {
			continue;
		}

		const Ply d = (update ? depth : depth - 1);
		const Score s = (update ? rootMoves[i].score_ : rootMoves[i].prevScore_);

		ss << "info depth " << d
		   << " seldepth " << selDepth
		   << " score " << (i == pvIdx ? scoreToUSI(s, alpha, beta) : scoreToUSI(s))
		   << " nodes " << pos.nodesSearched()
		   << " nps " << (0 < t ? pos.nodesSearched() * 1000 / t : 0)
		   << " time " << t
		   << " multipv " << i + 1
		   << " pv ";

		for (int j = 0; !rootMoves[i].pv_[j].isNone(); ++j) {
			ss << " " << rootMoves[i].pv_[j].toUSI();
		}

		ss << std::endl;
	}
	return ss.str();
}

template <NodeType NT, bool INCHECK>
Score Searcher::qsearch(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth) {
	const bool PVNode = (NT == PV);

	assert(NT == PV || NT == NonPV);
	assert(INCHECK == pos.inCheck());
	assert(-ScoreInfinite <= alpha && alpha < beta && beta <= ScoreInfinite);
	assert(PVNode || (alpha == beta - 1));
	assert(depth <= Depth0);

	StateInfo st;
	const TTEntry* tte;
	Key posKey;
	Move ttMove;
	Move move;
	Move bestMove;
	Score bestScore;
	Score score;
	Score ttScore;
	Score futilityScore;
	Score futilityBase;
	Score oldAlpha;
	bool givesCheck;
	bool evasionPrunable;
	Depth ttDepth;

	if (PVNode) {
		oldAlpha = alpha;
	}

	ss->currentMove = bestMove = Move::moveNone();
	ss->ply = (ss-1)->ply + 1;

	if (MaxPly < ss->ply) {
		return ScoreDraw;
	}

	ttDepth = ((INCHECK || DepthQChecks <= depth) ? DepthQChecks : DepthQNoChecks);

	posKey = pos.getKey();
	tte = tt.probe(posKey);
	ttMove = (tte != nullptr ? move16toMove(tte->move(), pos) : Move::moveNone());
	ttScore = (tte != nullptr ? scoreFromTT(tte->score(), ss->ply) : ScoreNone);

	if (tte != nullptr
		&& ttDepth <= tte->depth()
		&& ttScore != ScoreNone // アクセス競合が起きたときのみ、ここに引っかかる。
		&& (PVNode ? tte->type() == BoundExact
			: (beta <= ttScore ? (tte->type() & BoundLower)
			   : (tte->type() & BoundUpper))))
	{
		ss->currentMove = ttMove;
		return ttScore;
	}

	pos.setNodesSearched(pos.nodesSearched() + 1);

	if (INCHECK) {
		ss->staticEval = ScoreNone;
		bestScore = futilityBase = -ScoreInfinite;
	}
	else {
		if (!(move = pos.mateMoveIn1Ply()).isNone()) {
			return mateIn(ss->ply);
		}

		if (tte != nullptr) {
			if ((ss->staticEval = bestScore = tte->evalScore()) == ScoreNone) {
				ss->staticEval = bestScore = evaluate(pos, ss);
			}
		}
		else {
			ss->staticEval = bestScore = evaluate(pos, ss);
		}

		if (beta <= bestScore) {
			if (tte == nullptr) {
				tt.store(pos.getKey(), scoreToTT(bestScore, ss->ply), BoundLower,
						 DepthNone, Move::moveNone(), ss->staticEval);
			}

			return bestScore;
		}

		if (PVNode && alpha < bestScore) {
			alpha = bestScore;
		}

		futilityBase = bestScore + 128; // todo: 128 より大きくて良いと思う。
	}

	evaluate(pos, ss);

	MovePicker mp(pos, ttMove, depth, history, (ss-1)->currentMove.to());
	const CheckInfo ci(pos);

	while (!(move = mp.nextMove<false>()).isNone())
	{
		assert(pos.isOK());

		givesCheck = pos.moveGivesCheck(move, ci);

		// futility pruning
		if (!PVNode
			&& !INCHECK // 駒打ちは王手回避のみなので、ここで弾かれる。
			&& !givesCheck
			&& move != ttMove)
		{
			futilityScore =
				futilityBase + Position::capturePieceScore(pos.piece(move.to()));
			if (move.isPromotion()) {
				futilityScore += Position::promotePieceScore(move.pieceTypeFrom());
			}

			if (futilityScore < beta) {
				bestScore = std::max(bestScore, futilityScore);
				continue;
			}

			// todo: MovePicker のオーダリングで SEE してるので、ここで SEE するの勿体無い。
			if (futilityBase < beta
				&& depth < Depth0
				&& pos.see(move, beta - futilityBase) <= ScoreZero)
			{
				bestScore = std::max(bestScore, futilityBase);
				continue;
			}
		}

		evasionPrunable = (INCHECK
						   && ScoreMatedInMaxPly < bestScore
						   && !move.isCapture());

		if (!PVNode
			&& (!INCHECK || evasionPrunable)
			&& move != ttMove
			&& (!move.isPromotion() || ((INCHECK ? move.pieceTypeFromOrDropped() : move.pieceTypeFrom()) == Silver))
			&& pos.seeSign(move) < 0)
		{
			continue;
		}

		if (!pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned)) {
			continue;
		}

		ss->currentMove = move;

		pos.doMove(move, st, ci, givesCheck);
		(ss+1)->staticEvalRaw = static_cast<Score>(INT_MAX);
		score = (givesCheck ? -qsearch<NT, true>(pos, ss+1, -beta, -alpha, depth - OnePly)
				 : -qsearch<NT, false>(pos, ss+1, -beta, -alpha, depth - OnePly));
		pos.undoMove(move);

		assert(-ScoreInfinite < score && score < ScoreInfinite);

		if (bestScore < score) {
			bestScore = score;

			if (alpha < score) {
				if (PVNode && score < beta) {
					alpha = score;
					bestMove = move;
				}
				else {
					// fail high
					tt.store(posKey, scoreToTT(score, ss->ply), BoundLower,
							 ttDepth, move, ss->staticEval);
					return score;
				}
			}
		}
	}

	if (INCHECK && bestScore == -ScoreInfinite) {
		return matedIn(ss->ply);
	}

	tt.store(posKey, scoreToTT(bestScore, ss->ply), 
			 ((PVNode && oldAlpha < bestScore) ? BoundExact : BoundUpper),
			 ttDepth, bestMove, ss->staticEval);

	assert(-ScoreInfinite < bestScore && bestScore < ScoreInfinite);

	return bestScore;
}

// iterative deepening loop
void Searcher::idLoop(Position& pos) {
	SearchStack ss[MaxPlyPlus2];
	Ply depth;
	Ply prevBestMoveChanges;
	Score bestScore = -ScoreInfinite;
	Score delta = -ScoreInfinite;
	Score alpha = -ScoreInfinite;
	Score beta = ScoreInfinite;
	bool bestMoveNeverChanged = true;
	int lastInfoTime = -1; // 将棋所のコンソールが詰まる問題への対処用

	memset(ss, 0, 4 * sizeof(SearchStack));
	bestMoveChanges = 0;
#if defined LEARN
	// 高速化の為に浅い探索は反復深化しないようにする。実戦時ではほぼ影響無い。学習時は浅い探索をひたすら繰り返す為。
	depth = std::max<Ply>(0, limits.depth - 1);
#else
	depth = 0;
#endif

	ss[0].currentMove = Move::moveNull(); // skip update gains
	tt.newSearch();
	history.clear();
	gains.clear();

	pvSize = options["MultiPV"];
	Skill skill(options["Skill_Level"], options["Max_Random_Score_Diff"], thisptr);

	if (options["Max_Random_Score_Diff_Ply"] < pos.gamePly()) {
		skill.max_random_score_diff = ScoreZero;
		pvSize = 1;
		assert(!skill.enabled()); // level による設定が出来るようになるまでは、これで良い。
	}

	if (skill.enabled() && pvSize < 3) {
		pvSize = 3;
	}
	pvSize = std::min(pvSize, rootMoves.size());

	// 指し手が無ければ負け
	if (rootMoves.empty()) {
		rootMoves.push_back(RootMove(Move::moveNone()));
		SYNCCOUT << "info depth 0 score "
				 << scoreToUSI(-ScoreMate0Ply)
				 << SYNCENDL;

		return;
	}

	// 反復深化で探索を行う。
	while (++depth <= MaxPly && !signals.stop && (!limits.depth || depth <= limits.depth)) {
		// 前回の iteration の結果を全てコピー
		for (size_t i = 0; i < rootMoves.size(); ++i) {
			rootMoves[i].prevScore_ = rootMoves[i].score_;
		}

		prevBestMoveChanges = bestMoveChanges;
		bestMoveChanges = 0;

		// Multi PV loop
		for (pvIdx = 0; pvIdx < pvSize && !signals.stop; ++pvIdx) {
			// aspiration search
			// alpha, beta をある程度絞ることで、探索効率を上げる。
			if (5 <= depth && abs(rootMoves[pvIdx].prevScore_) < ScoreKnownWin) {
				delta = static_cast<Score>(16);
				alpha = rootMoves[pvIdx].prevScore_ - delta;
				beta  = rootMoves[pvIdx].prevScore_ + delta;
			}
			else {
				alpha = -ScoreInfinite;
				beta  =  ScoreInfinite;
			}

			// aspiration search の window 幅を、初めは小さい値にして探索し、
			// fail high/low になったなら、今度は window 幅を広げて、再探索を行う。
			while (true) {
				// 探索を行う。
				ss->staticEvalRaw = static_cast<Score>(INT_MAX);
				(ss+1)->staticEvalRaw = static_cast<Score>(INT_MAX);
				bestScore = search<Root>(pos, ss + 1, alpha, beta, static_cast<Depth>(depth * OnePly), false);
				// 先頭が最善手になるようにソート
				insertionSort(rootMoves.begin() + pvIdx, rootMoves.end());

				for (size_t i = 0; i <= pvIdx; ++i) {
					ss->staticEvalRaw = static_cast<Score>(INT_MAX);
					(ss+1)->staticEvalRaw = static_cast<Score>(INT_MAX);
					rootMoves[i].insertPvInTT(pos, ss + 1);
				}

#if 0
				// 詰みを発見したら即指す。
				if (ScoreMateInMaxPly <= abs(bestScore) && abs(bestScore) < ScoreInfinite) {
					SYNCCOUT << pvInfoToUSI(pos, ply, alpha, beta) << SYNCENDL;
					signals.stop = true;
				}
#endif

				if (signals.stop) {
					break;
				}

				if (alpha < bestScore && bestScore < beta) {
					break;
				}

				if (3000 < searchTimer.elapsed()
					// 将棋所のコンソールが詰まるのを防ぐ。
					&& (depth < 10 || lastInfoTime + 200 < searchTimer.elapsed()))
				{
					lastInfoTime = searchTimer.elapsed();
					SYNCCOUT << pvInfoToUSI(pos, depth, alpha, beta) << SYNCENDL;
				}

				// fail high/low のとき、aspiration window を広げる。
				if (ScoreKnownWin <= abs(bestScore)) {
					// 勝ち(負け)だと判定したら、最大の幅で探索を試してみる。
					alpha = -ScoreInfinite;
					beta = ScoreInfinite;
				}
				else if (beta <= bestScore) {
					beta += delta;
					delta += delta / 2;
				}
				else {
					signals.failedLowAtRoot = true;
					signals.stopOnPonderHit = false;

					alpha -= delta;
					delta += delta / 2;
				}

				assert(-ScoreInfinite <= alpha && beta <= ScoreInfinite);
			}

			insertionSort(rootMoves.begin(), rootMoves.begin() + pvIdx + 1);
			if ((pvIdx + 1 == pvSize || 3000 < searchTimer.elapsed())
				// 将棋所のコンソールが詰まるのを防ぐ。
				&& (depth < 10 || lastInfoTime + 200 < searchTimer.elapsed()))
			{
				lastInfoTime = searchTimer.elapsed();
				SYNCCOUT << pvInfoToUSI(pos, depth, alpha, beta) << SYNCENDL;
			}
		}

		//if (skill.enabled() && skill.timeToPick(depth)) {
		//	skill.pickMove();
		//}

#if 0
		if (options["Use_Search_Log"]) {
			pvInfoToLog(pos, depth, bestScore, searchTimer.elapsed(), &rootMoves[0].pv_[0]);
		}
#endif

		if (limits.useTimeManagement() && !signals.stopOnPonderHit) {
			bool stop = false;

			if (4 < depth && depth < 50 && pvSize == 1) {
				timeManager.pvInstability(bestMoveChanges, prevBestMoveChanges);
			}

			// 次のイテレーションを回す時間が無いなら、ストップ
			if ((timeManager.availableTime() * 62) / 100 < searchTimer.elapsed()) {
				stop = true;
			}

			if (2 < depth && bestMoveChanges)
				bestMoveNeverChanged = false;

			// 最善手が、ある程度の深さまで同じであれば、
			// その手が突出して良い手なのだろう。
			if (12 <= depth
				&& !stop
				&& bestMoveNeverChanged
				&& pvSize == 1
				// ここは確実にバグらせないようにする。
				&& -ScoreInfinite + 2 * CapturePawnScore <= bestScore
				&& (rootMoves.size() == 1
					|| timeManager.availableTime() * 40 / 100 < searchTimer.elapsed()))
			{
				const Score rBeta = bestScore - 2 * CapturePawnScore;
				(ss+1)->staticEvalRaw = static_cast<Score>(INT_MAX);
				(ss+1)->excludedMove = rootMoves[0].pv_[0];
				(ss+1)->skipNullMove = true;
				const Score s = search<NonPV>(pos, ss+1, rBeta-1, rBeta, (depth - 3) * OnePly, true);
				(ss+1)->skipNullMove = false;
				(ss+1)->excludedMove = Move::moveNone();

				if (s < rBeta) {
					stop = true;
				}
			}

			if (stop) {
				if (limits.ponder) {
					signals.stopOnPonderHit = true;
				}
				else {
					signals.stop = true;
				}
			}
		}
	}
	skill.swapIfEnabled(thisptr);
	SYNCCOUT << pvInfoToUSI(pos, depth-1, alpha, beta) << SYNCENDL;
}

#if defined INANIWA_SHIFT
// 稲庭判定
void Searcher::detectInaniwa(const Position& pos) {
	if (inaniwaFlag == NotInaniwa && 20 <= pos.gamePly()) {
		const Rank TRank7 = (pos.turn() == Black ? Rank7 : Rank3); // not constant
		const Bitboard mask = rankMask(TRank7) & ~fileMask<FileA>() & ~fileMask<FileI>();
		if ((pos.bbOf(Pawn, oppositeColor(pos.turn())) & mask) == mask) {
			inaniwaFlag = (pos.turn() == Black ? InaniwaIsWhite : InaniwaIsBlack);
			tt.clear();
		}
	}
}
#endif
template <bool DO> void Position::doNullMove(StateInfo& backUpSt) {
	assert(!inCheck());

	StateInfo* src = (DO ? st_ : &backUpSt);
	StateInfo* dst = (DO ? &backUpSt : st_);

	dst->boardKey      = src->boardKey;
	dst->handKey       = src->handKey;
	dst->pliesFromNull = src->pliesFromNull;
	dst->hand = hand(turn());
	turn_ = oppositeColor(turn());

	if (DO) {
		st_->boardKey ^= zobTurn();
		prefetch(csearcher()->tt.firstEntry(st_->key()));
		st_->pliesFromNull = 0;
		st_->continuousCheck[turn()] = 0;
	}
	st_->hand = hand(turn());

	assert(isOK());
}

template <NodeType NT>
Score Searcher::search(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth, const bool cutNode) {
	const bool PVNode = (NT == PV || NT == Root || NT == SplitPointPV || NT == SplitPointRoot);
	const bool SPNode = (NT == SplitPointPV || NT == SplitPointNonPV || NT == SplitPointRoot);
	const bool RootNode = (NT == Root || NT == SplitPointRoot);

	assert(-ScoreInfinite <= alpha && alpha < beta && beta <= ScoreInfinite);
	assert(PVNode || (alpha == beta - 1));
	assert(Depth0 < depth);

	// 途中で goto を使用している為、先に全部の変数を定義しておいた方が安全。
	Move movesSearched[64];
	StateInfo st;
	const TTEntry* tte;
	SplitPoint* splitPoint;
	Key posKey;
	Move ttMove;
	Move move;
	Move excludedMove;
	Move bestMove;
	Move threatMove;
	Depth newDepth;
	Depth extension;
	Score bestScore;
	Score score;
	Score ttScore;
	Score eval;
	bool inCheck;
	bool givesCheck;
	bool isPVMove;
	bool singularExtensionNode;
	bool captureOrPromotion;
	bool dangerous;
	bool doFullDepthSearch;
	int moveCount;
	int playedMoveCount;

	// step1
	// initialize node
	Thread* thisThread = pos.thisThread();
	moveCount = playedMoveCount = 0;
	inCheck = pos.inCheck();

	if (SPNode) {
		splitPoint = ss->splitPoint;
		bestMove = splitPoint->bestMove;
		threatMove = splitPoint->threatMove;
		bestScore = splitPoint->bestScore;
		tte = nullptr;
		ttMove = excludedMove = Move::moveNone();
		ttScore = ScoreNone;

		evaluate(pos, ss);

		assert(-ScoreInfinite < splitPoint->bestScore && 0 < splitPoint->moveCount);

		goto split_point_start;
	}

	bestScore = -ScoreInfinite;
	ss->currentMove = threatMove = bestMove = (ss + 1)->excludedMove = Move::moveNone();
	ss->ply = (ss-1)->ply + 1;
	(ss+1)->skipNullMove = false;
	(ss+1)->reduction = Depth0;
	(ss+2)->killers[0] = (ss+2)->killers[1] = Move::moveNone();

	if (PVNode && thisThread->maxPly < ss->ply) {
		thisThread->maxPly = ss->ply;
	}

	if (!RootNode) {
		// step2
		// stop と最大探索深さのチェック
		switch (pos.isDraw(16)) {
		case NotRepetition      : if (!signals.stop && ss->ply <= MaxPly) { break; }
		case RepetitionDraw     : return ScoreDraw;
		case RepetitionWin      : return mateIn(ss->ply);
		case RepetitionLose     : return matedIn(ss->ply);
		case RepetitionSuperior : if (ss->ply != 2) { return ScoreMateInMaxPly; } break;
		case RepetitionInferior : if (ss->ply != 2) { return ScoreMatedInMaxPly; } break;
		default                 : UNREACHABLE;
		}

		// step3
		// mate distance pruning
		if (!RootNode) {
			alpha = std::max(matedIn(ss->ply), alpha);
			beta = std::min(mateIn(ss->ply+1), beta);
			if (beta <= alpha) {
				return alpha;
			}
		}
	}

	pos.setNodesSearched(pos.nodesSearched() + 1);

	// step4
	// trans position table lookup
	excludedMove = ss->excludedMove;
	posKey = (excludedMove.isNone() ? pos.getKey() : pos.getExclusionKey());
	tte = tt.probe(posKey);
	ttMove = 
		RootNode ? rootMoves[pvIdx].pv_[0] :
		tte != nullptr ?
		move16toMove(tte->move(), pos) :
		Move::moveNone();
	ttScore = (tte != nullptr ? scoreFromTT(tte->score(), ss->ply) : ScoreNone);

	if (!RootNode
		&& tte != nullptr
		&& depth <= tte->depth()
		&& ttScore != ScoreNone // アクセス競合が起きたときのみ、ここに引っかかる。
		&& (PVNode ? tte->type() == BoundExact
			: (beta <= ttScore ? (tte->type() & BoundLower)
			   : (tte->type() & BoundUpper))))
	{
		tt.refresh(tte);
		ss->currentMove = ttMove; // Move::moveNone() もありえる。

		if (beta <= ttScore
			&& !ttMove.isNone()
			&& !ttMove.isCaptureOrPromotion()
			&& ttMove != ss->killers[0])
		{
			ss->killers[1] = ss->killers[0];
			ss->killers[0] = ttMove;
		}
		return ttScore;
	}

#if 1
	if (!RootNode
		&& !inCheck)
	{
		if (!(move = pos.mateMoveIn1Ply()).isNone()) {
			ss->staticEval = bestScore = mateIn(ss->ply);
			tt.store(posKey, scoreToTT(bestScore, ss->ply), BoundExact, depth,
					 move, ss->staticEval);
			bestMove = move;
			return bestScore;
		}
	}
#endif

	// step5
	// evaluate the position statically
	eval = ss->staticEval = evaluate(pos, ss); // Bonanza の差分評価の為、evaluate() を常に呼ぶ。
	if (inCheck) {
		eval = ss->staticEval = ScoreNone;
		goto iid_start;
	}
	else if (tte != nullptr) {
		if (ttScore != ScoreNone
			&& (tte->type() & (eval < ttScore ? BoundLower : BoundUpper)))
		{
			eval = ttScore;
		}
	}
	else {
		tt.store(posKey, ScoreNone, BoundNone, DepthNone,
				 Move::moveNone(), ss->staticEval);
	}

	// 一手前の指し手について、history を更新する。
	// todo: ここの条件はもう少し考えた方が良い。
	if ((move = (ss-1)->currentMove) != Move::moveNull()
		&& (ss-1)->staticEval != ScoreNone
		&& ss->staticEval != ScoreNone
		&& !move.isCapture() // 前回(一手前)の指し手が駒取りでなかった。
		)
	{
		const Square to = move.to();
		gains.update(move.isDrop(), pos.piece(to), to, -(ss-1)->staticEval - ss->staticEval);
	}

	// step6
	// razoring
	if (!PVNode
		&& depth < 4 * OnePly
		&& eval + razorMargin(depth) < beta
		&& ttMove.isNone()
		&& abs(beta) < ScoreMateInMaxPly)
	{
		const Score rbeta = beta - razorMargin(depth);
		const Score s = qsearch<NonPV, false>(pos, ss, rbeta-1, rbeta, Depth0);
		if (s < rbeta) {
			return s;
		}
	}

	// step7
	// static null move pruning
	if (!PVNode
		&& !ss->skipNullMove
		&& depth < 4 * OnePly
		&& beta <= eval - FutilityMargins[depth][0]
		&& abs(beta) < ScoreMateInMaxPly)
	{
		return eval - FutilityMargins[depth][0];
	}

	// step8
	// null move
	if (!PVNode
		&& !ss->skipNullMove
		&& 2 * OnePly <= depth
		&& beta <= eval
		&& abs(beta) < ScoreMateInMaxPly)
	{
		ss->currentMove = Move::moveNull();
		Depth reduction = static_cast<Depth>(3) * OnePly + depth / 4;

		if (beta < eval - PawnScore) {
			reduction += OnePly;
		}

		pos.doNullMove<true>(st);
		(ss+1)->staticEvalRaw = (ss)->staticEvalRaw; // 評価値の差分評価の為。
		(ss+1)->skipNullMove = true;
		Score nullScore = (depth - reduction < OnePly ?
						   -qsearch<NonPV, false>(pos, ss + 1, -beta, -alpha, Depth0)
						   : -search<NonPV>(pos, ss + 1, -beta, -alpha, depth - reduction, !cutNode));
		(ss+1)->skipNullMove = false;
		pos.doNullMove<false>(st);

		if (beta <= nullScore) {
			if (ScoreMateInMaxPly <= nullScore) {
				nullScore = beta;
			}

			if (depth < 6 * OnePly) {
				return nullScore;
			}

			ss->skipNullMove = true;
			assert(Depth0 < depth - reduction);
			const Score s = search<NonPV>(pos, ss, alpha, beta, depth - reduction, false);
			ss->skipNullMove = false;

			if (beta <= s) {
				return nullScore;
			}
		}
		else {
			// fail low
			threatMove = (ss+1)->currentMove;
			if (depth < 5 * OnePly
				&& (ss-1)->reduction != Depth0
				&& !threatMove.isNone()
				&& allows(pos, (ss-1)->currentMove, threatMove))
			{
				return beta - 1;
			}
		}
	}

	// step9
	// probcut
	if (!PVNode
		&& 5 * OnePly <= depth
		&& !ss->skipNullMove
		// 確実にバグらせないようにする。
		&& abs(beta) < ScoreInfinite - 200)
	{
		const Score rbeta = beta + 200;
		const Depth rdepth = depth - OnePly - 3 * OnePly;

		assert(OnePly <= rdepth);
		assert(!(ss-1)->currentMove.isNone());
		assert((ss-1)->currentMove != Move::moveNull());

		assert(move == (ss-1)->currentMove);
		// move.cap() は前回(一手前)の指し手で取った駒の種類
		MovePicker mp(pos, ttMove, history, move.cap());
		const CheckInfo ci(pos);
		while (!(move = mp.nextMove<false>()).isNone()) {
			if (pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned)) {
				ss->currentMove = move;
				pos.doMove(move, st, ci, pos.moveGivesCheck(move, ci));
				(ss+1)->staticEvalRaw = static_cast<Score>(INT_MAX);
				score = -search<NonPV>(pos, ss+1, -rbeta, -rbeta+1, rdepth, !cutNode);
				pos.undoMove(move);
				if (rbeta <= score) {
					return score;
				}
			}
		}
	}

iid_start:
	// step10
	// internal iterative deepening
	if ((PVNode ? 5 * OnePly : 8 * OnePly) <= depth
		&& ttMove.isNone()
		&& (PVNode || (!inCheck && beta <= ss->staticEval + static_cast<Score>(256))))
	{
		//const Depth d = depth - 2 * OnePly - (PVNode ? Depth0 : depth / 4);
		const Depth d = (PVNode ? depth - 2 * OnePly : depth / 2);

		ss->skipNullMove = true;
		search<PVNode ? PV : NonPV>(pos, ss, alpha, beta, d, true);
		ss->skipNullMove = false;

		tte = tt.probe(posKey);
		ttMove = (tte != nullptr ?
				  move16toMove(tte->move(), pos) :
				  Move::moveNone());
	}

split_point_start:
	MovePicker mp(pos, ttMove, depth, history, ss, PVNode ? -ScoreInfinite : beta);
	const CheckInfo ci(pos);
	score = bestScore;
	singularExtensionNode =
		!RootNode
		&& !SPNode
		&& 8 * OnePly <= depth
		&& !ttMove.isNone()
		&& excludedMove.isNone()
		&& (tte->type() & BoundLower)
		&& depth - 3 * OnePly <= tte->depth();

	// step11
	// Loop through moves
	while (!(move = mp.nextMove<SPNode>()).isNone()) {
		if (move == excludedMove) {
			continue;
		}

		if (RootNode
			&& std::find(rootMoves.begin() + pvIdx,
						 rootMoves.end(),
						 move) == rootMoves.end())
		{
			continue;
		}

		if (SPNode) {
			if (!pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned)) {
				continue;
			}
			moveCount = ++splitPoint->moveCount;
			splitPoint->mutex.unlock();
		}
		else {
			++moveCount;
		}


		if (RootNode) {
			signals.firstRootMove = (moveCount == 1);
#if 0
			if (thisThread == threads.mainThread() && 3000 < searchTimer.elapsed()) {
				SYNCCOUT << "info depth " << depth / OnePly
						 << " currmove " << move.toUSI()
						 << " currmovenumber " << moveCount + pvIdx << SYNCENDL;
			}
#endif
		}

		extension = Depth0;
		captureOrPromotion = move.isCaptureOrPromotion();
		givesCheck = pos.moveGivesCheck(move, ci);
		dangerous = givesCheck; // todo: not implement

		// step12
		if (givesCheck && ScoreZero <= pos.seeSign(move))
		{
			extension = OnePly;
		}

		// singuler extension
		if (singularExtensionNode
			&& extension == Depth0
			&& move == ttMove
			&& pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned)
			&& abs(ttScore) < ScoreKnownWin)
		{
			assert(ttScore != ScoreNone);

			const Score rBeta = ttScore - static_cast<Score>(depth);
			ss->excludedMove = move;
			ss->skipNullMove = true;
			score = search<NonPV>(pos, ss, rBeta - 1, rBeta, depth / 2, cutNode);
			ss->skipNullMove = false;
			ss->excludedMove = Move::moveNone();

			if (score < rBeta) {
				//extension = OnePly;
				extension = (beta <= rBeta ? OnePly + OnePly / 2 : OnePly);
			}
		}

		newDepth = depth - OnePly + extension;

		// step13
		// futility pruning
		if (!PVNode
			&& !captureOrPromotion
			&& !inCheck
			&& !dangerous
			//&& move != ttMove // 次の行がtrueならこれもtrueなので条件から省く。
			&& ScoreMatedInMaxPly < bestScore)
		{
			assert(move != ttMove);
			// move count based pruning
			if (depth < 16 * OnePly
				&& FutilityMoveCounts[depth] <= moveCount
				&& (threatMove.isNone() || !refutes(pos, move, threatMove)))
			{
				if (SPNode) {
					splitPoint->mutex.lock();
				}
				continue;
			}

			// score based pruning
			const Depth predictedDepth = newDepth - reduction<PVNode>(depth, moveCount);
			// gain を 2倍にする。
			const Score futilityScore = ss->staticEval + futilityMargin(predictedDepth, moveCount)
				+ 2 * gains.value(move.isDrop(), colorAndPieceTypeToPiece(pos.turn(), move.pieceTypeFromOrDropped()), move.to());

			if (futilityScore < beta) {
				bestScore = std::max(bestScore, futilityScore);
				if (SPNode) {
					splitPoint->mutex.lock();
					if (splitPoint->bestScore < bestScore) {
						splitPoint->bestScore = bestScore;
					}
				}
				continue;
			}

			if (predictedDepth < 4 * OnePly
				&& pos.seeSign(move) < ScoreZero)
			{
				if (SPNode) {
					splitPoint->mutex.lock();
				}
				continue;
			}
		}

		// RootNode, SPNode はすでに合法手であることを確認済み。
		if (!RootNode && !SPNode && !pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned)) {
			--moveCount;
			continue;
		}

		isPVMove = (PVNode && moveCount == 1);
		ss->currentMove = move;
		if (!SPNode && !captureOrPromotion && playedMoveCount < 64) {
			movesSearched[playedMoveCount++] = move;
		}

		// step14
		pos.doMove(move, st, ci, givesCheck);
		(ss+1)->staticEvalRaw = static_cast<Score>(INT_MAX);

		// step15
		// LMR
		if (3 * OnePly <= depth
			&& !isPVMove
			&& !captureOrPromotion
			&& move != ttMove
			&& ss->killers[0] != move
			&& ss->killers[1] != move)
		{
			ss->reduction = reduction<PVNode>(depth, moveCount);
			if (!PVNode && cutNode) {
				ss->reduction += OnePly;
			}
			const Depth d = std::max(newDepth - ss->reduction, OnePly);
			if (SPNode) {
				alpha = splitPoint->alpha;
			}
			// PVS
			score = -search<NonPV>(pos, ss+1, -(alpha + 1), -alpha, d, true);

			doFullDepthSearch = (alpha < score && ss->reduction != Depth0);
			ss->reduction = Depth0;
		}
		else {
			doFullDepthSearch = !isPVMove;
		}

		// step16
		// full depth search
		// PVS
		if (doFullDepthSearch) {
			if (SPNode) {
				alpha = splitPoint->alpha;
			}
			score = (newDepth < OnePly ?
					 (givesCheck ? -qsearch<NonPV, true>(pos, ss+1, -(alpha + 1), -alpha, Depth0)
					  : -qsearch<NonPV, false>(pos, ss+1, -(alpha + 1), -alpha, Depth0))
					 : -search<NonPV>(pos, ss+1, -(alpha + 1), -alpha, newDepth, !cutNode));
		}

		// 通常の探索
		if (PVNode && (isPVMove || (alpha < score && (RootNode || score < beta)))) {
			score = (newDepth < OnePly ?
					 (givesCheck ? -qsearch<PV, true>(pos, ss+1, -beta, -alpha, Depth0)
					  : -qsearch<PV, false>(pos, ss+1, -beta, -alpha, Depth0))
					 : -search<PV>(pos, ss+1, -beta, -alpha, newDepth, false));
		}

		// step17
		pos.undoMove(move);

		assert(-ScoreInfinite < score && score < ScoreInfinite);

		// step18
		if (SPNode) {
			splitPoint->mutex.lock();
			bestScore = splitPoint->bestScore;
			alpha = splitPoint->alpha;
		}

		if (signals.stop || thisThread->cutoffOccurred()) {
			return score;
		}

		if (RootNode) {
			RootMove& rm = *std::find(rootMoves.begin(), rootMoves.end(), move);
			if (isPVMove || alpha < score) {
				// PV move or new best move
				rm.score_ = score;
				rm.extractPvFromTT(pos);

				if (!isPVMove) {
					++bestMoveChanges;
				}
			}
			else {
				rm.score_ = -ScoreInfinite;
			}
		}

		if (bestScore < score) {
			bestScore = (SPNode ? splitPoint->bestScore = score : score);

			if (alpha < score) {
				bestMove = (SPNode ? splitPoint->bestMove = move : move);

				if (PVNode && score < beta) {
					alpha = (SPNode ? splitPoint->alpha = score : score);
				}
				else {
					// fail high
					if (SPNode) {
						splitPoint->cutoff = true;
					}
					break;
				}
			}
		}

		// step19
		if (!SPNode
			&& threads.minSplitDepth() <= depth
			&& threads.availableSlave(thisThread)
			&& thisThread->splitPointsSize < MaxSplitPointsPerThread)
		{
			assert(bestScore < beta);
			thisThread->split<FakeSplit>(pos, ss, alpha, beta, bestScore, bestMove,
										 depth, threatMove, moveCount, mp, NT, cutNode);
			if (beta <= bestScore) {
				break;
			}
		}
	}

	if (SPNode) {
		return bestScore;
	}

	// step20
	if (moveCount == 0) {
		return !excludedMove.isNone() ? alpha : matedIn(ss->ply);
	}

	if (bestScore == -ScoreInfinite) {
		assert(playedMoveCount == 0);
		bestScore = alpha;
	}

	if (beta <= bestScore) {
		// failed high
		tt.store(posKey, scoreToTT(bestScore, ss->ply), BoundLower, depth,
				 bestMove, ss->staticEval);

		if (!bestMove.isCaptureOrPromotion() && !inCheck) {
			if (bestMove != ss->killers[0]) {
				ss->killers[1] = ss->killers[0];
				ss->killers[0] = bestMove;
			}

			const Score bonus = static_cast<Score>(depth * depth);
			const Piece pc1 = colorAndPieceTypeToPiece(pos.turn(), bestMove.pieceTypeFromOrDropped());
			history.update(bestMove.isDrop(), pc1, bestMove.to(), bonus);

			for (int i = 0; i < playedMoveCount - 1; ++i) {
				const Move m = movesSearched[i];
				const Piece pc2 = colorAndPieceTypeToPiece(pos.turn(), m.pieceTypeFromOrDropped());
				history.update(m.isDrop(), pc2, m.to(), -bonus);
			}
		}
	}
	else {
		// failed low or PV search
		tt.store(posKey, scoreToTT(bestScore, ss->ply),
				 ((PVNode && !bestMove.isNone()) ? BoundExact : BoundUpper),
				 depth, bestMove, ss->staticEval);
	}

	assert(-ScoreInfinite < bestScore && bestScore < ScoreInfinite);

	return bestScore;
}

void RootMove::extractPvFromTT(Position& pos) {
	StateInfo state[MaxPlyPlus2];
	StateInfo* st = state;
	TTEntry* tte;
	Ply ply = 0;
	Move m = pv_[0];

	assert(!m.isNone() && pos.moveIsPseudoLegal(m));

	pv_.clear();

	do {
		pv_.push_back(m);

		assert(pos.moveIsLegal(pv_[ply]));
		pos.doMove(pv_[ply++], *st++);
		tte = pos.csearcher()->tt.probe(pos.getKey());
	}
	while (tte != nullptr
		   // このチェックは少し無駄。駒打ちのときはmove16toMove() 呼ばなくて良い。
		   && pos.moveIsPseudoLegal(m = move16toMove(tte->move(), pos))
		   && pos.pseudoLegalMoveIsLegal<false, false>(m, pos.pinnedBB())
		   && ply < MaxPly
		   && (!pos.isDraw(20) || ply < 6));

	pv_.push_back(Move::moveNone());
	while (ply) {
		pos.undoMove(pv_[--ply]);
	}
}

void RootMove::insertPvInTT(Position& pos, SearchStack* ss) {
	StateInfo state[MaxPlyPlus2];
	StateInfo* st = state;
	TTEntry* tte;
	Ply ply = 0;

	do {
		tte = pos.csearcher()->tt.probe(pos.getKey());

		if (tte == nullptr
			|| move16toMove(tte->move(), pos) != pv_[ply])
		{
			pos.searcher()->tt.store(pos.getKey(), ScoreNone, BoundNone, DepthNone, pv_[ply], ScoreNone);
		}

		assert(pos.moveIsLegal(pv_[ply]));
		pos.doMove(pv_[ply++], *st++);
	} while (!pv_[ply].isNone());

	while (ply) {
		pos.undoMove(pv_[--ply]);
	}
}

void initSearchTable() {
	// todo: パラメータは改善の余地あり。
	int d;  // depth (ONE_PLY == 2)
	int hd; // half depth (ONE_PLY == 1)
	int mc; // moveCount

	// Init reductions array
	for (hd = 1; hd < 64; hd++) {
		for (mc = 1; mc < 64; mc++) {
			double    pvRed = log(double(hd)) * log(double(mc)) / 3.0;
			double nonPVRed = 0.33 + log(double(hd)) * log(double(mc)) / 2.25;
			Reductions[1][hd][mc] = (int8_t) (   pvRed >= 1.0 ? floor(   pvRed * int(OnePly)) : 0);
			Reductions[0][hd][mc] = (int8_t) (nonPVRed >= 1.0 ? floor(nonPVRed * int(OnePly)) : 0);
		}
	}

	for (d = 1; d < 16; ++d) {
		for (mc = 0; mc < 64; ++mc) {
			FutilityMargins[d][mc] = static_cast<Score>(112 * static_cast<int>(log(static_cast<double>(d*d)/2) / log(2.0) + 1.001)
														- 8 * mc + 45);
		}
	}

	// init futility move counts
	for (d = 0; d < 32; ++d) {
		FutilityMoveCounts[d] = static_cast<int>(3.001 + 0.3 * pow(static_cast<double>(d), 1.8));
	}
}

// 入玉勝ちかどうかを判定
bool nyugyoku(const Position& pos) {
	// CSA ルールでは、一 から 六 の条件を全て満たすとき、入玉勝ち宣言が出来る。

	// 一 宣言側の手番である。

	// この関数を呼び出すのは自分の手番のみとする。ponder では呼び出さない。

	const Color us = pos.turn();
	// 敵陣のマスク
	const Bitboard opponentsField = (us == Black ? inFrontMask<Black, Rank6>() : inFrontMask<White, Rank4>());

	// 二 宣言側の玉が敵陣三段目以内に入っている。
	if (!pos.bbOf(King, us).andIsNot0(opponentsField))
		return false;

	// 三 宣言側が、大駒5点小駒1点で計算して
	//     先手の場合28点以上の持点がある。
	//     後手の場合27点以上の持点がある。
	//     点数の対象となるのは、宣言側の持駒と敵陣三段目以内に存在する玉を除く宣言側の駒のみである。
	const Bitboard bigBB = pos.bbOf(Rook, Dragon, Bishop, Horse) & opponentsField & pos.bbOf(us);
	const Bitboard smallBB = (pos.bbOf(Pawn, Lance, Knight, Silver) | pos.goldsBB()) & opponentsField & pos.bbOf(us);
	const Hand hand = pos.hand(us);
	const int val = (bigBB.popCount() + hand.numOf<HRook>() + hand.numOf<HBishop>()) * 5
		+ smallBB.popCount()
		+ hand.numOf<HPawn>() + hand.numOf<HLance>() + hand.numOf<HKnight>()
		+ hand.numOf<HSilver>() + hand.numOf<HGold>();
#if defined LAW_24
	if (val < 31)
		return false;
#else
	if (val < (us == Black ? 28 : 27))
		return false;
#endif

	// 四 宣言側の敵陣三段目以内の駒は、玉を除いて10枚以上存在する。

	// 玉は敵陣にいるので、自駒が敵陣に11枚以上あればよい。
	if ((pos.bbOf(us) & opponentsField).popCount() < 11)
		return false;

	// 五 宣言側の玉に王手がかかっていない。
	if (pos.inCheck())
		return false;

	// 六 宣言側の持ち時間が残っている。

	// 持ち時間が無ければ既に負けなので、何もチェックしない。

	return true;
}

void Searcher::think() {
	static Book book;
	Position& pos = rootPosition;
	timeManager.init(limits, pos.gamePly(), pos.turn(), thisptr);
	std::uniform_int_distribution<int> dist(options["Min_Book_Ply"], options["Max_Book_Ply"]);
	const Ply book_ply = dist(g_randomTimeSeed);

	bool nyugyokuWin = false;
	if (nyugyoku(pos)) {
		nyugyokuWin = true;
		goto finalize;
	}
	pos.setNodesSearched(0);

	tt.setSize(options["USI_Hash"]); // operator int() 呼び出し。

	SYNCCOUT << "info string book_ply " << book_ply << SYNCENDL;
	if (options["OwnBook"] && pos.gamePly() <= book_ply) {
		const std::tuple<Move, Score> bookMoveScore = book.probe(pos, options["Book_File"], options["Best_Book_Move"]);
		if (!std::get<0>(bookMoveScore).isNone() && std::find(rootMoves.begin(),
															  rootMoves.end(),
															  std::get<0>(bookMoveScore)) != rootMoves.end())
		{
			std::swap(rootMoves[0], *std::find(rootMoves.begin(),
											   rootMoves.end(),
											   std::get<0>(bookMoveScore)));
			SYNCCOUT << "info"
					 << " score " << scoreToUSI(std::get<1>(bookMoveScore))
					 << " pv " << std::get<0>(bookMoveScore).toUSI()
					 << SYNCENDL;

			goto finalize;
		}
	}

	threads.wakeUp(thisptr);

	threads.timerThread()->msec =
		(limits.useTimeManagement() ? std::min(100, std::max(timeManager.availableTime() / 16, TimerResolution)) :
		 limits.nodes               ? 2 * TimerResolution :
		 100);
	threads.timerThread()->notifyOne();

#if defined INANIWA_SHIFT
	detectInaniwa(pos);
#endif
	idLoop(pos);

	threads.timerThread()->msec = 0; // timer を止める。
	threads.sleep();

finalize:

	SYNCCOUT << "info nodes " << pos.nodesSearched()
			 << " time " << searchTimer.elapsed() << SYNCENDL;

	if (!signals.stop && (limits.ponder || limits.infinite)) {
		signals.stopOnPonderHit = true;
		pos.thisThread()->waitFor(signals.stop);
	}

	if (nyugyokuWin) {
		SYNCCOUT << "bestmove win" << SYNCENDL;
	}
	else if (rootMoves[0].pv_[0].isNone()) {
		SYNCCOUT << "bestmove resign" << SYNCENDL;
	}
	else {
		SYNCCOUT << "bestmove " << rootMoves[0].pv_[0].toUSI()
				 << " ponder " << rootMoves[0].pv_[1].toUSI()
				 << SYNCENDL;
	}
}

void Searcher::checkTime() {
	if (limits.ponder)
		return;

	s64 nodes = 0;
	if (limits.nodes) {
		std::unique_lock<std::mutex> lock(threads.mutex_);

		nodes = rootPosition.nodesSearched();
		for (size_t i = 0; i < threads.size(); ++i) {
			for (int j = 0; j < threads[i]->splitPointsSize; ++j) {
				SplitPoint& splitPoint = threads[i]->splitPoints[j];
				std::unique_lock<std::mutex> spLock(splitPoint.mutex);
				nodes += splitPoint.nodes;
				u64 sm = splitPoint.slavesMask;
				while (sm) {
					const int index = firstOneFromLSB(sm);
					sm &= sm - 1;
					Position* pos = threads[index]->activePosition;
					if (pos != nullptr) {
						nodes += pos->nodesSearched();
					}
				}
			}
		}
	}

	const int e = searchTimer.elapsed();
	const bool stillAtFirstMove =
		signals.firstRootMove
		&& !signals.failedLowAtRoot
		&& timeManager.availableTime() < e;

	const bool noMoreTime =
		timeManager.maximumTime() - 2 * TimerResolution < e
		|| stillAtFirstMove;

	if ((limits.useTimeManagement() && noMoreTime)
		|| (limits.moveTime != 0 && limits.moveTime < e)
		|| (limits.nodes != 0 && nodes <= limits.nodes))
	{
		signals.stop = true;
	}
}

void Thread::idleLoop() {
	SplitPoint* thisSp = splitPointsSize ? activeSplitPoint : nullptr;
	assert(!thisSp || (thisSp->masterThread == this && searching));

	while (true) {
		while ((!searching && searcher->threads.sleepWhileIdle_) || exit)
		{
			if (exit) {
				assert(thisSp == nullptr);
				return;
			}

			std::unique_lock<std::mutex> lock(sleepLock);
			if (thisSp != nullptr && !thisSp->slavesMask) {
				break;
			}

			if (!searching && !exit) {
				sleepCond.wait(lock);
			}
		}

		if (searching) {
			assert(!exit);

			searcher->threads.mutex_.lock();
			assert(searching);
			SplitPoint* sp = activeSplitPoint;
			searcher->threads.mutex_.unlock();

			SearchStack ss[MaxPlyPlus2];
			Position pos(*sp->pos, this);

			memcpy(ss, sp->ss - 1, 4 * sizeof(SearchStack));
			(ss+1)->splitPoint = sp;

			sp->mutex.lock();

			assert(activePosition == nullptr);

			activePosition = &pos;

			switch (sp->nodeType) {
			case Root : searcher->search<SplitPointRoot >(pos, ss + 1, sp->alpha, sp->beta, sp->depth, sp->cutNode); break;
			case PV   : searcher->search<SplitPointPV   >(pos, ss + 1, sp->alpha, sp->beta, sp->depth, sp->cutNode); break;
			case NonPV: searcher->search<SplitPointNonPV>(pos, ss + 1, sp->alpha, sp->beta, sp->depth, sp->cutNode); break;
			default   : UNREACHABLE;
			}

			assert(searching);
			searching = false;
			activePosition = nullptr;
			assert(sp->slavesMask & (UINT64_C(1) << idx));
			sp->slavesMask ^= (UINT64_C(1) << idx);
			sp->nodes += pos.nodesSearched();

			if (searcher->threads.sleepWhileIdle_
				&& this != sp->masterThread
				&& !sp->slavesMask)
			{
				assert(!sp->masterThread->searching);
				sp->masterThread->notifyOne();
			}
			sp->mutex.unlock();
		}

		if (thisSp != nullptr && !thisSp->slavesMask) {
			thisSp->mutex.lock();
			const bool finished = !thisSp->slavesMask;
			thisSp->mutex.unlock();
			if (finished) {
				return;
			}
		}
	}
}
