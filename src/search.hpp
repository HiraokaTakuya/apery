#ifndef APERY_SEARCH_HPP
#define APERY_SEARCH_HPP

#include "move.hpp"
#include "pieceScore.hpp"
#include "timeManager.hpp"
#include "tt.hpp"
#include "thread.hpp"

class Position;
struct SplitPoint;

struct SearchStack {
	SplitPoint* splitPoint;
	Ply ply;
	Move currentMove;
	Move excludedMove; // todo: これは必要？
	Move killers[2];
	Depth reduction;
	Score staticEval;
	bool skipNullMove;
	EvalSum staticEvalRaw; // 評価関数の差分計算用、値が入っていないときは [0] を ScoreNotEvaluated にしておく。
						   // 常に Black 側から見た評価値を入れておく。
						   // 0: 双玉に対する評価値, 1: 先手玉に対する評価値, 2: 後手玉に対する評価値
};

struct SignalsType {
	std::atomic_bool stop;
	std::atomic_bool stopOnPonderHit;
};

enum InaniwaFlag {
	NotInaniwa,
	InaniwaIsBlack,
	InaniwaIsWhite,
	InaniwaFlagNum
};

enum BishopInDangerFlag {
	NotBishopInDanger,
	BlackBishopInDangerIn28,
	WhiteBishopInDangerIn28,
	BlackBishopInDangerIn78,
	WhiteBishopInDangerIn78,
	BlackBishopInDangerIn38,
	WhiteBishopInDangerIn38,
	BishopInDangerFlagNum
};

struct EasyMoveManager {
	void clear() {
		stableCount = 0;
		expectedPosKey = 0;
		pv[0] = pv[1] = pv[2] = Move::moveNone();
	}

	Move get(Key key) const {
		return expectedPosKey == key ? pv[2] : Move::moveNone();
	}

	void update(Position& pos, const std::vector<Move>& newPv) {
		assert(newPv.size() >= 3);
		stableCount = (newPv[2] == pv[2]) ? stableCount + 1 : 0;
		if (!std::equal(std::begin(newPv), std::begin(newPv) + 3, pv)) {
			std::copy(std::begin(newPv), std::begin(newPv) + 3, pv);
			StateInfo st[2];
			pos.doMove(newPv[0], st[0]);
			pos.doMove(newPv[1], st[1]);
			expectedPosKey = pos.getKey();
			pos.undoMove(newPv[1]);
			pos.undoMove(newPv[0]);
		}
	}

	int stableCount;
	Key expectedPosKey;
	Move pv[3];
};

class TranspositionTable;

struct Searcher {
	// static メンバ関数からだとthis呼べないので代わりに thisptr を使う。
	// static じゃないときは this を入れることにする。
	STATIC Searcher* thisptr;
	STATIC SignalsType signals;
	STATIC LimitsType limits;
	STATIC std::vector<Move> searchMoves;
	STATIC StateStackPtr setUpStates;
	STATIC StateStackPtr usiSetUpStates;

#if defined LEARN
	STATIC Score alpha;
	STATIC Score beta;
#endif

	STATIC TimeManager timeManager;
	STATIC TranspositionTable tt;

#if defined INANIWA_SHIFT
	STATIC InaniwaFlag inaniwaFlag;
#endif
	STATIC ThreadPool threads;
	STATIC OptionsMap options;
	STATIC EasyMoveManager easyMove;

	STATIC void init();
	STATIC void clear();
	template <NodeType NT, bool INCHECK>
	STATIC Score qsearch(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth);
#if defined INANIWA_SHIFT
	STATIC void detectInaniwa(const Position& pos);
#endif
	template <NodeType NT>
	STATIC Score search(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth, const bool cutNode);
	STATIC void think();
	STATIC void checkTime();

	STATIC void doUSICommandLoop(int argc, char* argv[]);
	STATIC void setOption(std::istringstream& ssCmd);
};

void initSearchTable();

#endif // #ifndef APERY_SEARCH_HPP
