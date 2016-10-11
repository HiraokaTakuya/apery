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

#include "benchmark.hpp"
#include "common.hpp"
#include "usi.hpp"
#include "position.hpp"
#include "search.hpp"

// 今はベンチマークというより、PGO ビルドの自動化の為にある。
void benchmark(Position& pos) {
	std::string token;
	std::string options[] = {"name Threads value 1",
							 "name MultiPV value 1",
							 "name OwnBook value false",
							 "name Max_Random_Score_Diff value 0"};
	for (auto& str : options) {
		std::istringstream is(str);
		pos.searcher()->setOption(is);
	}

	std::ifstream ifs("benchmark.sfen");
	std::string sfen;
	while (std::getline(ifs, sfen)) {
		std::cout << sfen << std::endl;
		std::istringstream ss_sfen(sfen);
		setPosition(pos, ss_sfen);
		std::istringstream ss_go("byoyomi 10000");
		go(pos, ss_go);
		pos.searcher()->threads.main()->waitForSearchFinished();
	}
}
