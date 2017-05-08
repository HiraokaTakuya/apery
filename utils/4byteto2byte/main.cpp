/*
  Apery, a USI shogi playing engine derived from Stockfish, a UCI chess playing engine.
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2011-2017 Hiraoka Takuya

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
#include <string>
#include <algorithm>
#include <cinttypes>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cout << "USAGE: " << argv[0] << " <input 4byte elements> <output 2byte elements>\n" << std::endl;
        return 0;
    }
    std::ifstream ifs(argv[1], std::ios::binary);
    const size_t fileSize = static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
    ifs.seekg(0, std::ios::beg); // ストリームのポインタを一番前に戻して、これから先で使いやすいようにする
    std::vector<int32_t> buf(fileSize/sizeof(int32_t));
    std::vector<int16_t> outbuf(buf.size());
    ifs.read(reinterpret_cast<char*>(buf.data()), fileSize);
    std::copy(std::begin(buf), std::end(buf), std::begin(outbuf));
    std::ofstream ofs(argv[2], std::ios::binary);
    ofs.write(reinterpret_cast<char*>(outbuf.data()), outbuf.size() * sizeof(int16_t));
}
