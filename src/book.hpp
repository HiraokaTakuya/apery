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

#ifndef APERY_BOOK_HPP
#define APERY_BOOK_HPP

#include "position.hpp"
#include "mt64bit.hpp"

struct BookEntry {
    Key key;
    u16 fromToPro;
    u16 count;
    Score score;
};

class Book : private std::ifstream {
public:
    Book() : random_(std::chrono::system_clock::now().time_since_epoch().count()) {}
    std::tuple<Move, Score> probe(const Position& pos, const std::string& fName, const bool pickBest);
    static void init();
    static Key bookKey(const Position& pos);

private:
    bool open(const char* fName);
    void binary_search(const Key key);

    static MT64bit mt64bit_; // 定跡のhash生成用なので、seedは固定でデフォルト値を使う。
    MT64bit random_; // 時刻をseedにして色々指すようにする。
    std::string fileName_;
    size_t size_;

    static Key ZobPiece[PieceNone][SquareNum];
    static Key ZobHand[HandPieceNum][19];
    static Key ZobTurn;
};

void makeBook(Position& pos, std::istringstream& ssCmd);

#endif // #ifndef APERY_BOOK_HPP
