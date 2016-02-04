#include "generateMoves.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "usi.hpp"

Thread::Thread(Searcher* s) {
	searcher = s;
	resetCalls = exit = false;
	maxPly = callsCount = 0;
	history.clear();
	gains.clear();
	idx = s->threads.size();

	std::unique_lock<Mutex> lock(mutex);
	searching = true;
	nativeThread = std::thread(&Thread::idleLoop, this);
	sleepCondition.wait(lock, [&] { return !searching; });
}

Thread::~Thread() {
	mutex.lock();
	exit = true;
	sleepCondition.notify_one();
	mutex.unlock();
	nativeThread.join();
}

void Thread::startSearching(const bool resume) {
	std::unique_lock<Mutex> lock(mutex);
	if (!resume)
		searching = true;
	sleepCondition.notify_one();
}

void Thread::waitForSearchFinished() {
	std::unique_lock<Mutex> lock(mutex);
	sleepCondition.wait(lock, [&] { return !searching; });
}

void Thread::wait(std::atomic_bool& condition) {
	std::unique_lock<Mutex> lock(mutex);
	sleepCondition.wait(lock, [&] { return static_cast<bool>(condition); });
}

void Thread::idleLoop() {
	while (!exit) {
		std::unique_lock<Mutex> lock(mutex);
		searching = false;
		while (!searching && !exit) {
			sleepCondition.notify_one();
			sleepCondition.wait(lock);
		}
		lock.unlock();
		if (!exit)
			search();
	}
}

void ThreadPool::init(Searcher* s) {
	push_back(new MainThread(s));
	readUSIOptions(s);
}

void ThreadPool::exit() {
	while (size()) {
		delete back();
		pop_back();
	}
}

void ThreadPool::readUSIOptions(Searcher* s) {
	const size_t requested   = s->options["Threads"];
	assert(0 < requested);

	while (size() < requested)
		push_back(new Thread(s));

	while (requested < size()) {
		delete back();
		pop_back();
	}
}

u64 ThreadPool::nodesSearched() const {
	u64 nodes = 0;
	for (Thread* th : *this)
		nodes += th->rootPosition.nodesSearched();
	return nodes;
}

void ThreadPool::startThinking(const Position& pos, const LimitsType& limits, StateStackPtr& states) {
	mainThread()->waitForSearchFinished();
	pos.searcher()->signals.stopOnPonderHit = pos.searcher()->signals.stop = false;

	mainThread()->rootMoves.clear();
	mainThread()->rootPosition = pos;
	pos.searcher()->limits = limits;
	if (states.get()) {
		pos.searcher()->setUpStates = std::move(states);
		assert(!states.get());
	}

	for (MoveList<Legal> ml(pos); !ml.end(); ++ml) {
		if (limits.searchmoves.empty()
			|| std::find(std::begin(limits.searchmoves), std::end(limits.searchmoves), ml.move()) != std::end(limits.searchmoves))
		{
			mainThread()->rootMoves.push_back(RootMove(ml.move()));
		}
	}
	mainThread()->startSearching();
}
