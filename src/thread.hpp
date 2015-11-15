#ifndef APERY_THREAD_HPP
#define APERY_THREAD_HPP

#include "common.hpp"
#include "evaluate.hpp"
#include "usi.hpp"
#include "tt.hpp"

const int MaxThreads = 64;
const int MaxSplitPointsPerThread = 8;

struct Thread;
struct SearchStack;
class MovePicker;

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

struct SplitPoint {
	const Position* pos;
	const SearchStack* ss;
	Thread* masterThread;
	Depth depth;
	Score beta;
	NodeType nodeType;
	Move threatMove;
	bool cutNode;

	MovePicker* movePicker;
	SplitPoint* parentSplitPoint;

	Mutex mutex;
	volatile u64 slavesMask;
	volatile s64 nodes;
	volatile Score alpha;
	volatile Score bestScore;
	volatile Move bestMove;
	volatile int moveCount;
	volatile bool cutoff;
};

struct Thread {
	explicit Thread(Searcher* s);
	virtual ~Thread() {};

	virtual void idleLoop();
	void notifyOne();
	bool cutoffOccurred() const;
	bool isAvailableTo(Thread* master) const;
	void waitFor(volatile const bool& b);

	template <bool Fake>
	void split(Position& pos, SearchStack* ss, const Score alpha, const Score beta, Score& bestScore,
			   Move& bestMove, const Depth depth, const Move threatMove, const int moveCount,
			   MovePicker& mp, const NodeType nodeType, const bool cutNode);

	SplitPoint splitPoints[MaxSplitPointsPerThread];
	Position* activePosition;
	int idx;
	int maxPly;
	Mutex sleepLock;
	ConditionVariable sleepCond;
	std::thread handle;
	SplitPoint* volatile activeSplitPoint;
	volatile int splitPointsSize;
	volatile bool searching;
	volatile bool exit;
    Searcher* searcher;
};

struct MainThread : public Thread {
	explicit MainThread(Searcher* s) : Thread(s), thinking(true) {}
	virtual void idleLoop();
	volatile bool thinking;
};

struct TimerThread : public Thread {
	explicit TimerThread(Searcher* s) : Thread(s), msec(0) {}
	virtual void idleLoop();
	int msec;
};

class ThreadPool : public std::vector<Thread*> {
public:
	void init(Searcher* s);
	void exit();

	MainThread* mainThread() { return static_cast<MainThread*>((*this)[0]); }
	Depth minSplitDepth() const { return minimumSplitDepth_; }
	TimerThread* timerThread() { return timer_; }
	void wakeUp(Searcher* s);
	void sleep();
	void readUSIOptions(Searcher* s);
	Thread* availableSlave(Thread* master) const;
	void setTimer(const int msec);
	void waitForThinkFinished();
	void startThinking(const Position& pos, const LimitsType& limits,
					   const std::vector<Move>& searchMoves);

	bool sleepWhileIdle_;
	size_t maxThreadsPerSplitPoint_;
	Mutex mutex_;
	ConditionVariable sleepCond_;

private:
	TimerThread* timer_;
	Depth minimumSplitDepth_;
};

#endif // #ifndef APERY_THREAD_HPP
