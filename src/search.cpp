/*
  Apery, a USI shogi playing engine derived from Stockfish, a UCI chess playing engine.
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2011-2016 Hiraoka Takuya

  Apery is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Apery is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
StateListPtr Searcher::states;
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
	tt.resize(options["USI_Hash"]);
}

void Searcher::clear() {
	tt.clear();
	for (Thread* th : threads) {
		th->history.clear();
		th->counterMoves.clear();
		th->fromTo.clear();
		th->counterMoveHistory.clear();
	}
	threads.main()->previousScore = ScoreInfinite;
}

namespace {
	const Score Tempo = (Score)20;
	const int SkillLevel = 20; // [0, 20] 大きいほど強くする予定。現状 20 以外未対応。

	const int RazorMargin[4] = { 483, 570, 603, 554 };
	inline Score futilityMargin(const Depth depth) { return static_cast<Score>(150 * depth / OnePly); }

	int FutilityMoveCounts[2][16]; // [improving][depth]
	int Reductions[2][2][64][64];  // [pv][improving][depth][moveNumber]

	template <bool PVNode> inline Depth reduction(const bool improving, const Depth depth, const int moveCount) {
		return Reductions[PVNode][improving][std::min(depth / OnePly, 63)][std::min(moveCount, 63)] * OnePly;
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
									(best ? best : pickMove(th, pvSize)));
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
					if (max_random_score_diff < th->rootMoves[0].score - th->rootMoves[i].score)
						break;
				}
				// 0 から i-1 までの間でランダムに選ぶ。
				std::uniform_int_distribution<size_t> dist(0, i-1);
				best = th->rootMoves[dist(g_randomTimeSeed)].pv[0];
				return best;
			}
			best = th->rootMoves[0].pv[0];
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

	Score scoreToTT(const Score s, const Ply ply) {
		assert(s != ScoreNone);

		return (ScoreMateInMaxPly <= s ? s + ply
				: s <= ScoreMatedInMaxPly ? s - ply
				: s);
	}

	Score scoreFromTT(const Score s, const Ply ply) {
		return (s == ScoreNone ? ScoreNone
				: ScoreMateInMaxPly <= s ? s - ply
				: s <= ScoreMatedInMaxPly ? s + ply
				: s);
	}

	void updatePV(Move* pv, const Move move, const Move* childPV) {
		for (*pv++ = move; childPV && *childPV != Move::moveNone();)
			*pv++ = *childPV++;
		*pv = Move::moveNone();
	}

	void updateCMStats(SearchStack* ss, const Piece pc, const Square sq, const Score bonus) {
		CounterMoveStats* cmh  = (ss-1)->counterMoves;
		CounterMoveStats* fmh1 = (ss-2)->counterMoves;
		CounterMoveStats* fmh2 = (ss-4)->counterMoves;
		if (cmh)
			cmh->update(pc, sq, bonus);
		if (fmh1)
			fmh1->update(pc, sq, bonus);
		if (fmh2)
			fmh2->update(pc, sq, bonus);
	}

	void updateStats(const Position& pos, SearchStack* ss, const Move move,
					 const Move* quiets, const int quietsCount, const Score bonus) {
		if (ss->killers[0] != move) {
			ss->killers[1] = ss->killers[0];
			ss->killers[0] = move;
		}

		const Color c = pos.turn();
		Thread* thisThread = pos.thisThread();
		thisThread->fromTo.update(c, move, bonus);
		thisThread->history.update(pos.movedPiece(move), move.to(), bonus);
		updateCMStats(ss, pos.movedPiece(move), move.to(), bonus);
		if ((ss-1)->counterMoves) {
			const Square prevSq = (ss-1)->currentMove.to();
			thisThread->counterMoves.update(pos.piece(prevSq), prevSq, move);
		}

		for (int i = 0; i < quietsCount; ++i) {
			thisThread->fromTo.update(c, quiets[i], -bonus);
			thisThread->history.update(pos.movedPiece(quiets[i]), quiets[i].to(), -bonus);
			updateCMStats(ss, pos.movedPiece(quiets[i]), quiets[i].to(), -bonus);
		}
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

std::string pvInfoToUSI(Position& pos, const size_t pvSize, const Depth depth, const Score alpha, const Score beta) {
	std::stringstream ss;
	const int elapsed = pos.csearcher()->timeManager.elapsed() + 1;
	const auto& rootMoves = pos.thisThread()->rootMoves;
	const size_t PVIdx = pos.thisThread()->pvIdx;
	const size_t usiPVSize = pvSize;
	const auto nodesSearched = pos.csearcher()->threads.nodesSearched();

	for (size_t i = usiPVSize-1; 0 <= static_cast<int>(i); --i) {
		const bool update = (i <= PVIdx);

		if (depth == OnePly && !update)
			continue;

		const Depth d = (update ? depth : depth - OnePly);
		const Score s = (update ? rootMoves[i].score : rootMoves[i].previousScore);

		if (ss.rdbuf()->in_avail()) // 空以外は真
			ss << "\n";

		ss << "info depth " << d / OnePly
		   << " seldepth " << pos.thisThread()->maxPly
		   << " multipv " << i + 1
		   << " score " << (i == PVIdx ? scoreToUSI(s, alpha, beta) : scoreToUSI(s))
		   << " nodes " << nodesSearched
		   << " nps " << nodesSearched * 1000 / elapsed
		   << " time " << elapsed
		   << " pv";

		for (Move m : rootMoves[i].pv)
			ss << " " << m.toUSI();
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
	assert(depth / OnePly * OnePly == depth);
	auto& tt = pos.searcher()->tt;
	//auto& history = pos.thisThread()->history;

	Move pv[MaxPly+1];
	StateInfo st;
	TTEntry* tte;
	Key posKey;
	Move ttMove, move, bestMove;
	Score bestScore, score, ttScore, futilityScore, futilityBase, oldAlpha;
	bool ttHit, givesCheck, evasionPrunable;
	Depth ttDepth;

	if (PVNode) {
		oldAlpha = alpha;
		(ss+1)->pv = pv;
		ss->pv[0] = Move::moveNone();
	}

	ss->currentMove = bestMove = Move::moveNone();
	ss->ply = (ss-1)->ply + 1;

	if (ss->ply >= MaxPly) // isDraw() 呼び出しは遅い。駒取りと成りなら千日手にならないので省く。
		return ScoreDraw;

	assert(0 <= ss->ply && ss->ply < MaxPly);

	ttDepth = (INCHECK || depth >= DepthQChecks ? DepthQChecks: DepthQNoChecks);

	posKey = pos.getKey();
	tte = tt.probe(posKey, ttHit);
	ttMove = (ttHit ? move16toMove(tte->move(), pos) :  Move::moveNone());
	ttScore = (ttHit ? scoreFromTT(tte->score(), ss->ply) : ScoreNone);

	if (!PVNode
		&& ttHit
		&& tte->depth() >= ttDepth
		&& ttScore != ScoreNone // アクセス競合が起きたときのみ、ここに引っかかる。
		&& (ttScore >= beta ? (tte->bound() & BoundLower) : (tte->bound() & BoundUpper)))
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
		if ((move = pos.mateMoveIn1Ply()))
			return mateIn(ss->ply);

		if (ttHit) {
			if ((ss->staticEval = bestScore = tte->evalScore()) == ScoreNone)
				ss->staticEval = bestScore = evaluate(pos, ss);
			if (ttScore != ScoreNone)
				if (tte->bound() & (ttScore > bestScore ? BoundLower : BoundUpper))
					bestScore = ttScore;
		}
		else
			ss->staticEval = bestScore = ((ss-1)->currentMove != Move::moveNull() ? evaluate(pos, ss) : -(ss-1)->staticEval + 2 * Tempo);

		if (bestScore >= beta) {
			if (!ttHit)
				tte->save(pos.getKey(), scoreToTT(bestScore, ss->ply), BoundLower,
						  DepthNone, Move::moveNone(), ss->staticEval, tt.generation());

			return bestScore;
		}

		if (PVNode && bestScore > alpha)
			alpha = bestScore;

		futilityBase = bestScore + 128;
    }

	evaluate(pos, ss);

	MovePicker mp(pos, ttMove, depth, (ss-1)->currentMove.to());
	const CheckInfo ci(pos);

	while ((move = mp.nextMove()) != Move::moveNone()) {
		assert(pos.isOK());

		givesCheck = pos.moveGivesCheck(move, ci);

		// futility pruning
		if (!INCHECK // 駒打ちは王手回避のみなので、ここで弾かれる。
			&& !givesCheck
			&& futilityBase > -ScoreKnownWin)
		{
			futilityScore = futilityBase + Position::capturePieceScore(pos.piece(move.to()));
			if (move.isPromotion())
				futilityScore += Position::promotePieceScore(move.pieceTypeFrom());

			if (futilityScore <= alpha) {
				bestScore = std::max(bestScore, futilityScore);
				continue;
			}

			if (futilityBase <= alpha && pos.see(move) <= ScoreZero) {
				bestScore = std::max(bestScore, futilityBase);
				continue;
			}
		}

		evasionPrunable = (INCHECK
						   && bestScore > ScoreMatedInMaxPly
						   && !move.isCaptureOrPawnPromotion());

		if ((!INCHECK || evasionPrunable)
			&& (!move.isPromotion() || move.pieceTypeFrom() != Pawn) // todo: この条件は不要そう。
			&& pos.seeSign(move) < ScoreZero)
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

		if (score > bestScore) {
			bestScore = score;

			if (score > alpha) {
				if (PVNode)
					updatePV(ss->pv, move, (ss+1)->pv);
				if (PVNode && score < beta) {
					alpha = score;
					bestMove = move;
				}
				else { // fail high
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
			  ((PVNode && bestScore > oldAlpha) ? BoundExact : BoundUpper),
			  ttDepth, bestMove, ss->staticEval, tt.generation());

	assert(-ScoreInfinite < bestScore && bestScore < ScoreInfinite);

	return bestScore;
}

void Thread::search() {
	SearchStack stack[MaxPly+7];
	SearchStack* ss = stack + 5; // To allow referencing (ss-5) and (ss+2)
	Score bestScore = -ScoreInfinite;
	Score alpha = -ScoreInfinite;
	Score beta = ScoreInfinite;
	Score delta = -ScoreInfinite;
	Move easyMove = Move::moveNone();
	MainThread* mainThread = (this == searcher->threads.main() ? searcher->threads.main() : nullptr);
	int lastInfoTime = -1; // 将棋所のコンソールが詰まる問題への対処用

	memset(ss-5, 0, 8 * sizeof(SearchStack));
	completedDepth = Depth0;

	if (mainThread) {
		easyMove = searcher->easyMove.get(rootPos.getKey());
		searcher->easyMove.clear();
		mainThread->easyMovePlayed = mainThread->failedLow = false;
		mainThread->bestMoveChanges = 0;
		searcher->tt.newSearch();
	}

	size_t multiPV = searcher->options["MultiPV"];
	Skill skill(SkillLevel, searcher->options["Max_Random_Score_Diff"]);

	if (searcher->options["Max_Random_Score_Diff_Ply"] < rootPos.gamePly()) {
		skill.max_random_score_diff = ScoreZero;
		multiPV = 1;
		assert(!skill.enabled()); // level による設定が出来るようになるまでは、これで良い。
	}

	if (skill.enabled())
		multiPV = std::max(multiPV, (size_t)4);
	multiPV = std::min(multiPV, rootMoves.size());

	// 反復深化で探索を行う。
	while ((rootDepth += OnePly) < DepthMax
		   && !searcher->signals.stop
		   && (!searcher->limits.depth || searcher->threads.main()->rootDepth / OnePly <= searcher->limits.depth))
	{
		if (!mainThread) {
			const Row& row = HalfDensity[(idx - 1) % HalfDensitySize];
			if (row[(rootDepth / OnePly + rootPos.gamePly()) % row.size()])
				continue;
		}

		if (mainThread) {
			mainThread->bestMoveChanges *= 0.505;
			mainThread->failedLow = false;
		}
		for (RootMove& rm : rootMoves)
			rm.previousScore = rm.score;

		for (pvIdx = 0; pvIdx < multiPV && !searcher->signals.stop; ++pvIdx) {
#if defined LEARN
			alpha = searcher->alpha;
			beta  = searcher->beta;
#else
			if (rootDepth >= 5 * OnePly) {
				delta = Score(18);
				alpha = std::max(rootMoves[pvIdx].previousScore - delta, -ScoreInfinite);
				beta  = std::min(rootMoves[pvIdx].previousScore + delta,  ScoreInfinite);
			}
#endif

			while (true) {
				(ss-1)->staticEvalRaw.p[0][0] = ss->staticEvalRaw.p[0][0] = ScoreNotEvaluated;
				bestScore = searcher->search<PV>(rootPos, ss, alpha, beta, rootDepth, false);
				std::stable_sort(std::begin(rootMoves) + pvIdx, std::end(rootMoves));
#if defined LEARN
				break;
#endif
				if (searcher->signals.stop)
					break;

				if (mainThread
					&& multiPV == 1
					&& (bestScore <= alpha || beta <= bestScore)
					&& searcher->timeManager.elapsed() > 3000
					// 将棋所のコンソールが詰まるのを防ぐ。
					&& (rootDepth < 10 * OnePly || lastInfoTime + 200 < searcher->timeManager.elapsed()))
				{
					lastInfoTime = searcher->timeManager.elapsed();
					SYNCCOUT << pvInfoToUSI(rootPos, multiPV, rootDepth, alpha, beta) << SYNCENDL;
				}

				if (bestScore <= alpha) {
					beta = (alpha + beta) / 2;
					alpha = std::max(bestScore - delta, -ScoreInfinite);
					if (mainThread) {
						mainThread->failedLow = true;
						searcher->signals.stopOnPonderHit = false;
					}
				}
				else if (bestScore >= beta) {
					alpha = (alpha + beta) / 2;
					beta = std::min(bestScore + delta, ScoreInfinite);
				}
				else
					break;

				delta += delta / 4 + 5;
				assert(alpha >= -ScoreInfinite && beta <= ScoreInfinite);
			}

			std::stable_sort(std::begin(rootMoves), std::begin(rootMoves) + pvIdx + 1);

			if (!mainThread)
				continue;

			if (searcher->signals.stop)
				SYNCCOUT << "info nodes " << searcher->threads.nodesSearched()
						 << " time " << searcher->timeManager.elapsed() << SYNCENDL;
			else if ((pvIdx + 1 == multiPV || searcher->timeManager.elapsed() > 3000)
					 // 将棋所のコンソールが詰まるのを防ぐ。
					 && (rootDepth < 10 * OnePly || lastInfoTime + 200 < searcher->timeManager.elapsed()))
			{
				lastInfoTime = searcher->timeManager.elapsed();
				SYNCCOUT << pvInfoToUSI(rootPos, multiPV, rootDepth, alpha, beta) << SYNCENDL;
			}
		}

		if (!searcher->signals.stop)
			completedDepth = rootDepth;

		if (!mainThread)
			continue;

		//if (skill.enabled() && skill.timeToPick(rootDepth))
		//	skill.pickMove(this, multiPV);

		if (searcher->limits.mate
			&& bestScore >= ScoreMateInMaxPly
			&& ScoreMate0Ply - bestScore <= 2 * searcher->limits.mate)
		{
			searcher->signals.stop = true;
		}

		if (searcher->limits.useTimeManagement()) {
			if (!searcher->signals.stop && !searcher->signals.stopOnPonderHit) {
				const int F[] = { mainThread->failedLow,
								  bestScore - mainThread->previousScore };

				const int improvingFactor = std::max(229, std::min(715, 357 + 119 * F[0] - 6 * F[1]));
				const double unstablePvFactor = 1 + mainThread->bestMoveChanges;

				const bool doEasyMove = (rootMoves[0].pv[0] == easyMove
										 && mainThread->bestMoveChanges < 0.03
										 && searcher->timeManager.elapsed() > searcher->timeManager.optimum() * 5 / 42);

				if (rootMoves.size() == 1
					|| searcher->timeManager.elapsed() > searcher->timeManager.optimum() * unstablePvFactor * improvingFactor / 628
					|| (mainThread->easyMovePlayed = doEasyMove, doEasyMove)) // MSVC で warning 出ないようにするハック
				{
					if (searcher->limits.ponder)
						searcher->signals.stopOnPonderHit = true;
					else
						searcher->signals.stop = true;
				}
			}

			if (rootMoves[0].pv.size() >= 3)
				searcher->easyMove.update(rootPos, rootMoves[0].pv);
			else
				searcher->easyMove.clear();
		}
	}

	if (!mainThread)
		return;

	if (searcher->easyMove.stableCount < 6 || mainThread->easyMovePlayed)
		searcher->easyMove.clear();

	skill.swapIfEnabled(this, multiPV);
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
	const bool PVNode = (NT == PV);
	const bool RootNode = PVNode && (ss-1)->ply == 0;

	assert(-ScoreInfinite <= alpha && alpha < beta && beta <= ScoreInfinite);
	assert(PVNode || (alpha == beta - 1));
	assert(Depth0 < depth && depth < DepthMax);
	assert(!(PVNode && cutNode));
	assert(depth / OnePly * OnePly == depth);

	// goto と ラベルの間で変数を定義してはいけないので、関数の最初で変数をまとめて定義しておく。
	Move pv[MaxPly+1], quietsSearched[64];
	StateInfo st;
	TTEntry* tte;
	Key posKey;
	Move ttMove, move, excludedMove, bestMove;
	Depth extension, newDepth;
	Score bestScore, score, ttScore, eval;
	bool ttHit, inCheck, givesCheck, singularExtensionNode, improving;
	bool captureOrPawnPromotion, doFullDepthSearch, moveCountPruning;
	Piece movedPiece;
	int moveCount, quietCount;

	// step1
	// initialize node
	Thread* thisThread = pos.thisThread();
	inCheck = pos.inCheck();
	moveCount = quietCount = ss->moveCount = 0;
	bestScore = -ScoreInfinite;
	ss->ply = (ss-1)->ply + 1;

	if (thisThread->resetCalls.load(std::memory_order_relaxed)) {
		thisThread->resetCalls = false;
		thisThread->callsCnt = 0;
	}
	if (++thisThread->callsCnt > 4096) {
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
		alpha = std::max(matedIn(ss->ply), alpha);
		beta  = std::min(mateIn(ss->ply+1), beta);
		if (alpha >= beta)
			return alpha;
	}
	assert(0 <= ss->ply && ss->ply < MaxPly);
	ss->currentMove = (ss+1)->excludedMove = bestMove = Move::moveNone();
	ss->counterMoves = nullptr;
	(ss+1)->skipEarlyPruning = false;
	(ss+2)->killers[0] = (ss+2)->killers[1] = Move::moveNone();

	pos.setNodesSearched(pos.nodesSearched() + 1);

	// step4
	// trans position table lookup
	excludedMove = ss->excludedMove;
	posKey = (!excludedMove ? pos.getKey() : pos.getExclusionKey());
	tte = tt.probe(posKey, ttHit);
	ttScore = ttHit ? scoreFromTT(tte->score(), ss->ply) : ScoreNone;
	ttMove = (RootNode ? thisThread->rootMoves[thisThread->pvIdx].pv[0] :
			  ttHit    ? move16toMove(tte->move(), pos) : Move::moveNone());

	if (!PVNode
		&& ttHit
		&& tte->depth() >= depth
		&& ttScore != ScoreNone
		&& (ttScore >= beta ? (tte->bound() & BoundLower) : (tte->bound() & BoundUpper)))
	{
		ss->currentMove = ttMove;
		if (ttScore >= beta && ttMove) {
			const int d = depth / OnePly;
			if (!ttMove.isCaptureOrPawnPromotion()) {
				const Score bonus = Score(d * d + 2 * d - 2);
				updateStats(pos, ss, ttMove, nullptr, 0, bonus);
			}
			// Extra penalty for a quiet TT move in previous ply when it gets refuted
			if ((ss-1)->moveCount == 1 && !(ss-1)->currentMove.isCaptureOrPawnPromotion()) {
				const Score penalty = Score(d * d + 4 * d + 1);
				const Square prevSq = (ss-1)->currentMove.to();
				updateCMStats(ss-1, pos.piece(prevSq), prevSq, -penalty);
			}
		}
		return ttScore;
	}

#if 1
	if (!RootNode
		&& !inCheck)
	{
		if ((move = pos.mateMoveIn1Ply())) {
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
	eval = ss->staticEval = evaluate(pos, ss); // 評価関数の差分計算の為に、常に評価関数を呼ぶ。
	if (inCheck) {
		ss->staticEval = eval = ScoreNone;
		goto movesLoop;
	}
	else if (ttHit) {
		if (ttScore != ScoreNone)
			if (tte->bound() & (ttScore > eval ? BoundLower : BoundUpper))
				eval = ttScore;
	}
	else {
#if 1 // 必要？
		if ((ss-1)->currentMove == Move::moveNull())
			eval = ss->staticEval = -(ss-1)->staticEval + 2 * Tempo;
#endif
		tte->save(posKey, ScoreNone, BoundNone, DepthNone,
				  Move::moveNone(), ss->staticEval, tt.generation());
	}

	if (ss->skipEarlyPruning)
		goto movesLoop;

	// step6
	// razoring
	if (!PVNode
		&& depth < 4 * OnePly
		&& ttMove == Move::moveNone()
		&& eval + RazorMargin[depth / OnePly] <= alpha)
	{
		if (depth <= OnePly)
			return qsearch<NonPV, false>(pos, ss, alpha, beta, Depth0);
		const Score ralpha = alpha - RazorMargin[depth / OnePly];
		const Score s = qsearch<NonPV, false>(pos, ss, ralpha, ralpha+1, Depth0);
		if (s <= ralpha)
			return s;
	}

	// step7
	// futility pruning
	if (!RootNode
		&& depth < 7 * OnePly
		&& eval - futilityMargin(depth) >= beta
		&& eval < ScoreKnownWin) // todo: non_pawn_material に相当する条件を付けるべきか？
	{
		return eval - futilityMargin(depth);
	}

	// step8
	// null move
	if (!PVNode
		&& eval >= beta
		&& (ss->staticEval >= beta - 35 * (depth / OnePly - 6) || depth >= 13 * OnePly))
	{
		ss->currentMove = Move::moveNull();
		ss->counterMoves = nullptr;

		assert(eval - beta >= 0);

		const Depth R = ((823 + 67 * depth / OnePly) / 256 + std::min((eval - beta) / PawnScore, 3)) * OnePly;

		pos.doNullMove<true>(st);
		(ss+1)->skipEarlyPruning = true;
		(ss+1)->staticEvalRaw = (ss)->staticEvalRaw; // 評価値の差分評価の為。
		Score nullScore = (depth-R < OnePly ?
						   -qsearch<NonPV, false>(pos, ss+1, -beta, -beta+1, Depth0)
						   : -search<NonPV>(pos, ss+1, -beta, -beta+1, depth-R, !cutNode));
		(ss+1)->skipEarlyPruning = false;
		pos.doNullMove<false>(st);

		if (nullScore >= beta) {
			if (nullScore >= ScoreMateInMaxPly)
				nullScore = beta;

			if (depth < 12 * OnePly && abs(beta) < ScoreKnownWin)
				return nullScore;

			ss->skipEarlyPruning = true;
			const Score s = (depth-R < OnePly ?
							 qsearch<NonPV, false>(pos, ss, beta-1, beta, Depth0)
							 : search<NonPV>(pos, ss, beta-1, beta, depth-R, false));
			ss->skipEarlyPruning = false;

			if (s >= beta)
				return nullScore;
		}
	}

	// step9
	// probcut
	if (!PVNode
		&& depth >= 5 * OnePly
		&& abs(beta) < ScoreMateInMaxPly)
	{
		const Score rbeta = std::min(beta + 200, ScoreInfinite);
		const Depth rdepth = depth - 4 * OnePly;

		assert(rdepth >= OnePly);
		assert((ss-1)->currentMove != Move::moveNone());
		assert((ss-1)->currentMove != Move::moveNull());

		MovePicker mp(pos, ttMove, rbeta - ss->staticEval);
		const CheckInfo ci(pos);
		while ((move = mp.nextMove()) != Move::moveNone()) {
			if (pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned)) {
				ss->currentMove = move;
				ss->counterMoves = &thisThread->counterMoveHistory[pos.movedPiece(move)][move.to()];
				pos.doMove(move, st, ci, pos.moveGivesCheck(move, ci));
				(ss+1)->staticEvalRaw.p[0][0] = ScoreNotEvaluated;
				score = -search<NonPV>(pos, ss+1, -rbeta, -rbeta+1, rdepth, !cutNode);
				pos.undoMove(move);
				if (score >= rbeta)
					return score;
			}
		}
	}

	// step10
	// internal iterative deepening
	if (depth >= 6 * OnePly
		&& !ttMove
		&& (PVNode || ss->staticEval + 256 >= beta))
	{
		const Depth d = (3 * depth / (4 * OnePly) - 2) * OnePly;
		ss->skipEarlyPruning = true;
		search<NT>(pos, ss, alpha, beta, d, cutNode);
		ss->skipEarlyPruning = false;

		tte = tt.probe(posKey, ttHit);
		ttMove = (ttHit ? move16toMove(tte->move(), pos) : Move::moveNone());
	}

movesLoop:

	const CounterMoveStats* cmh  = (ss-1)->counterMoves;
	const CounterMoveStats* fmh  = (ss-2)->counterMoves;
	const CounterMoveStats* fmh2 = (ss-4)->counterMoves;

	MovePicker mp(pos, ttMove, depth, ss);
	const CheckInfo ci(pos);
	score = bestScore;
	improving = (ss->staticEval >= (ss-2)->staticEval
				 //|| ss->staticEval == ScoreNone
				 ||(ss-2)->staticEval == ScoreNone);

	singularExtensionNode = (!RootNode
							 &&  depth >= 8 * OnePly
							 &&  ttMove != Move::moveNone()
							 //&&  ttScore != ScoreNone
							 &&  abs(ttScore) < ScoreKnownWin
							 && !excludedMove
							 && (tte->bound() & BoundLower)
							 &&  tte->depth() >= depth - 3 * OnePly);

	// step11
	// Loop through moves
    while ((move = mp.nextMove()) != Move::moveNone()) {
		if (move == excludedMove)
			continue;

		if (RootNode && std::find(std::begin(thisThread->rootMoves) + thisThread->pvIdx,
								  std::end(thisThread->rootMoves), move) == std::end(thisThread->rootMoves))
			continue;

		ss->moveCount = ++moveCount;

		if (PVNode)
			(ss+1)->pv = nullptr;

		extension = Depth0;
		captureOrPawnPromotion = move.isCaptureOrPawnPromotion();
		movedPiece = pos.movedPiece(move);

		givesCheck = pos.moveGivesCheck(move, ci);

		moveCountPruning = (depth < 16 * OnePly
							&& moveCount >= FutilityMoveCounts[improving][depth / OnePly]);
		// step12
		if (givesCheck
			&& !moveCountPruning
			&& pos.seeSign(move) >= ScoreZero)
		{
			extension = OnePly;
		}

		// singuler extension
		if (singularExtensionNode
			&& move == ttMove
			&& extension == Depth0
			&& pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned))
		{
			const Score rBeta = ttScore - 2 * depth / OnePly;
			const Depth d = (depth / (2 * OnePly)) * OnePly;
			ss->excludedMove = move;
			ss->skipEarlyPruning = true;
			score = search<NonPV>(pos, ss, rBeta - 1, rBeta, d, cutNode);
			ss->skipEarlyPruning = false;
			ss->excludedMove = Move::moveNone();

			if (score < rBeta)
				extension = OnePly;
		}

		newDepth = depth - OnePly + extension;

		// step13
		// pruning at shallow depth
		if (!RootNode
			&& !inCheck
			&& bestScore > ScoreMatedInMaxPly)
		{
			if (!captureOrPawnPromotion
				&& !givesCheck)
			{
				// move count based pruning
				if (moveCountPruning)
					continue;

				const int lmrDepth = std::max(newDepth - reduction<PVNode>(improving, depth, moveCount), Depth0) / OnePly;

				// countermoves based pruning
				if (lmrDepth < 3
					&& (!cmh  || (*cmh )[movedPiece][move.to()] < ScoreZero)
					&& (!fmh  || (*fmh )[movedPiece][move.to()] < ScoreZero)
					&& (!fmh2 || (*fmh2)[movedPiece][move.to()] < ScoreZero || (cmh && fmh)))
					continue;

				// futility pruning: parent node
				if (lmrDepth < 7
					&& ss->staticEval + 256 + 200 * lmrDepth <= alpha)
					continue;

				// Prune moves with negative SEE
				if (lmrDepth < 8
					&& pos.seeSign(move) < Score(-35 * lmrDepth * lmrDepth))
					continue;
			}
			else if (depth < 7 * OnePly
					 && pos.seeSign(move) < Score(-35 * depth / OnePly * depth / OnePly))
				continue;
		}

		// RootNode はすでに合法手であることを確認済み。
		if (!RootNode && !pos.pseudoLegalMoveIsLegal<false, false>(move, ci.pinned)) {
			ss->moveCount = --moveCount;
			continue;
		}

		ss->currentMove = move;
		ss->counterMoves = &thisThread->counterMoveHistory[movedPiece][move.to()];

		// step14
		pos.doMove(move, st, ci, givesCheck);
		(ss+1)->staticEvalRaw.p[0][0] = ScoreNotEvaluated;

		// step15
		// LMR
		if (depth >= 3 * OnePly
			&& moveCount > 1
			//&& !(pos.gamePly() > 70 // todo: 進行度などに変えた方が良いだろう。
			//	 && thisThread == pos.searcher()->threads.back()
			//	 && st.continuousCheck[oppositeColor(pos.turn())] >= 4) // !(70手以上 && mainThread && 連続王手)
			&& (!captureOrPawnPromotion || moveCountPruning))
		{
			Depth r = reduction<PVNode>(improving, depth, moveCount);

			if (captureOrPawnPromotion)
				r -= (r ? OnePly : Depth0);
			else {
				if (cutNode)
					r += 2 * OnePly;
#if 0
				else if (!move.isDrop()
						 && pieceToPieceType(pos.piece(move.to())) != Pawn
						 && pos.seeSign(makeMove(pieceToPieceType(pos.piece(move.to())), move.to(), move.from())) < ScoreZero)
				{
					r -= 2 * OnePly;
				}
#endif

				const Score val = thisThread->history[movedPiece][move.to()]
					+    (cmh  ? (*cmh )[movedPiece][move.to()] : ScoreZero)
					+    (fmh  ? (*fmh )[movedPiece][move.to()] : ScoreZero)
					+    (fmh2 ? (*fmh2)[movedPiece][move.to()] : ScoreZero)
					+    thisThread->fromTo.get(oppositeColor(pos.turn()), move);
				const int rHist = (val - 8000) / 20000;
				r = std::max(Depth0, (r / OnePly - rHist) * OnePly);
			}

			const Depth d = std::max(newDepth - r, OnePly);

			// PVS
			score = -search<NonPV>(pos, ss+1, -(alpha+1), -alpha, d, true);

			doFullDepthSearch = (score > alpha && d != newDepth);
		}
		else
			doFullDepthSearch = !PVNode || moveCount > 1;

		// step16
		// full depth search
		// PVS
		if (doFullDepthSearch)
			score = (newDepth < OnePly ?
					 (givesCheck ? -qsearch<NonPV, true>(pos, ss+1, -(alpha + 1), -alpha, Depth0)
					  : -qsearch<NonPV, false>(pos, ss+1, -(alpha + 1), -alpha, Depth0))
					 : -search<NonPV>(pos, ss+1, -(alpha + 1), -alpha, newDepth, !cutNode));

		if (PVNode && (moveCount == 1 || (score > alpha && (RootNode || score < beta)))) {
			(ss+1)->pv = pv;
			(ss+1)->pv[0] = Move::moveNone();

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
			RootMove& rm = *std::find(std::begin(thisThread->rootMoves), std::end(thisThread->rootMoves), move);
			if (moveCount == 1 || score > alpha) {
				rm.score = score;
				rm.pv.resize(1);
				//rm.extractPvFromTT(pos);

				assert((ss+1)->pv);

				for (Move* m = (ss+1)->pv; *m != Move::moveNone(); ++m)
					rm.pv.push_back(*m);

				if (moveCount > 1 && thisThread == threads.main())
					++static_cast<MainThread*>(thisThread)->bestMoveChanges;
			}
			else
				rm.score = -ScoreInfinite;
		}

		if (score > bestScore) {
			bestScore = score;
			if (score > alpha) {
				if (PVNode
					&& thisThread == threads.main()
					&& easyMove.get(pos.getKey())
					&& (move != easyMove.get(pos.getKey()) || moveCount > 1))
				{
					easyMove.clear();
				}
				bestMove = move;

				if (PVNode && !RootNode)
					updatePV(ss->pv, move, (ss+1)->pv);

				if (PVNode && score < beta)
					alpha = score;
				else {
					assert(score >= beta);
					break;
				}
			}
		}

		if (!captureOrPawnPromotion && move != bestMove && quietCount < 64)
			quietsSearched[quietCount++] = move;
	}

	// step20
	if (moveCount == 0)
		bestScore = (excludedMove ? alpha : matedIn(ss->ply));
	else if (bestMove) {
		const int d = depth / OnePly;

		if (!bestMove.isCaptureOrPawnPromotion()) {
			const Score bonus = Score(d * d + 2 * d - 2);
			updateStats(pos, ss, bestMove, quietsSearched, quietCount, bonus);
		}

		if ((ss-1)->moveCount == 1 && (ss-1)->currentMove.isCaptureOrPawnPromotion()) {
			const Score penalty = Score(d * d + 4 * d + 1);
			const Square prevSq = (ss-1)->currentMove.to();
			updateCMStats(ss-1, pos.piece(prevSq), prevSq, -penalty);
		}
	}
	else if(depth >= 3 * OnePly
			&& !(ss-1)->currentMove.isCaptureOrPromotion()
			&& (ss-1)->currentMove.isOK())
	{
		const int d = depth / OnePly;
		const Score bonus = Score(d * d + 2 * d - 2);
		const Square prevSq = (ss-1)->currentMove.to();
		updateCMStats(ss-1, pos.piece(prevSq), prevSq, bonus);
	}

	tte->save(posKey, scoreToTT(bestScore, ss->ply),
			  (bestScore >= beta ? BoundLower :
			   PVNode && bestMove ? BoundExact : BoundUpper),
			  depth, bestMove, ss->staticEval, tt.generation());

	assert(-ScoreInfinite < bestScore && bestScore < ScoreInfinite);

	return bestScore;
}

bool RootMove::extractPonderFromTT(Position& pos) {
	StateInfo st;
	bool ttHit;

	assert(pv.size() == 1);

	pos.doMove(pv[0], st/*, pos.moveGivesCheck(pv[0])*/);
	TTEntry* tte = pos.csearcher()->tt.probe(pos.getKey(), ttHit);

	if (ttHit) {
		const Move m = tte->move();
		if (MoveList<Legal>(pos).contains(m))
			pv.push_back(m);
	}

	pos.undoMove(pv[0]);
	return pv.size() > 1;
}

void initSearchTable() {
	for (int improving = 0; improving < 2; ++improving) {
		for (int d = 1; d < 64; d++) {
			for (int mc = 1; mc < 64; mc++) {
				const double r = log(d) * log(mc) / 2;
				if (r < 0.80)
					continue;

				Reductions[NonPV][improving][d][mc] = int(std::round(r));
				Reductions[PV   ][improving][d][mc] = std::max(Reductions[NonPV][improving][d][mc] - 1, 0);

				if (!improving && Reductions[NonPV][improving][d][mc] >= 2)
					Reductions[NonPV][improving][d][mc]++;
			}
		}
	}

	for (int d = 0; d < 16; ++d) {
		FutilityMoveCounts[0][d] = int(2.4 + 0.773 * pow(d + 0.00, 1.8));
		FutilityMoveCounts[1][d] = int(2.9 + 1.045 * pow(d + 0.49, 1.8));
	}
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
	if (!pos.bbOf(King, us).andIsAny(opponentsField))
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
	auto& options = searcher->options;
	auto& tt = searcher->tt;
	auto& signals = searcher->signals;

	static Book book;
	Position& pos = rootPos;
	const Color us = pos.turn();
	searcher->timeManager.init(searcher->limits, us, pos.gamePly(), searcher);
	std::uniform_int_distribution<int> dist(options["Min_Book_Ply"], options["Max_Book_Ply"]);
	const Ply book_ply = dist(g_randomTimeSeed);
	bool searched = false;

	bool nyugyokuWin = false;
	if (nyugyoku(pos)) {
		nyugyokuWin = true;
		goto finalize;
	}
	pos.setNodesSearched(0);

	SYNCCOUT << "info string book_ply " << book_ply << SYNCENDL;
	if (options["OwnBook"] && pos.gamePly() <= book_ply) {
		const std::tuple<Move, Score> bookMoveScore = book.probe(pos, options["Book_File"], options["Best_Book_Move"]);
		if (std::get<0>(bookMoveScore) && std::find(rootMoves.begin(),
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
					return rm.pv[0].toCSA() == str;
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
	if (rootMoves.empty()) {
		rootMoves.push_back(RootMove(Move::moveNone()));
		SYNCCOUT << "info depth 0 score "
				 << scoreToUSI(-ScoreMate0Ply)
				 << SYNCENDL;
	}
	else {
		for (Thread* th : searcher->threads)
			if (th != this)
				th->startSearching();

		Thread::search();
		searched = true;
	}

finalize:

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
		&& !searcher->limits.depth
		&& !Skill(SkillLevel, searcher->options["Max_Random_Score_Diff"]).enabled()
		&& rootMoves[0].pv[0] != Move::moveNone())
	{
		for (Thread* th : searcher->threads)
			if (th->completedDepth > bestThread->completedDepth
				&& th->rootMoves[0].score > bestThread->rootMoves[0].score)
			{
				bestThread = th;
			}
	}

	previousScore = bestThread->rootMoves[0].score;

	if (bestThread != this)
		SYNCCOUT << pvInfoToUSI(bestThread->rootPos, 1, bestThread->completedDepth, -ScoreInfinite, ScoreInfinite) << SYNCENDL;

	if (nyugyokuWin)
		SYNCCOUT << "bestmove win" << SYNCENDL;
	else if (!bestThread->rootMoves[0].pv[0])
		SYNCCOUT << "bestmove resign" << SYNCENDL;
	else
		SYNCCOUT << "bestmove " << bestThread->rootMoves[0].pv[0].toUSI()
				 << " ponder " << bestThread->rootMoves[0].pv[1].toUSI()
				 << SYNCENDL;
#endif
}

void Searcher::checkTime() {
	if (limits.ponder)
		return;

	const auto elapsed = timeManager.elapsed();
	if ((limits.useTimeManagement() && elapsed > timeManager.maximum() - 10)
		|| (limits.moveTime && elapsed >= limits.moveTime)
		|| (limits.nodes && threads.nodesSearched() >= limits.nodes))
	{
		signals.stop = true;
	}
}
