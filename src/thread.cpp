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

#include "generateMoves.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "usi.hpp"

Thread::Thread(Searcher* s) {
	searcher = s;
	resetCalls = exit = false;
	maxPly = callsCnt = 0;
	//history.clear();
	//gains.clear();
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

s64 ThreadPool::nodesSearched() const {
	s64 nodes = 0;
	for (Thread* th : *this)
		nodes += th->rootPos.nodesSearched();
	return nodes;
}

void ThreadPool::startThinking(const Position& pos, const LimitsType& limits, StateStackPtr& states) {
	main()->waitForSearchFinished();
	pos.searcher()->signals.stopOnPonderHit = pos.searcher()->signals.stop = false;

	main()->rootMoves.clear();
	main()->rootPos = pos;
	pos.searcher()->limits = limits;
	if (states.get()) {
		pos.searcher()->setUpStates = std::move(states);
		assert(!states.get());
	}

	for (MoveList<Legal> ml(pos); !ml.end(); ++ml) {
		if (limits.searchmoves.empty()
			|| std::find(std::begin(limits.searchmoves), std::end(limits.searchmoves), ml.move()) != std::end(limits.searchmoves))
		{
			main()->rootMoves.push_back(RootMove(ml.move()));
		}
	}
	main()->startSearching();
}
