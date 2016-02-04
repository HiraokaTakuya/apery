#ifndef APERY_THREAD_HPP
#define APERY_THREAD_HPP

#include "common.hpp"
#include "evaluate.hpp"
#include "usi.hpp"
#include "tt.hpp"

const int MaxThreads = 64;

enum NodeType {
	Root, PV, NonPV
};

// 時間や探索深さの制限を格納する為の構造体
struct LimitsType {
	LimitsType() : searchmoves(), movesToGo(), depth(), nodes(), moveTime(), infinite(), ponder() {
		time[Black] = time[White] = increment[Black] = increment[White] = 0;
	}
	bool useTimeManagement() const { return !(depth | nodes | moveTime | static_cast<int>(infinite)); }

	std::vector<Move> searchmoves;
	int time[ColorNum];
	int increment[ColorNum];
	int movesToGo;
	Ply depth;
	u64 nodes;
	int moveTime;
	bool infinite;
	bool ponder;
	Timer startTime;
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
		if (Gain)
			table_[isDrop][pc][to] = std::max(s, value(isDrop, pc, to) - 1);
		else if (abs(value(isDrop, pc, to) + s) < MaxScore)
			table_[isDrop][pc][to] += s;
	}

private:
	// [isDrop][piece][square] とする。
	Score table_[2][PieceNone][SquareNum];
};

using History = Stats<false>;
using Gains   = Stats<true>;

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
	void insertPvInTT(Position& pos);

public:
	Score score_;
	Score prevScore_;
	std::vector<Move> pv_;
};

struct Thread {
	explicit Thread(Searcher* s);
	virtual ~Thread();
	virtual void search();
	void idleLoop();
	void startSearching(const bool resume = false);
	void waitForSearchFinished();
	void wait(std::atomic_bool& condition);

    Searcher* searcher;
	Position* activePosition;
	int idx;
	size_t pvIdx;
	int maxPly;
	int callsCount;

	Position rootPosition;
	std::vector<RootMove> rootMoves;
	Ply rootDepth; // depth にしたい。
	History history;
	Gains gains;
	Ply completedDepth; // depth にしたい。
	std::atomic_bool resetCalls;

private:
	Mutex mutex;
	ConditionVariable sleepCondition;
	std::thread nativeThread;
	bool exit;
	bool searching;
};

struct MainThread : public Thread {
	explicit MainThread(Searcher* s) : Thread(s) {}
	virtual void search();

	bool easyMovePlayed;
	bool failedLow;
	double bestMoveChanges;
	Score previousScore;
};

struct ThreadPool : public std::vector<Thread*> {
	void init(Searcher* s);
	void exit();

	MainThread* mainThread() { return static_cast<MainThread*>((*this)[0]); }
	void startThinking(const Position& pos, const LimitsType& limits, StateStackPtr& states);
	void readUSIOptions(Searcher* s);
	u64 nodesSearched() const;
};

#endif // #ifndef APERY_THREAD_HPP
