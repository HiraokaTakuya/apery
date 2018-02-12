/*
  Apery, a USI shogi playing engine derived from Stockfish, a UCI chess playing engine.
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2018 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2011-2018 Hiraoka Takuya

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

#ifndef APERY_USI_HPP
#define APERY_USI_HPP

#include "common.hpp"
#include "move.hpp"

const std::string DefaultStartPositionSFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

struct OptionsMap;

class USIOption {
    using Fn = void (Searcher*, const USIOption&);
public:
    USIOption(Fn* = nullptr, Searcher* s = nullptr);
    USIOption(const char* v, Fn* = nullptr, Searcher* s = nullptr);
    USIOption(const bool v, Fn* = nullptr, Searcher* s = nullptr);
    USIOption(const int v, const int min, const int max, Fn* = nullptr, Searcher* s = nullptr);

    USIOption& operator = (const std::string& v);

    operator int() const {
        assert(type_ == "check" || type_ == "spin");
        return (type_ == "spin" ? atoi(currentValue_.c_str()) : currentValue_ == "true");
    }

    operator std::string() const {
        assert(type_ == "string");
        return currentValue_;
    }

private:
    friend std::ostream& operator << (std::ostream&, const OptionsMap&);

    std::string defaultValue_;
    std::string currentValue_;
    std::string type_;
    int min_;
    int max_;
    Fn* onChange_;
    Searcher* searcher_;
};

struct CaseInsensitiveLess {
    bool operator() (const std::string&, const std::string&) const;
};

struct OptionsMap : public std::map<std::string, USIOption, CaseInsensitiveLess> {
public:
    void init(Searcher* s);
    bool isLegalOption(const std::string name) {
        return this->find(name) != std::end(*this);
    }
};

void go(const Position& pos, std::istringstream& ssCmd);
#if defined LEARN
void go(const Position& pos, const Ply depth, const Move move);
void go(const Position& pos, const Ply depth);
#endif
void setPosition(Position& pos, std::istringstream& ssCmd);
bool setPosition(Position& pos, const HuffmanCodedPos& hcp);
Move csaToMove(const Position& pos, const std::string& moveStr);
Move usiToMove(const Position& pos, const std::string& moveStr);

#endif // #ifndef APERY_USI_HPP
