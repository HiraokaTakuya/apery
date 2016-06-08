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

#if defined USE_GLOBAL
SignalsType Searcher::signals;
LimitsType Searcher::limits;
std::vector<Move> Searcher::searchMoves;
StateStackPtr Searcher::setUpStates;
StateStackPtr Searcher::usiSetUpStates;
TimeManager Searcher::timeManager;
TranspositionTable Searcher::tt;
#if defined INANIWA_SHIFT
InaniwaFlag Searcher::inaniwaFlag;
#endif
ThreadPool Searcher::threads;
OptionsMap Searcher::options;
EasyMoveManager Searcher::easyMove;
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

void Searcher::clear() {
	tt.clear();
	for (Thread* th : threads) {
		th->history.clear();
		th->gains.clear();
	}
	threads.mainThread()->previousScore = ScoreInfinite;
}

namespace {
	const int SkillLevel = 20; // [0, 20] 大きいほど強くする予定。現状 20 以外未対応。

	inline Score razorMargin(const Depth d) {
		return static_cast<Score>(512 + 16 * static_cast<int>(d));
	}

	inline Score futilityMargin(const Depth depth) {
		return static_cast<Score>(100 * depth);
	}

	int FutilityMoveCounts[32];    // [depth]

	s8 Reductions[2][2][64][64]; // [pv][improving][depth][moveNumber]
	template <bool PVNode> inline Depth reduction(const bool improving, const Depth depth, const int moveCount) {
		return static_cast<Depth>(Reductions[PVNode][improving][std::min(Depth(depth/OnePly), Depth(63))][std::min(moveCount, 63)]);
	}

	struct Skill {
		Skill(const int l, const int mr)
			: level(l),
			  max_random_score_diff(static_cast<Score>(mr)),
			  best(Move::moveNone()) {}
		~Skill() {}
		void swapIfEnabled(Thread* th, const size_t pvSize) {
			if (enabled()) {
				auto it = std::find(th->rootMoves.begin(),
									th->rootMoves.end(),
									(!best.isNone() ? best : pickMove(th, pvSize)));
				if (th->rootMoves.begin() != it)
					SYNCCOUT << "info string swap multipv 1, " << it - th->rootMoves.begin() + 1 << SYNCENDL;
				std::swap(th->rootMoves[0], *it);
			}
		}
		bool enabled() const { return level < 20 || max_random_score_diff != ScoreZero; }
		bool timeToPick(const int depth) const { return depth == 1 + level; }
		Move pickMove(const Thread* th, const size_t pvSize) {
			// level については未対応。max_random_score_diff についてのみ対応する。
			if (max_random_score_diff != ScoreZero) {
				size_t i = 1;
				for (; i < pvSize; ++i) {
					if (max_random_score_diff < th->rootMoves[0].score_ - th->rootMoves[i].score_)
						break;
				}
				// 0 から i-1 までの間でランダムに選ぶ。
				std::uniform_int_distribution<size_t> dist(0, i-1);
				best = th->rootMoves[dist(g_randomTimeSeed)].pv_[0];
				return best;
			}
			best = th->rootMoves[0].pv_[0];
			return best;
		}

		int level;
		Score max_random_score_diff;
		Move best;
	};

	using Row = std::vector<int>;

	const Row HalfDensity[] = {
		{0, 1},
		{1, 0},
		{0, 0, 1, 1},
		{0, 1, 1, 0},
		{1, 1, 0, 0},
		{1, 0, 0, 1},
		{0, 0, 0, 1, 1, 1},
		{0, 0, 1, 1, 1, 0},
		{0, 1, 1, 1, 0, 0},
		{1, 1, 1, 0, 0, 0},
		{1, 1, 0, 0, 0, 1},
		{1, 0, 0, 0, 1, 1},
		{0, 0, 0, 0, 1, 1, 1, 1},
		{0, 0, 0, 1, 1, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0 ,0},
		{0, 1, 1, 1, 1, 0, 0 ,0},
		{1, 1, 1, 1, 0, 0, 0 ,0},
		{1, 1, 1, 0, 0, 0, 0 ,1},
		{1, 1, 0, 0, 0, 0, 1 ,1},
		{1, 0, 0, 0, 0, 1, 1 ,1},
	};

	const size_t HalfDensitySize = std::extent<decltype(HalfDensity)>::value;

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
		if (m1to == m2from || m2to == m1from)
			return true;

		if (second.isDrop() && first.isDrop())
			return false;

		if (!second.isDrop() && !first.isDrop()) {
			if (betweenBB(m2from, m2to).isSet(m1from))
				return true;
		}

		const PieceType m1pt = first.pieceTypeFromOrDropped();
		const Color us = pos.turn();
		const Bitboard occ = (second.isDrop() ? pos.occupiedBB() : pos.occupiedBB() ^ setMaskBB(m2from));
		const Bitboard m1att = pos.attacksFrom(m1pt, us, m1to, occ);
		if (m1att.isSet(m2to))
			return true;

		if (m1att.isSet(pos.kingSquare(us)))
			return true;

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

	void updatePV(Move* pv, const Move move, const Move* childPV) {
		for (*pv++ = move; childPV && !childPV->isNone();)
			*pv++ = *childPV++;
		*pv = Move::moveNone();
	}

	void updateStats(const Position& pos, SearchStack* ss, const Move move,
					 const Depth depth, const Move* quiets, const int quietsCount) {
		if (ss->killers[0] != move) {
			ss->killers[1] = ss->killers[0];
			ss->killers[0] = move;
		}

		const Score bonus = static_cast<Score>((depth / OnePly) * (depth / OnePly) + depth / OnePly - 1);

		const Square prevSq = (ss-1)->currentMove.to();
//		CounterMoveStats& cmh = CounterMoveHistory[pos.piece_on(prevSq)][prevSq];
		Thread* thisThread = pos.thisThread();

		const Piece pc = colorAndPieceTypeToPiece(pos.turn(), move.pieceTypeFromOrDropped());
		thisThread->history.update(move.isDrop(), pc, move.to(), bonus);

//		if (is_ok((ss-1)->currentMove))
//		{
//			thisThread->counterMoves.update(pos.piece_on(prevSq), prevSq, move);
//			cmh.update(pos.moved_piece(move), to_sq(move), bonus);
//		}

		for (int i = 0; i < quietsCount; ++i) {
			const Piece pc_quiet = colorAndPieceTypeToPiece(pos.turn(), quiets[i].pieceTypeFromOrDropped());
			thisThread->history.update(quiets[i].isDrop(), pc_quiet, quiets[i].to(), -bonus);

//			if (is_ok((ss-1)->currentMove))
//				cmh.update(pos.moved_piece(quiets[i]), to_sq(quiets[i]), -bonus);
		}

//		if (   (ss-1)->moveCount == 1
//			   && !pos.captured_piece_type()
//			   && is_ok((ss-2)->currentMove))
//		{
//			Square prevPrevSq = to_sq((ss-2)->currentMove);
//			CounterMoveStats& prevCmh = CounterMoveHistory[pos.piece_on(prevPrevSq)][prevPrevSq];
//			prevCmh.update(pos.piece_on(prevSq), prevSq, -bonus - 2 * (depth + 1) / ONE_PLY);
//		}
	}

	// fitst move によって、first move の相手側の second move を違法手にするか。
	bool refutes(const Position& pos, const Move first, const Move second) {
		assert(pos.isOK());

		const Square m2to = second.to();
		const Square m1from = first.from(); // 駒打でも今回はこれで良い。

		if (m1from == m2to)
			return true;

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

			if (first.isDrop())
				m1ptTo = first.pieceTypeDropped();
			else {
				m1ptTo = first.pieceTypeTo();
				occ ^= setMaskBB(m1from);
			}

			if (pos.attacksFrom(m1ptTo, us, m1to, occ).isSet(m2to))
				return true;

			const Color them = oppositeColor(us);
			// first で動いた後、sq へ当たりになっている遠隔駒
			const Bitboard xray =
				(pos.attacksFrom<Lance>(them, m2to, occ) & pos.bbOf(Lance, us))
				| (pos.attacksFrom<Rook  >(m2to, occ) & pos.bbOf(Rook, Dragon, us))
				| (pos.attacksFrom<Bishop>(m2to, occ) & pos.bbOf(Bishop, Horse, us));

			// sq へ当たりになっている駒のうち、first で動くことによって新たに当たりになったものがあるなら true
			if (xray.isNot0() && (xray ^ (xray & queenAttack(m2to, pos.occupiedBB()))).isNot0())
				return true;
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

		if (abs(score) < ScoreMateInMaxPly)
			// cp は centi pawn の略
			ss << "cp " << score * 100 / PawnScore;
		else
			// mate の後には、何手で詰むかを表示する。
			ss << "mate " << (0 < score ? ScoreMate0Ply - score : -ScoreMate0Ply - score);

		ss << (beta <= score ? " lowerbound" : score <= alpha ? " upperbound" : "");

		return ss.str();
	}

	inline std::string scoreToUSI(const Score score) {
		return scoreToUSI(score, -ScoreInfinite, ScoreInfinite);
	}

#if defined BISHOP_IN_DANGER
	BishopInDangerFlag detectBishopInDanger(const Position& pos) {
		if (pos.gamePly() <= 60) {
			const Color them = oppositeColor(pos.turn());
			if (pos.hand(pos.turn()).exists<HBishop>()
				&& pos.bbOf(Silver, them).isSet(inverseIfWhite(them, SQ27))
				&& (pos.bbOf(King  , them).isSet(inverseIfWhite(them, SQ48))
					|| pos.bbOf(King  , them).isSet(inverseIfWhite(them, SQ47))
					|| pos.bbOf(King  , them).isSet(inverseIfWhite(them, SQ59)))
				&& pos.bbOf(Pawn  , them).isSet(inverseIfWhite(them, SQ37))
				&& pos.piece(inverseIfWhite(them, SQ28)) == Empty
				&& pos.piece(inverseIfWhite(them, SQ38)) == Empty
				&& pos.piece(inverseIfWhite(them, SQ39)) == Empty)
			{
				return (pos.turn() == Black ? BlackBishopInDangerIn28 : WhiteBishopInDangerIn28);
			}
			else if (pos.hand(pos.turn()).exists<HBishop>()
					 && pos.hand(them).exists<HBishop>()
					 && pos.piece(inverseIfWhite(them, SQ78)) == Empty
					 && pos.piece(inverseIfWhite(them, SQ79)) == Empty
					 && pos.piece(inverseIfWhite(them, SQ68)) == Empty
					 && pos.piece(inverseIfWhite(them, SQ69)) == Empty
					 && pos.piece(inverseIfWhite(them, SQ98)) == Empty
					 && (pieceToPieceType(pos.piece(inverseIfWhite(them, SQ77))) == Silver
						 || pieceToPieceType(pos.piece(inverseIfWhite(them, SQ88))) == Silver)
					 && (pieceToPieceType(pos.piece(inverseIfWhite(them, SQ77))) == Knight
						 || pieceToPieceType(pos.piece(inverseIfWhite(them, SQ89))) == Knight)
					 && ((pieceToPieceType(pos.piece(inverseIfWhite(them, SQ58))) == Gold
						  && pieceToPieceType(pos.piece(inverseIfWhite(them, SQ59))) == King)
						 || pieceToPieceType(pos.piece(inverseIfWhite(them, SQ59))) == Gold))
			{
				return (pos.turn() == Black ? BlackBishopInDangerIn78 : WhiteBishopInDangerIn78);
			}
			else if (pos.hand(pos.turn()).exists<HBishop>()
					 && pos.hand(them).exists<HBishop>()
					 && pos.piece(inverseIfWhite(them, SQ38)) == Empty
					 && pos.piece(inverseIfWhite(them, SQ18)) == Empty
					 && pieceToPieceType(pos.piece(inverseIfWhite(them, SQ28))) == Silver
					 && (pieceToPieceType(pos.piece(inverseIfWhite(them, SQ58))) == King
						 || pieceToPieceType(pos.piece(inverseIfWhite(them, SQ57))) == King
						 || pieceToPieceType(pos.piece(inverseIfWhite(them, SQ58))) == Gold
						 || pieceToPieceType(pos.piece(inverseIfWhite(them, SQ57))) == Gold)
					 && (pieceToPieceType(pos.piece(inverseIfWhite(them, SQ59))) == King
						 || pieceToPieceType(pos.piece(inverseIfWhite(them, SQ59))) == Gold))
			{
				return (pos.turn() == Black ? BlackBishopInDangerIn38 : WhiteBishopInDangerIn38);
			}
		}
		return NotBishopInDanger;
	}
#endif
}

std::string pvInfoToUSI(Position& pos, const size_t pvSize, const Ply depth, const Score alpha, const Score beta) {
	const int t = pos.csearcher()->timeManager.elapsed();
	const size_t usiPVSize = pvSize;
	Ply selDepth = 0; // 選択的に読んでいる部分の探索深さ。
	std::stringstream ss;
	const auto& rootMoves = pos.thisThread()->rootMoves;
	const auto nodesSearched = pos.csearcher()->threads.nodesSearched();

	for (size_t i = 0; i < pos.csearcher()->threads.size(); ++i) {
		if (selDepth < pos.csearcher()->threads[i]->maxPly)
			selDepth = pos.csearcher()->threads[i]->maxPly;
	}

	for (size_t i = usiPVSize-1; 0 <= static_cast<int>(i); --i) {
		const bool update = (i <= pos.thisThread()->pvIdx);

		if (depth == 1 && !update)
			continue;

		const Ply d = (update ? depth : depth - 1);
		const Score s = (update ? rootMoves[i].score_ : rootMoves[i].prevScore_);

		if (ss.rdbuf()->in_avail()) // 空以外は真
			ss << "\n";

		ss << "info depth " << d
		   << " seldepth " << selDepth
		   << " score " << (i == pos.thisThread()->pvIdx ? scoreToUSI(s, alpha, beta) : scoreToUSI(s))
		   << " nodes " << nodesSearched
		   << " nps " << (0 < t ? nodesSearched * 1000 / t : 0)
		   << " time " << t
		   << " multipv " << i + 1
		   << " pv ";

		for (int j = 0; !rootMoves[i].pv_[j].isNone(); ++j)
			ss << " " << rootMoves[i].pv_[j].toUSI();
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
//	auto& tt = pos.searcher()->tt;
	auto& history = pos.thisThread()->history;

	StateInfo st;
	TTEntry* tte;
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
	bool ttHit;
	bool givesCheck;
	bool evasionPrunable;
	Depth ttDepth;

	if (PVNode)
		oldAlpha = alpha;

	ss->currentMove = bestMove = Move::moveNone();
	ss->ply = (ss-1)->ply + 1;

	if (MaxPly < ss->ply)
		return ScoreDraw;

	ttDepth = ((INCHECK || DepthQChecks <= depth) ? DepthQChecks : DepthQNoChecks);

	posKey = pos.getKey();
	tte = tt.probe(posKey, ttHit);
	ttMove = (ttHit ? move16toMove(tte->move(), pos) : Move::moveNone());
	ttScore = (ttHit ? scoreFromTT(tte->score(), ss->ply) : ScoreNone);

	if (ttHit
		&& ttDepth <= tte->depth()
		&& ttScore != ScoreNone // アクセス競合が起きたときのみ、ここに引っかかる。
		&& (PVNode ? tte->bound() == BoundExact
			: (beta <= ttScore ? (tte->bound() & BoundLower)
			   : (tte->bound() & BoundUpper))))
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
		if (!(move = pos.mateMoveIn1Ply()).isNone())
			return mateIn(ss->ply);

		if (ttHit) {
			if ((ss->staticEval = bestScore = tte->evalScore()) == ScoreNone)
				ss->staticEval = bestScore = evaluate(pos, ss);
		}
		else
			ss->staticEval = bestScore = evaluate(pos, ss);

		if (beta <= bestScore) {
			if (!ttHit)
				tte->save(pos.getKey(), scoreToTT(bestScore, ss->ply), BoundLower,
						  DepthNone, Move::moveNone(), ss->staticEval, tt.generation());

			return bestScore;
		}

		if (PVNode && alpha < bestScore)
			alpha = bestScore;

		futilityBase = bestScore + 128; // todo: 128 より大きくて良いと思う。
	}

	evaluate(pos, ss);

	MovePicker mp(pos, ttMove, depth, history, (ss-1)->currentMove.to());
	const CheckInfo ci(pos);

	while (!(move = mp.nextMove()).isNone())
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
			if (move.isPromotion())
				futilityScore += Position::promotePieceScore(move.pieceTypeFrom());

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
						   && !move.isCaptureOrPawnPromotion());

		if (!PVNode
			&& (!INCHECK || evasionPrunable)
			&& move != ttMove
			&& (!move.isPromotion() || move.pieceTypeFrom() != Pawn)
			&& pos.seeSign(move) < 0)
		{
			continue;
		}

		if (!pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned))
			continue;

		ss->currentMove = move;

		pos.doMove(move, st, ci, givesCheck);
		(ss+1)->staticEvalRaw.p[0][0] = ScoreNotEvaluated;
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
					tte->save(posKey, scoreToTT(score, ss->ply), BoundLower,
							  ttDepth, move, ss->staticEval, tt.generation());
					return score;
				}
			}
		}
	}

	if (INCHECK && bestScore == -ScoreInfinite)
		return matedIn(ss->ply);

	tte->save(posKey, scoreToTT(bestScore, ss->ply), 
			  ((PVNode && oldAlpha < bestScore) ? BoundExact : BoundUpper),
			  ttDepth, bestMove, ss->staticEval, tt.generation());

	assert(-ScoreInfinite < bestScore && bestScore < ScoreInfinite);

	return bestScore;
}

void Thread::search() {
	SearchStack ss[MaxPlyPlus4];
	Score bestScore = -ScoreInfinite;
	Score delta = -ScoreInfinite;
	Score alpha = -ScoreInfinite;
	Score beta = ScoreInfinite;
	Move easyMove = Move::moveNone();
	MainThread* mainThread = (this == searcher->threads.mainThread() ? searcher->threads.mainThread() : nullptr);
	int lastInfoTime = -1; // 将棋所のコンソールが詰まる問題への対処用

	memset(ss, 0, 5 * sizeof(SearchStack));
	completedDepth = Depth0;

	if (mainThread) {
		easyMove = searcher->easyMove.get(rootPosition.getKey());
		searcher->easyMove.clear();
		mainThread->easyMovePlayed = mainThread->failedLow = false;
		mainThread->bestMoveChanges = 0;
		searcher->tt.newSearch();
	}

	ss[0].currentMove = ss[1].currentMove = Move::moveNull(); // skip update gains
	searcher->tt.newSearch();
	history.clear();
	gains.clear();

	size_t pvSize = searcher->options["MultiPV"];
	Skill skill(SkillLevel, searcher->options["Max_Random_Score_Diff"]);

	if (searcher->options["Max_Random_Score_Diff_Ply"] < rootPosition.gamePly()) {
		skill.max_random_score_diff = ScoreZero;
		pvSize = 1;
		assert(!skill.enabled()); // level による設定が出来るようになるまでは、これで良い。
	}

	if (skill.enabled() && pvSize < 3)
		pvSize = 3;
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
	while (++rootDepth <= MaxPly && !searcher->signals.stop && (!searcher->limits.depth || rootDepth <= searcher->limits.depth)) {
		if (!mainThread) {
			const Row& row = HalfDensity[(idx - 1) % HalfDensitySize];
			if (row[(rootDepth + rootPosition.gamePly()) % row.size()])
				continue;
		}
		if (mainThread) {
			mainThread->bestMoveChanges *= 0.505;
			mainThread->failedLow = false;
		}
		// 前回の iteration の結果を全てコピー
		for (RootMove& rm : rootMoves)
			rm.prevScore_ = rm.score_;

		// Multi PV loop
		for (pvIdx = 0; pvIdx < pvSize && !searcher->signals.stop; ++pvIdx) {
#if defined LEARN
			alpha = searcher->alpha;
			beta  = searcher->beta;
#else
			// aspiration search
			// alpha, beta をある程度絞ることで、探索効率を上げる。
			if (5 <= rootDepth) {
				delta = static_cast<Score>(18);
				alpha = std::max(rootMoves[pvIdx].prevScore_ - delta, -ScoreInfinite);
				beta  = std::min(rootMoves[pvIdx].prevScore_ + delta, ScoreInfinite);
			}
#endif

			// aspiration search の window 幅を、初めは小さい値にして探索し、
			// fail high/low になったなら、今度は window 幅を広げて、再探索を行う。
			while (true) {
				// 探索を行う。
				ss->staticEvalRaw.p[0][0] = (ss+1)->staticEvalRaw.p[0][0] = (ss+2)->staticEvalRaw.p[0][0] = ScoreNotEvaluated;
				bestScore = searcher->search<Root>(rootPosition, ss + 2, alpha, beta, static_cast<Depth>(rootDepth * OnePly), false);
				// 先頭が最善手になるようにソート
				insertionSort(rootMoves.begin() + pvIdx, rootMoves.end()); // todo: std::stable_sort() で良いのでは？

				for (size_t i = 0; i <= pvIdx; ++i) {
					ss->staticEvalRaw.p[0][0] = (ss+1)->staticEvalRaw.p[0][0] = (ss+2)->staticEvalRaw.p[0][0] = ScoreNotEvaluated; // todo: 不要ぽい。
					rootMoves[i].insertPvInTT(rootPosition);
				}

#if 0
				// 詰みを発見したら即指す。
				if (ScoreMateInMaxPly <= abs(bestScore) && abs(bestScore) < ScoreInfinite) {
					SYNCCOUT << pvInfoToUSI(rootPosition, pvSize, ply, alpha, beta) << SYNCENDL;
					signals.stop = true;
				}
#endif

#if defined LEARN
				break;
#endif

				if (searcher->signals.stop)
					break;

				if (alpha < bestScore && bestScore < beta)
					break;

				if (mainThread
					&& 3000 < searcher->timeManager.elapsed()
					// 将棋所のコンソールが詰まるのを防ぐ。
					&& (rootDepth < 10 || lastInfoTime + 200 < searcher->timeManager.elapsed()))
				{
					lastInfoTime = searcher->timeManager.elapsed();
					SYNCCOUT << pvInfoToUSI(rootPosition, pvSize, rootDepth, alpha, beta) << SYNCENDL;
				}

				// fail high/low のとき、aspiration window を広げる。
				if (bestScore <= alpha) {
					beta = (alpha + beta) / 2;
					alpha = std::max(bestScore - delta, -ScoreInfinite);
					if (mainThread) {
						mainThread->failedLow = true;
						searcher->signals.stopOnPonderHit = false;
					}
				}
				else if (beta <= bestScore) {
					alpha = (alpha + beta) / 2;
					beta = std::min(bestScore + delta, ScoreInfinite);
				}
				else
					break;
				delta += delta / 4 + 5;

				assert(-ScoreInfinite <= alpha && beta <= ScoreInfinite);
			}

			insertionSort(rootMoves.begin(), rootMoves.begin() + pvIdx + 1); // todo: std::stable_sort() で良いのでは？

			if (!mainThread)
				break;

			if (!searcher->signals.stop
				&& (pvIdx + 1 == pvSize || 3000 < searcher->timeManager.elapsed())
				// 将棋所のコンソールが詰まるのを防ぐ。
				&& (rootDepth < 10 || lastInfoTime + 200 < searcher->timeManager.elapsed()))
			{
				lastInfoTime = searcher->timeManager.elapsed();
				SYNCCOUT << pvInfoToUSI(rootPosition, pvSize, rootDepth, alpha, beta) << SYNCENDL;
			}
		}

		if (!searcher->signals.stop)
			completedDepth = rootDepth;

		if (!mainThread)
			continue;

		//if (skill.enabled() && skill.timeToPick(depth)) {
		//	skill.pickMove();
		//}

		if (searcher->limits.useTimeManagement()) {
			if (!searcher->signals.stop && !searcher->signals.stopOnPonderHit) {
				const bool F[] = {!mainThread->failedLow, bestScore >= mainThread->previousScore};
				const int improvingFactor = 640 - 160 * F[0] - 126 * F[1] - 124 * F[0] * F[1];
				const double unstablePVFactor = 1 + mainThread->bestMoveChanges;
				const bool doEasyMove = (rootMoves[0].pv_[0] == easyMove
                                         && mainThread->bestMoveChanges < 0.03
                                         && searcher->timeManager.elapsed() > searcher->timeManager.optimumSearchTime() * 25 / 204);

                if (rootMoves.size() == 1
                    || searcher->timeManager.elapsed() > searcher->timeManager.optimumSearchTime() * unstablePVFactor * improvingFactor / 634
					|| (mainThread->easyMovePlayed = doEasyMove))
				{
					if (searcher->limits.ponder)
						searcher->signals.stopOnPonderHit = true;
					else
						searcher->signals.stop = true;
				}
			}

			if (rootMoves[0].pv_.size() >= 3)
				searcher->easyMove.update(rootPosition, rootMoves[0].pv_);
			else
				searcher->easyMove.clear();
		}
	}

	if (!mainThread)
		return;

	if (searcher->easyMove.stableCount < 6 || mainThread->easyMovePlayed)
		searcher->easyMove.clear();

	skill.swapIfEnabled(this, pvSize);
	SYNCCOUT << pvInfoToUSI(rootPosition, pvSize, rootDepth-1, alpha, beta) << SYNCENDL;
}

#if defined INANIWA_SHIFT
// 稲庭判定
void Searcher::detectInaniwa(const Position& pos) {
	const auto prevInaniwaFlag = inaniwaFlag;
	inaniwaFlag = NotInaniwa;
	if (70 <= pos.gamePly()) {
		const Rank Trank3 = (pos.turn() == Black ? Rank3 : Rank7); // not constant
		const Bitboard mask = rankMask(Trank3) & ~fileMask<File9>() & ~fileMask<File1>();
		if ((pos.bbOf(Pawn, oppositeColor(pos.turn())) & mask) == mask)
			inaniwaFlag = (pos.turn() == Black ? InaniwaIsWhite : InaniwaIsBlack);
	}
	// 稲庭判定の結果が変わったら、過去に探索した評価値は使えないのでクリアする必要がある。
	// 稲庭判定をした後に全く別の対局の sfen を送られても対応出来るようにする。
	if (prevInaniwaFlag != inaniwaFlag) {
		tt.clear();
		g_evalTable.clear();
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
	const bool PVNode = (NT == PV || NT == Root);
	const bool RootNode = (NT == Root);

	assert(-ScoreInfinite <= alpha && alpha < beta && beta <= ScoreInfinite);
	assert(PVNode || (alpha == beta - 1));
	assert(Depth0 < depth);

	// 途中で goto を使用している為、先に全部の変数を定義しておいた方が安全。
	Move movesSearched[64];
	StateInfo st;
	TTEntry* tte;
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
	bool ttHit;
	bool inCheck;
	bool givesCheck;
	bool isPVMove;
	bool singularExtensionNode;
	bool captureOrPawnPromotion;
	bool dangerous;
	bool doFullDepthSearch;
	int moveCount;
	int playedMoveCount;

	// step1
	// initialize node
	Thread* thisThread = pos.thisThread();
	auto& history = thisThread->history;
	auto& gains = thisThread->gains;
	auto& rootMoves = thisThread->rootMoves;
	moveCount = playedMoveCount = 0;
	inCheck = pos.inCheck();

	bestScore = -ScoreInfinite;
	ss->currentMove = threatMove = bestMove = (ss + 1)->excludedMove = Move::moveNone();
	ss->ply = (ss-1)->ply + 1;
	(ss+1)->skipNullMove = false;
	(ss+1)->reduction = Depth0;
	(ss+2)->killers[0] = (ss+2)->killers[1] = Move::moveNone();

	if (thisThread->resetCalls.load(std::memory_order_relaxed)) {
		thisThread->resetCalls = false;
		thisThread->callsCount = 0;
	}
	if (++thisThread->callsCount > 4096) {
		for (Thread* th : threads)
			th->resetCalls = true;

		checkTime();
	}

	if (PVNode && thisThread->maxPly < ss->ply)
		thisThread->maxPly = ss->ply;

	if (!RootNode) {
		// step2
		// stop と最大探索深さのチェック
		switch (pos.isDraw(16)) {
		case NotRepetition      : if (!signals.stop.load(std::memory_order_relaxed) && ss->ply <= MaxPly) break; // else の場合は fallthrough
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
			if (beta <= alpha)
				return alpha;
		}
	}

	pos.setNodesSearched(pos.nodesSearched() + 1);

	// step4
	// trans position table lookup
	excludedMove = ss->excludedMove;
	posKey = (excludedMove.isNone() ? pos.getKey() : pos.getExclusionKey());
	tte = tt.probe(posKey, ttHit);
	ttMove = 
		RootNode ? rootMoves[pos.thisThread()->pvIdx].pv_[0] :
		ttHit ?
		move16toMove(tte->move(), pos) :
		Move::moveNone();
	ttScore = (ttHit ? scoreFromTT(tte->score(), ss->ply) : ScoreNone);

	if (!RootNode
		&& ttHit
		&& depth <= tte->depth()
		&& ttScore != ScoreNone // アクセス競合が起きたときのみ、ここに引っかかる。
		&& (PVNode ? tte->bound() == BoundExact
			: (beta <= ttScore ? (tte->bound() & BoundLower)
			   : (tte->bound() & BoundUpper))))
	{
		ss->currentMove = ttMove; // Move::moveNone() もありえる。

		if (beta <= ttScore
			&& !ttMove.isNone()
			&& !ttMove.isCaptureOrPawnPromotion()
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
			tte->save(posKey, scoreToTT(bestScore, ss->ply), BoundExact, depth,
					  move, ss->staticEval, tt.generation());
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
	else if (ttHit) {
		if (ttScore != ScoreNone
			&& (tte->bound() & (eval < ttScore ? BoundLower : BoundUpper)))
		{
			eval = ttScore;
		}
	}
	else {
		tte->save(posKey, ScoreNone, BoundNone, DepthNone,
				  Move::moveNone(), ss->staticEval, tt.generation());
	}

	// 一手前の指し手について、history を更新する。
	// todo: ここの条件はもう少し考えた方が良い。
	if ((move = (ss-1)->currentMove) != Move::moveNull()
		&& (ss-1)->staticEval != ScoreNone
		&& ss->staticEval != ScoreNone
		&& !move.isCaptureOrPawnPromotion() // 前回(一手前)の指し手が駒取りでなかった。
		)
	{
		const Square to = move.to();
		gains.update(move.isDrop(), pos.piece(to), to, -(ss-1)->staticEval - ss->staticEval);
	}

	// step6
	// razoring
	if (!PVNode
		&& depth < 4 * OnePly
		&& eval + razorMargin(depth) <= alpha
		&& ttMove.isNone())
	{
		if (depth <= OnePly && eval + razorMargin(3 * OnePly) <= alpha)
			return qsearch<NonPV, false>(pos, ss, alpha, beta, Depth0);
		const Score ralpha = alpha - razorMargin(depth);
		const Score s = qsearch<NonPV, false>(pos, ss, ralpha, ralpha+1, Depth0);
		if (s <= ralpha)
			return s;
	}

	// step7
	// static null move pruning
	if (!PVNode
		&& !ss->skipNullMove
		&& depth < 6 * OnePly
		&& beta <= eval - futilityMargin(depth)
		&& abs(beta) < ScoreMateInMaxPly)
	{
		return eval - futilityMargin(depth);
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
		Depth reduction = static_cast<Depth>(4) * OnePly + depth / 4;

		if (beta < eval - PawnScore)
			reduction += OnePly;

		pos.doNullMove<true>(st);
		(ss+1)->staticEvalRaw = (ss)->staticEvalRaw; // 評価値の差分評価の為。
		(ss+1)->skipNullMove = true;
		Score nullScore = (depth - reduction < OnePly ?
						   -qsearch<NonPV, false>(pos, ss + 1, -beta, -alpha, Depth0)
						   : -search<NonPV>(pos, ss + 1, -beta, -alpha, depth - reduction, !cutNode));
		(ss+1)->skipNullMove = false;
		pos.doNullMove<false>(st);

		if (beta <= nullScore) {
			if (ScoreMateInMaxPly <= nullScore)
				nullScore = beta;

			if (depth < 6 * OnePly)
				return nullScore;

			ss->skipNullMove = true;
			const Score s = (depth - reduction < OnePly ?
							 qsearch<NonPV, false>(pos, ss, alpha, beta, Depth0)
							 : search<NonPV>(pos, ss, alpha, beta, depth - reduction, false));
			ss->skipNullMove = false;

			if (beta <= s)
				return nullScore;
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
		while (!(move = mp.nextMove()).isNone()) {
			if (pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned)) {
				ss->currentMove = move;
				pos.doMove(move, st, ci, pos.moveGivesCheck(move, ci));
				(ss+1)->staticEvalRaw.p[0][0] = ScoreNotEvaluated;
				score = -search<NonPV>(pos, ss+1, -rbeta, -rbeta+1, rdepth, !cutNode);
				pos.undoMove(move);
				if (rbeta <= score)
					return score;
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

		tte = tt.probe(posKey, ttHit);
		ttMove = (ttHit ?
				  move16toMove(tte->move(), pos) :
				  Move::moveNone());
	}

	MovePicker mp(pos, ttMove, depth, history, ss, PVNode ? -ScoreInfinite : beta);
	const CheckInfo ci(pos);
	bool improving = (ss->staticEval >= (ss-2)->staticEval
					  || ss->staticEval == ScoreNone
					  || (ss-2)->staticEval == ScoreNone);
	score = bestScore;
	singularExtensionNode =
		!RootNode
		&& 8 * OnePly <= depth
		&& !ttMove.isNone()
		&& excludedMove.isNone()
		&& (tte->bound() & BoundLower)
		&& depth - 3 * OnePly <= tte->depth();

	// step11
	// Loop through moves
	while (!(move = mp.nextMove()).isNone()) {
		if (move == excludedMove)
			continue;

		if (RootNode
			&& std::find(rootMoves.begin() + pos.thisThread()->pvIdx,
						 rootMoves.end(),
						 move) == rootMoves.end())
		{
			continue;
		}

		++moveCount;

		if (RootNode) {
#if 0
			if (thisThread == threads.mainThread() && 3000 < timeManager.elapsed()) {
				SYNCCOUT << "info depth " << depth / OnePly
						 << " currmove " << move.toUSI()
						 << " currmovenumber " << moveCount + pvIdx << SYNCENDL;
			}
#endif
		}

		extension = Depth0;
		captureOrPawnPromotion = move.isCaptureOrPawnPromotion();
		givesCheck = pos.moveGivesCheck(move, ci);
		dangerous = givesCheck; // todo: not implement

		// step12
		if (givesCheck && ScoreZero <= pos.seeSign(move))
			extension = OnePly;

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
			&& !captureOrPawnPromotion
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
				continue;
			}

			// score based pruning
			const Depth predictedDepth = newDepth - reduction<PVNode>(improving, depth, moveCount);

			if (predictedDepth < 7 * OnePly) {
				// gain を 2倍にする。
				const Score futilityScore = ss->staticEval + futilityMargin(predictedDepth);
				if (futilityScore < beta) {
					bestScore = std::max(bestScore, futilityScore);
					continue;
				}
			}

			if (predictedDepth < 4 * OnePly && pos.seeSign(move) < ScoreZero)
				continue;
		}

		// RootNode, SPNode はすでに合法手であることを確認済み。
		if (!RootNode && !pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned)) {
			--moveCount;
			continue;
		}

		isPVMove = (PVNode && moveCount == 1);
		ss->currentMove = move;
		if (!captureOrPawnPromotion && playedMoveCount < 64)
			movesSearched[playedMoveCount++] = move;

		// step14
		pos.doMove(move, st, ci, givesCheck);
		(ss+1)->staticEvalRaw.p[0][0] = ScoreNotEvaluated;

		// step15
		// LMR
		if (3 * OnePly <= depth
			&& !isPVMove
			&& !(pos.gamePly() > 70 // todo: 進行度などに変えた方が良いだろう。
				 && thisThread == pos.searcher()->threads.back()
				 && st.continuousCheck[oppositeColor(pos.turn())] >= 4) // !(70手以上 && mainThread && 連続王手)
			&& !captureOrPawnPromotion
			&& move != ttMove
			&& ss->killers[0] != move
			&& ss->killers[1] != move)
		{
			ss->reduction = reduction<PVNode>(improving, depth, moveCount);
			if (!PVNode && cutNode)
				ss->reduction += OnePly;
			const Depth d = std::max(newDepth - ss->reduction, OnePly);
			// PVS
			score = -search<NonPV>(pos, ss+1, -(alpha + 1), -alpha, d, true);

			doFullDepthSearch = (alpha < score && ss->reduction != Depth0);
			ss->reduction = Depth0;
		}
		else
			doFullDepthSearch = !isPVMove;

		// step16
		// full depth search
		// PVS
		if (doFullDepthSearch) {
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
		if (signals.stop.load(std::memory_order_relaxed))
			return ScoreZero;

		if (RootNode) {
			RootMove& rm = *std::find(rootMoves.begin(), rootMoves.end(), move);
			if (isPVMove || alpha < score) {
				// PV move or new best move
				rm.score_ = score;
				rm.extractPvFromTT(pos);

				if (moveCount > 1 && thisThread == threads.mainThread())
					++static_cast<MainThread*>(thisThread)->bestMoveChanges;
			}
			else
				rm.score_ = -ScoreInfinite;
		}

		if (bestScore < score) {
			bestScore = score;

			if (alpha < score) {
				bestMove = move;

				// todo: update pv

				if (PVNode && score < beta)
					alpha = score;
				else
					break; // fail high
			}
		}
	}

	// step20
	if (moveCount == 0)
		return !excludedMove.isNone() ? alpha : matedIn(ss->ply);

	if (bestScore == -ScoreInfinite) {
		assert(playedMoveCount == 0);
		bestScore = alpha;
	}

	if (beta <= bestScore) {
		// failed high
		tte->save(posKey, scoreToTT(bestScore, ss->ply), BoundLower, depth,
				  bestMove, ss->staticEval, tt.generation());

		if (!bestMove.isCaptureOrPawnPromotion()) {
			if (!inCheck && bestMove != ss->killers[0]) {
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
	else
		// failed low or PV search
		tte->save(posKey, scoreToTT(bestScore, ss->ply),
				  ((PVNode && !bestMove.isNone()) ? BoundExact : BoundUpper),
				  depth, bestMove, ss->staticEval, tt.generation());

	assert(-ScoreInfinite < bestScore && bestScore < ScoreInfinite);

	return bestScore;
}

void RootMove::extractPvFromTT(Position& pos) {
	StateInfo state[MaxPlyPlus4];
	StateInfo* st = state;
	TTEntry* tte;
	Ply ply = 0;
	Move m = pv_[0];
	bool ttHit;

	assert(!m.isNone() && pos.moveIsPseudoLegal(m));

	pv_.clear();

	do {
		pv_.push_back(m);

		assert(pos.moveIsLegal(pv_[ply]));
		pos.doMove(pv_[ply++], *st++);
		tte = pos.csearcher()->tt.probe(pos.getKey(), ttHit);
	}
	while (ttHit
		   // このチェックは少し無駄。駒打ちのときはmove16toMove() 呼ばなくて良い。
		   && pos.moveIsPseudoLegal(m = move16toMove(tte->move(), pos))
		   && pos.pseudoLegalMoveIsLegal<false, false>(m, pos.pinnedBB())
		   && ply < MaxPly
		   && (!pos.isDraw(20) || ply < 6));

	pv_.push_back(Move::moveNone());
	while (ply)
		pos.undoMove(pv_[--ply]);
}

void RootMove::insertPvInTT(Position& pos) {
	StateInfo state[MaxPlyPlus4];
	StateInfo* st = state;
	TTEntry* tte;
	Ply ply = 0;
	bool ttHit;

	do {
		tte = pos.csearcher()->tt.probe(pos.getKey(), ttHit);

		if (!ttHit || move16toMove(tte->move(), pos) != pv_[ply])
			tte->save(pos.getKey(), ScoreNone, BoundNone, DepthNone, pv_[ply], ScoreNone, pos.csearcher()->tt.generation());

		assert(pos.moveIsLegal(pv_[ply]));
		pos.doMove(pv_[ply++], *st++);
	} while (!pv_[ply].isNone());

	while (ply)
		pos.undoMove(pv_[--ply]);
}

void initSearchTable() {
	// Init reductions array
	for (int improving = 0; improving < 2; ++improving) {
		for (int hd = 1; hd < 64; hd++) {
			for (int mc = 1; mc < 64; mc++) {
				double    pvRed = log(double(hd)) * log(double(mc)) / 3.0;
				double nonPVRed = 0.33 + log(double(hd)) * log(double(mc)) / 2.25;
				Reductions[1][improving][hd][mc] = (int8_t) (   pvRed >= 1.0 ? floor(   pvRed * int(OnePly)) : 0);
				Reductions[0][improving][hd][mc] = (int8_t) (nonPVRed >= 1.0 ? floor(nonPVRed * int(OnePly)) : 0);
				if (!improving && Reductions[0][improving][hd][mc] >= 2 * OnePly)
					Reductions[0][improving][hd][mc] += OnePly;
			}
		}
	}

	// init futility move counts
	for (int d = 0; d < 32; ++d)
		FutilityMoveCounts[d] = static_cast<int>(3.001 + 0.3 * pow(d, 1.8));
}

// 入玉勝ちかどうかを判定
bool nyugyoku(const Position& pos) {
	// CSA ルールでは、一 から 六 の条件を全て満たすとき、入玉勝ち宣言が出来る。

	// 一 宣言側の手番である。

	// この関数を呼び出すのは自分の手番のみとする。ponder では呼び出さない。

	const Color us = pos.turn();
	// 敵陣のマスク
	const Bitboard opponentsField = (us == Black ? inFrontMask<Black, Rank4>() : inFrontMask<White, Rank6>());

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

void MainThread::search() {
#if defined LEARN
	maxPly = 0;
	rootDepth = Depth0;
	Thread::search();
#else
	static Book book;
	Position& pos = rootPosition;
	searcher->timeManager.init(searcher->limits, pos.gamePly(), pos.turn(), searcher);
	auto& options = searcher->options;
	auto& tt = searcher->tt;
	auto& signals = searcher->signals;
	std::uniform_int_distribution<int> dist(options["Min_Book_Ply"], options["Max_Book_Ply"]);
	const Ply book_ply = dist(g_randomTimeSeed);
	bool searched = false;

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
#if defined BISHOP_IN_DANGER
	{
		auto deleteFunc = [](const std::string& str) {
			auto it = std::find_if(std::begin(rootMoves), std::end(rootMoves), [&str](const RootMove& rm) {
					return rm.pv_[0].toCSA() == str;
				});
			if (it != std::end(rootMoves))
				rootMoves.erase(it);
		};
		switch (detectBishopInDanger(pos)) {
		case NotBishopInDanger: break;
		case BlackBishopInDangerIn28: deleteFunc("0082KA"); break;
		case WhiteBishopInDangerIn28: deleteFunc("0028KA"); break;
		case BlackBishopInDangerIn78: deleteFunc("0032KA"); break;
		case WhiteBishopInDangerIn78: deleteFunc("0078KA"); break;
		case BlackBishopInDangerIn38: deleteFunc("0072KA"); break;
		case WhiteBishopInDangerIn38: deleteFunc("0038KA"); break;
		default: UNREACHABLE;
		}
	}
#endif

#if defined INANIWA_SHIFT
	detectInaniwa(pos);
#endif
	for (Thread* th : searcher->threads) {
		th->maxPly = 0;
		th->rootDepth = Depth0;
		if (th != this) {
			th->rootPosition = Position(rootPosition, th);
			th->rootMoves = rootMoves;
			th->startSearching();
		}
	}
	Thread::search();
	searched = true;

finalize:

	SYNCCOUT << "info nodes " << searcher->threads.nodesSearched()
			 << " time " << searcher->timeManager.elapsed() << SYNCENDL;

	if (!signals.stop && (searcher->limits.ponder || searcher->limits.infinite)) {
		signals.stopOnPonderHit = true;
		wait(signals.stop);
	}

	signals.stop = true;

	for (Thread* th : searcher->threads)
		if (th != this)
			th->waitForSearchFinished();

	Thread* bestThread = this;
	if (searched
		&& !this->easyMovePlayed
		&& searcher->options["MultiPV"] == 1
		&& !Skill(SkillLevel, searcher->options["Max_Random_Score_Diff"]).enabled())
	{
		for (Thread* th : searcher->threads)
			if (th->completedDepth > bestThread->completedDepth
				&& th->rootMoves[0].score_ > bestThread->rootMoves[0].score_)
			{
				bestThread = th;
			}
	}

	previousScore = bestThread->rootMoves[0].score_;

	if (bestThread != this)
		SYNCCOUT << pvInfoToUSI(bestThread->rootPosition, 1, bestThread->completedDepth, -ScoreInfinite, ScoreInfinite) << SYNCENDL;

	if (nyugyokuWin)
		SYNCCOUT << "bestmove win" << SYNCENDL;
	else if (bestThread->rootMoves[0].pv_[0].isNone())
			SYNCCOUT << "bestmove resign" << SYNCENDL;
	else
		SYNCCOUT << "bestmove " << bestThread->rootMoves[0].pv_[0].toUSI()
				 << " ponder " << bestThread->rootMoves[0].pv_[1].toUSI()
				 << SYNCENDL;
#endif
}

void Searcher::checkTime() {
	if (limits.ponder)
		return;

	const auto elapsed = timeManager.elapsed();
	if ((limits.useTimeManagement() && elapsed > timeManager.maximumTime() - 10)
		|| (limits.moveTime && elapsed >= limits.moveTime)
		|| (limits.nodes && threads.nodesSearched() >= limits.nodes))
	{
		signals.stop = true;
	}
}
