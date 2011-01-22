#ifndef THREAD_HPP
#define THREAD_HPP

#include "common.hpp"
#include "movePicker.hpp"
#include "search.hpp"
#include "evaluate.hpp"
#include "usi.hpp"

const int MaxThreads = 64;
const int MaxSplitPointsPerThread = 8;

struct Thread;

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

	std::mutex mutex;
	volatile u64 slavesMask;
	volatile s64 nodes;
	volatile Score alpha;
	volatile Score bestScore;
	volatile Move bestMove;
	volatile int moveCount;
	volatile bool cutoff;
};

struct Thread {
	Thread();
	virtual ~Thread();

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
	std::mutex sleepLock;
	std::condition_variable sleepCond;
	std::thread handle;
	SplitPoint* volatile activeSplitPoint;
	volatile int splitPointsSize;
	volatile bool searching;
	volatile bool exit;
};

struct MainThread : public Thread {
	MainThread() : thinking(true) {}
	virtual void idleLoop();
	volatile bool thinking;
};

struct TimerThread : public Thread {
	TimerThread() : msec(0) {}
	virtual void idleLoop();
	int msec;
};

class ThreadPool : public std::vector<Thread*> {
public:
	void init();
	~ThreadPool();

	MainThread* mainThread() { return static_cast<MainThread*>((*this)[0]); }
	Depth minSplitDepth() const { return minimumSplitDepth_; }
	TimerThread* timerThread() { return timer_; }

	// 一箇所でしか呼ばないので、FORCE_INLINE
	FORCE_INLINE void wakeUp() {
		for (size_t i = 0; i < size(); ++i) {
			(*this)[i]->maxPly = 0;
		}
		sleepWhileIdle_ = g_options["Use_Sleeping_Threads"];
	}
	// 一箇所でしか呼ばないので、FORCE_INLINE
	FORCE_INLINE void sleep() {
		sleepWhileIdle_ = true;
	}
	void readUSIOptions();
	Thread* availableSlave(Thread* master) const;
	void setTimer(const int msec);
	void waitForThinkFinished();
	void startThinking(const Position& pos, const LimitsType& limits,
					   const std::vector<Move>& searchMoves, StateStackPtr&& states);

	bool sleepWhileIdle_;
	size_t maxThreadsPerSplitPoint_;
	std::mutex mutex_;
	std::condition_variable sleepCond_;

private:
	TimerThread* timer_;
	Depth minimumSplitDepth_;
};

extern ThreadPool g_threads;

#endif // #ifndef THREAD_HPP
