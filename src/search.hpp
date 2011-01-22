#ifndef SEARCH_HPP
#define SEARCH_HPP

#include "move.hpp"
#include "pieceScore.hpp"
#include "timeManager.hpp"
#include "tt.hpp"

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
	Score staticEvalRaw; // 評価関数の差分計算用、値が入っていないときは INT_MAX にしておく。
						 // 常に Black の評価値を入れておく。
};

enum NodeType {
	Root, PV, NonPV, SplitPointRoot, SplitPointPV, SplitPointNonPV
};

// 時間や探索深さの制限を格納する為の構造体
struct LimitsType {
	LimitsType() { memset(this, 0, sizeof(LimitsType)); }
	bool useTimeManagement() const { return !(depth | nodes | moveTime | static_cast<int>(infinite)); }

	int time[ColorNum];
	int increment[ColorNum];
	int movesToGo;
	Ply depth;
	u32 nodes;
	int moveTime;
	bool infinite;
	bool ponder;
};

struct SignalsType {
	bool stopOnPonderHit;
	bool firstRootMove;
	bool stop;
	bool failedLowAtRoot;
};

enum InaniwaFlag {
	NotInaniwa,
	InaniwaIsBlack,
	InaniwaIsWhite,
	InaniwaFlagNum
};

class RootMove {
public:
	RootMove() {}
	explicit RootMove(const Move m) : score_(-ScoreInfinite), prevScore_(-ScoreInfinite) {
		pv_.push_back(m);
		pv_.push_back(Move::moveNone());
	}
	explicit RootMove(const std::tuple<Move, Score> m) : score_(std::get<1>(m)), prevScore_(-ScoreInfinite) {
		pv_.push_back(std::get<0>(m));
		pv_.push_back(Move::moveNone());
	}

	bool operator < (const RootMove& m) const {
		return score_ < m.score_;
	}
	bool operator == (const Move& m) const {
		return pv_[0] == m;
	}

	void extractPvFromTT(Position& pos);
	void insertPvInTT(Position& pos, SearchStack* ss);

public:
	Score score_;
	Score prevScore_;
	std::vector<Move> pv_;
};

template <bool Gain>
class Stats {
public:
	static const Score MaxScore = static_cast<Score>(2000);

	void clear() { memset(table_, 0, sizeof(table_)); }
	Score value(const bool isDrop, const Piece pc, const Square to) const {
		assert(0 < pc && pc < PieceNone);
		assert(isInSquare(to));
		return table_[isDrop][pc][to];
	}
	void update(const bool isDrop, const Piece pc, const Square to, const Score s) {
		if (Gain) {
			table_[isDrop][pc][to] = std::max(s, value(isDrop, pc, to) - 1);
		}
		else if (abs(value(isDrop, pc, to) + s) < MaxScore) {
			table_[isDrop][pc][to] += s;
		}
	}

private:
	// [isDrop][piece][square] とする。
	Score table_[2][PieceNone][SquareNum];
};

typedef Stats<false> History;
typedef Stats<true>  Gains;

class TranspositionTable;

struct Searcher {
	static volatile SignalsType signals;
	static LimitsType limits;
	static std::vector<Move> searchMoves;
	static Time searchTimer;
	static StateStackPtr setUpStates;
	static std::vector<RootMove> rootMoves;

	static size_t pvSize;
	static size_t pvIdx;
	static TimeManager timeManager;
	static Ply bestMoveChanges;
	static History history;
	static Gains gains;
	static TranspositionTable tt;

	static void idLoop(Position& pos);
	static std::string pvInfoToUSI(Position& pos, const Ply depth, const Score alpha, const Score beta);
	template <NodeType NT, bool INCHECK>
	static Score qsearch(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth);
#if defined INANIWA_SHIFT
	static void detectInaniwa(const Position& pos);
#endif
	template <NodeType NT>
	static Score search(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth, const bool cutNode);
	static void think();
};
#if defined INANIWA_SHIFT
extern InaniwaFlag g_inaniwaFlag;
#endif
extern Position g_rootPosition;

extern bool g_inaniwaGame;

void initSearchTable();

#endif // #ifndef SEARCH_HPP
