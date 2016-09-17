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

#include <iostream>
#include <fstream>
#include <vector>
#include <cinttypes>

struct BookEntry {
	uint64_t key;
	uint16_t fromToPro;
	uint16_t count;
	int32_t score;
};

int main(int argc, char *argv[]) {
	std::ifstream ifs("../../bin/book.bin");
	const size_t fileSize = static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
	ifs.seekg(0, std::ios::beg);     // ストリームのポインタを一番前に戻して、これから先で使いやすいようにする

	if (fileSize % sizeof(BookEntry) != 0) {
		std::cout << "arere?" << std::endl;
		return 0;
	}
	std::vector<BookEntry> vec(fileSize/sizeof(BookEntry));
	ifs.read(reinterpret_cast<char*>(&vec[0]), fileSize);

	while (true) {
		auto it = std::begin(vec);
		for (; it != std::end(vec); ++it) {
			if (it->key == 18050028689871886544ull && it->fromToPro == 8386) { vec.erase(it); break; } // 2726fu3334fu7776fu 8384fu
			if (it->key == 18050028689871886544ull && it->fromToPro == 4903) { vec.erase(it); break; } // 2726fu3334fu7776fu 5354fu
			if (it->key == 8222405343924515057ull && it->fromToPro == 8386)  { vec.erase(it); break; } // 2726fu 8384fu
			if (it->key == 4502226897143472341ull && it->fromToPro == 8386)  { vec.erase(it); break; } // 7776fu 8384fu
		}
		if (it == std::end(vec)) break;
	}
	std::ofstream ofs("book.bin", std::ios::binary);
	ofs.write(reinterpret_cast<char*>(&vec[0]), vec.size()*sizeof(BookEntry));
}
