#include "generateMoves.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "usi.hpp"

ThreadPool g_threads;

Thread::Thread() /*: splitPoints()*/ {
	exit = false;
	searching = false;
	splitPointsSize = 0;
	maxPly = 0;
	activeSplitPoint = nullptr;
	activePosition = nullptr;
	idx = g_threads.size();

	// move constructor
	handle = std::thread(&Thread::idleLoop, this);
}

Thread::~Thread() {
	exit = true;
	notifyOne();

	handle.join(); // Wait for thread termination
}

extern void checkTime();
void TimerThread::idleLoop() {
	while (!exit) {
		{
			std::unique_lock<std::mutex> lock(sleepLock);
			if (!exit) {
				sleepCond.wait_for(lock, std::chrono::milliseconds(msec ? msec : INT_MAX));
			}
		}
		if (msec) {
			checkTime();
		}
	}
}

void MainThread::idleLoop() {
	while (true) {
		{
			std::unique_lock<std::mutex> lock(sleepLock);
			thinking = false;
			while (!thinking && !exit) {
				// UI 関連だから要らないのかも。
				g_threads.sleepCond_.notify_one();
				sleepCond.wait(lock);
			}
		}

		if (exit) {
			return;
		}

		searching = true;
		Searcher::think();
		assert(searching);
		searching = false;
	}
}

void Thread::notifyOne() {
	std::unique_lock<std::mutex> lock(sleepLock);
	sleepCond.notify_one();
}

bool Thread::cutoffOccurred() const {
	for (SplitPoint* sp = activeSplitPoint; sp != nullptr; sp = sp->parentSplitPoint) {
		if (sp->cutoff) {
			return true;
		}
	}
	return false;
}

// master と同じ thread であるかを判定
bool Thread::isAvailableTo(Thread* master) const {
	if (searching) {
		return false;
	}

	// ローカルコピーし、途中で値が変わらないようにする。
	const int spCount = splitPointsSize;
	return !spCount || (splitPoints[spCount - 1].slavesMask & (UINT64_C(1) << master->idx));
}

void Thread::waitFor(volatile const bool& b) {
	std::unique_lock<std::mutex> lock(sleepLock);
	sleepCond.wait(lock, [&] { return b; });
}

void ThreadPool::init() {
	sleepWhileIdle_ = true;
	timer_ = new TimerThread();
	push_back(new MainThread());
	readUSIOptions();
}

ThreadPool::~ThreadPool() {
	// checkTime() がデータにアクセスしないよう、先に timer_ を delete
	delete timer_;

	for (auto elem : *this) {
		delete elem;
	}
}

void ThreadPool::readUSIOptions() {
	maxThreadsPerSplitPoint_ = g_options["Max_Threads_per_Split_Point"];
	minimumSplitDepth_       = g_options["Min_Split_Depth"] * OnePly;
	const size_t requested   = g_options["Threads"];

	assert(0 < requested);

	while (size() < requested) {
		push_back(new Thread());
	}

	while (requested < size()) {
		delete back();
		pop_back();
	}
}

Thread* ThreadPool::availableSlave(Thread* master) const {
	for (auto elem : *this) {
		if (elem->isAvailableTo(master)) {
			return elem;
		}
	}
	return nullptr;
}

void ThreadPool::setTimer(const int msec) {
	timer_->maxPly = msec;
	timer_->notifyOne(); // Wake up and restart the timer
}

void ThreadPool::waitForThinkFinished() {
	MainThread* t = mainThread();
	std::unique_lock<std::mutex> lock(t->sleepLock);
	sleepCond_.wait(lock, [&] { return !(t->thinking); });
}

void ThreadPool::startThinking(const Position& pos, const LimitsType& limits,
							   const std::vector<Move>& searchMoves, StateStackPtr&& states)
{
	waitForThinkFinished();
	Searcher::searchTimer.restart();

	Searcher::signals.stopOnPonderHit = Searcher::signals.firstRootMove = false;
	Searcher::signals.stop = Searcher::signals.failedLowAtRoot = false;

	g_rootPosition = pos;
	Searcher::limits = limits;
	Searcher::setUpStates = std::move(states);
	Searcher::rootMoves.clear();

	for (MoveList<Legal> ml(pos); !ml.end(); ++ml) {
		if (searchMoves.empty()
			|| std::find(searchMoves.begin(), searchMoves.end(), ml.move()) != searchMoves.end())
		{
			Searcher::rootMoves.push_back(RootMove(ml.move()));
		}
	}

	mainThread()->thinking = true;
	mainThread()->notifyOne();
}

template <bool Fake>
void Thread::split(Position& pos, SearchStack* ss, const Score alpha, const Score beta, Score& bestScore,
				   Move& bestMove, const Depth depth, const Move threatMove, const int moveCount,
				   MovePicker& mp, const NodeType nodeType, const bool cutNode)
{
	assert(pos.isOK());
	assert(bestScore <= alpha && alpha < beta && beta <= ScoreInfinite);
	assert(-ScoreInfinite < bestScore);
	assert(g_threads.minSplitDepth() <= depth);

	assert(searching);
	assert(splitPointsSize < MaxSplitPointsPerThread);

	SplitPoint& sp = splitPoints[splitPointsSize];

	sp.masterThread = this;
	sp.parentSplitPoint = activeSplitPoint;
	sp.slavesMask = UINT64_C(1) << idx;
	sp.depth = depth;
	sp.bestMove = bestMove;
	sp.threatMove = threatMove;
	sp.alpha = alpha;
	sp.beta = beta;
	sp.nodeType = nodeType;
	sp.cutNode = cutNode;
	sp.bestScore = bestScore;
	sp.movePicker = &mp;
	sp.moveCount = moveCount;
	sp.pos = &pos;
	sp.nodes = 0;
	sp.cutoff = false;
	sp.ss = ss;

	g_threads.mutex_.lock();
	sp.mutex.lock();

	++splitPointsSize;
	activeSplitPoint = &sp;
	activePosition = nullptr;

	// thisThread が常に含まれるので 1
	size_t slavesCount = 1;
	Thread* slave;

	while ((slave = g_threads.availableSlave(this)) != nullptr
		   && ++slavesCount <= g_threads.maxThreadsPerSplitPoint_ && !Fake)
	{
		sp.slavesMask |= UINT64_C(1) << slave->idx;
		slave->activeSplitPoint = &sp;
		slave->searching = true;
		slave->notifyOne();
	}

	if (1 < slavesCount || Fake) {
		sp.mutex.unlock();
		g_threads.mutex_.unlock();
		Thread::idleLoop();
		assert(!searching);
		assert(!activePosition);
		g_threads.mutex_.lock();
		sp.mutex.lock();
	}

	searching = true;
	--splitPointsSize;
	activeSplitPoint = sp.parentSplitPoint;
	activePosition = &pos;
	pos.setNodesSearched(pos.nodesSearched() + sp.nodes);
	bestMove = sp.bestMove;
	bestScore = sp.bestScore;

	g_threads.mutex_.unlock();
	sp.mutex.unlock();
}

template void Thread::split<true >(Position& pos, SearchStack* ss, const Score alpha, const Score beta, Score& bestScore,
								   Move& bestMove, const Depth depth, const Move threatMove, const int moveCount,
								   MovePicker& mp, const NodeType nodeType, const bool cutNode);
template void Thread::split<false>(Position& pos, SearchStack* ss, const Score alpha, const Score beta, Score& bestScore,
								   Move& bestMove, const Depth depth, const Move threatMove, const int moveCount,
								   MovePicker& mp, const NodeType nodeType, const bool cutNode);
