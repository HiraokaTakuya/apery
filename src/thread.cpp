#include "generateMoves.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "usi.hpp"

namespace {
	template <typename T> T* newThread(Searcher* s) {
		T* th = new T(s);
		th->handle = std::thread(&Thread::idleLoop, th); // move constructor
		return th;
	}
	void deleteThread(Thread* th) {
		th->exit = true;
		th->notifyOne();
		th->handle.join(); // Wait for thread termination
		delete th;
	}
}

Thread::Thread(Searcher* s) /*: splitPoints()*/ {
	searcher = s;
	exit = false;
	searching = false;
	splitPointsSize = 0;
	maxPly = 0;
	activeSplitPoint = nullptr;
	activePosition = nullptr;
	idx = s->threads.size();
}

void TimerThread::idleLoop() {
	while (!exit) {
		{
			std::unique_lock<Mutex> lock(sleepLock);
			if (!exit)
				sleepCond.wait_for(lock, std::chrono::milliseconds(msec ? msec : INT_MAX));
		}
		if (msec)
			searcher->checkTime();
	}
}

void MainThread::idleLoop() {
	while (true) {
		{
			std::unique_lock<Mutex> lock(sleepLock);
			thinking = false;
			while (!thinking && !exit) {
				// UI 関連だから要らないのかも。
				searcher->threads.sleepCond_.notify_one();
				sleepCond.wait(lock);
			}
		}

		if (exit)
			return;

		searching = true;
		searcher->think();
		assert(searching);
		searching = false;
	}
}

void Thread::notifyOne() {
	std::unique_lock<Mutex> lock(sleepLock);
	sleepCond.notify_one();
}

bool Thread::cutoffOccurred() const {
	for (SplitPoint* sp = activeSplitPoint; sp != nullptr; sp = sp->parentSplitPoint) {
		if (sp->cutoff)
			return true;
	}
	return false;
}

// master と同じ thread であるかを判定
bool Thread::isAvailableTo(Thread* master) const {
	if (searching)
		return false;

	// ローカルコピーし、途中で値が変わらないようにする。
	const int spCount = splitPointsSize;
	return !spCount || (splitPoints[spCount - 1].slavesMask & (UINT64_C(1) << master->idx));
}

void Thread::waitFor(volatile const bool& b) {
	std::unique_lock<Mutex> lock(sleepLock);
	sleepCond.wait(lock, [&] { return b; });
}

void ThreadPool::init(Searcher* s) {
	sleepWhileIdle_ = true;
#if defined LEARN
#else
	timer_ = newThread<TimerThread>(s);
#endif
	push_back(newThread<MainThread>(s));
	readUSIOptions(s);
}

void ThreadPool::exit() {
#if defined LEARN
#else
	// checkTime() がデータにアクセスしないよう、先に timer_ を delete
	deleteThread(timer_);
#endif

	for (auto elem : *this)
		deleteThread(elem);
}

void ThreadPool::readUSIOptions(Searcher* s) {
	maxThreadsPerSplitPoint_ = s->options["Max_Threads_per_Split_Point"];
	const size_t requested   = s->options["Threads"];
	minimumSplitDepth_ = (requested < 6 ? 4 : (requested < 8 ? 5 : 7)) * OnePly;

	assert(0 < requested);

	while (size() < requested)
		push_back(newThread<Thread>(s));

	while (requested < size()) {
		deleteThread(back());
		pop_back();
	}
}

Thread* ThreadPool::availableSlave(Thread* master) const {
	for (auto elem : *this) {
		if (elem->isAvailableTo(master))
			return elem;
	}
	return nullptr;
}

void ThreadPool::setTimer(const int msec) {
	timer_->maxPly = msec;
	timer_->notifyOne(); // Wake up and restart the timer
}

void ThreadPool::waitForThinkFinished() {
	MainThread* t = mainThread();
	std::unique_lock<Mutex> lock(t->sleepLock);
	sleepCond_.wait(lock, [&] { return !(t->thinking); });
}

void ThreadPool::startThinking(const Position& pos, const LimitsType& limits,
							   const std::vector<Move>& searchMoves)
{
#if defined LEARN
#else
	waitForThinkFinished();
#endif
	pos.searcher()->searchTimer.restart();

	pos.searcher()->signals.stopOnPonderHit = pos.searcher()->signals.firstRootMove = false;
	pos.searcher()->signals.stop = pos.searcher()->signals.failedLowAtRoot = false;

	pos.searcher()->rootPosition = pos;
	pos.searcher()->limits = limits;
	pos.searcher()->rootMoves.clear();

#if defined LEARN
	// searchMoves を直接使う。
	pos.searcher()->rootMoves.push_back(RootMove(searchMoves[0]));
#else
	const MoveType MT = Legal;
	for (MoveList<MT> ml(pos); !ml.end(); ++ml) {
		if (searchMoves.empty()
			|| std::find(searchMoves.begin(), searchMoves.end(), ml.move()) != searchMoves.end())
		{
			pos.searcher()->rootMoves.push_back(RootMove(ml.move()));
		}
	}
#endif

#if defined LEARN
	// 浅い探索なので、thread 生成、破棄のコストが高い。余分な thread を生成せずに直接探索を呼び出す。
	pos.searcher()->think();
#else
	mainThread()->thinking = true;
	mainThread()->notifyOne();
#endif
}

template <bool Fake>
void Thread::split(Position& pos, SearchStack* ss, const Score alpha, const Score beta, Score& bestScore,
				   Move& bestMove, const Depth depth, const Move threatMove, const int moveCount,
				   MovePicker& mp, const NodeType nodeType, const bool cutNode)
{
	assert(pos.isOK());
	assert(bestScore <= alpha && alpha < beta && beta <= ScoreInfinite);
	assert(-ScoreInfinite < bestScore);
	assert(searcher->threads.minSplitDepth() <= depth);

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

	searcher->threads.mutex_.lock();
	sp.mutex.lock();

	++splitPointsSize;
	activeSplitPoint = &sp;
	activePosition = nullptr;

	// thisThread が常に含まれるので 1
	size_t slavesCount = 1;
	Thread* slave;

	while ((slave = searcher->threads.availableSlave(this)) != nullptr
		   && ++slavesCount <= searcher->threads.maxThreadsPerSplitPoint_ && !Fake)
	{
		sp.slavesMask |= UINT64_C(1) << slave->idx;
		slave->activeSplitPoint = &sp;
		slave->searching = true;
		slave->notifyOne();
	}

	if (1 < slavesCount || Fake) {
		sp.mutex.unlock();
		searcher->threads.mutex_.unlock();
		Thread::idleLoop();
		assert(!searching);
		assert(!activePosition);
		searcher->threads.mutex_.lock();
		sp.mutex.lock();
	}

	searching = true;
	--splitPointsSize;
	activeSplitPoint = sp.parentSplitPoint;
	activePosition = &pos;
	pos.setNodesSearched(pos.nodesSearched() + sp.nodes);
	bestMove = sp.bestMove;
	bestScore = sp.bestScore;

	searcher->threads.mutex_.unlock();
	sp.mutex.unlock();
}

template void Thread::split<true >(Position& pos, SearchStack* ss, const Score alpha, const Score beta, Score& bestScore,
								   Move& bestMove, const Depth depth, const Move threatMove, const int moveCount,
								   MovePicker& mp, const NodeType nodeType, const bool cutNode);
template void Thread::split<false>(Position& pos, SearchStack* ss, const Score alpha, const Score beta, Score& bestScore,
								   Move& bestMove, const Depth depth, const Move threatMove, const int moveCount,
								   MovePicker& mp, const NodeType nodeType, const bool cutNode);
