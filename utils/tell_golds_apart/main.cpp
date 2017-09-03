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
#include <random>
#include <chrono>
#include <tuple>
#include <memory>
#include <cinttypes>

enum struct EvalIndexIdentifyGolds : int32_t {
    f_hand_pawn   = 0, // 0
    e_hand_pawn   = f_hand_pawn   + 19,
    f_hand_lance  = e_hand_pawn   + 19,
    e_hand_lance  = f_hand_lance  +  5,
    f_hand_knight = e_hand_lance  +  5,
    e_hand_knight = f_hand_knight +  5,
    f_hand_silver = e_hand_knight +  5,
    e_hand_silver = f_hand_silver +  5,
    f_hand_gold   = e_hand_silver +  5,
    e_hand_gold   = f_hand_gold   +  5,
    f_hand_bishop = e_hand_gold   +  5,
    e_hand_bishop = f_hand_bishop +  3,
    f_hand_rook   = e_hand_bishop +  3,
    e_hand_rook   = f_hand_rook   +  3,
    fe_hand_end   = e_hand_rook   +  3,

    f_pawn        = fe_hand_end,
    e_pawn        = f_pawn        + 81,
    f_lance       = e_pawn        + 81,
    e_lance       = f_lance       + 81,
    f_knight      = e_lance       + 81,
    e_knight      = f_knight      + 81,
    f_silver      = e_knight      + 81,
    e_silver      = f_silver      + 81,
    f_gold        = e_silver      + 81,
    e_gold        = f_gold        + 81,
    f_bishop      = e_gold        + 81,
    e_bishop      = f_bishop      + 81,
    f_horse       = e_bishop      + 81,
    e_horse       = f_horse       + 81,
    f_rook        = e_horse       + 81,
    e_rook        = f_rook        + 81,
    f_dragon      = e_rook        + 81,
    e_dragon      = f_dragon      + 81,
    fe_end        = e_dragon      + 81
};

enum struct EvalIndex : int32_t {
    f_hand_pawn   = 0, // 0
    e_hand_pawn   = f_hand_pawn   + 19,
    f_hand_lance  = e_hand_pawn   + 19,
    e_hand_lance  = f_hand_lance  +  5,
    f_hand_knight = e_hand_lance  +  5,
    e_hand_knight = f_hand_knight +  5,
    f_hand_silver = e_hand_knight +  5,
    e_hand_silver = f_hand_silver +  5,
    f_hand_gold   = e_hand_silver +  5,
    e_hand_gold   = f_hand_gold   +  5,
    f_hand_bishop = e_hand_gold   +  5,
    e_hand_bishop = f_hand_bishop +  3,
    f_hand_rook   = e_hand_bishop +  3,
    e_hand_rook   = f_hand_rook   +  3,
    fe_hand_end   = e_hand_rook   +  3,

    f_pawn        = fe_hand_end,
    e_pawn        = f_pawn        + 81,
    f_lance       = e_pawn        + 81,
    e_lance       = f_lance       + 81,
    f_knight      = e_lance       + 81,
    e_knight      = f_knight      + 81,
    f_silver      = e_knight      + 81,
    e_silver      = f_silver      + 81,
    f_gold        = e_silver      + 81,
    e_gold        = f_gold        + 81,
    f_pro_pawn    = e_gold        + 81,
    e_pro_pawn    = f_pro_pawn    + 81,
    f_pro_lance   = e_pro_pawn    + 81,
    e_pro_lance   = f_pro_lance   + 81,
    f_pro_knight  = e_pro_lance   + 81,
    e_pro_knight  = f_pro_knight  + 81,
    f_pro_silver  = e_pro_knight  + 81,
    e_pro_silver  = f_pro_silver  + 81,
    f_bishop      = e_pro_silver  + 81,
    e_bishop      = f_bishop      + 81,
    f_horse       = e_bishop      + 81,
    e_horse       = f_horse       + 81,
    f_rook        = e_horse       + 81,
    e_rook        = f_rook        + 81,
    f_dragon      = e_rook        + 81,
    e_dragon      = f_dragon      + 81,
    fe_end        = e_dragon      + 81
};

const std::tuple<EvalIndexIdentifyGolds, EvalIndex, int> array[] = {
    {EvalIndexIdentifyGolds::f_hand_pawn  , EvalIndex::f_hand_pawn  , 19},
    {EvalIndexIdentifyGolds::e_hand_pawn  , EvalIndex::e_hand_pawn  , 19},
    {EvalIndexIdentifyGolds::f_hand_lance , EvalIndex::f_hand_lance ,  5},
    {EvalIndexIdentifyGolds::e_hand_lance , EvalIndex::e_hand_lance ,  5},
    {EvalIndexIdentifyGolds::f_hand_knight, EvalIndex::f_hand_knight,  5},
    {EvalIndexIdentifyGolds::e_hand_knight, EvalIndex::e_hand_knight,  5},
    {EvalIndexIdentifyGolds::f_hand_silver, EvalIndex::f_hand_silver,  5},
    {EvalIndexIdentifyGolds::e_hand_silver, EvalIndex::e_hand_silver,  5},
    {EvalIndexIdentifyGolds::f_hand_gold  , EvalIndex::f_hand_gold  ,  5},
    {EvalIndexIdentifyGolds::e_hand_gold  , EvalIndex::e_hand_gold  ,  5},
    {EvalIndexIdentifyGolds::f_hand_bishop, EvalIndex::f_hand_bishop,  3},
    {EvalIndexIdentifyGolds::e_hand_bishop, EvalIndex::e_hand_bishop,  3},
    {EvalIndexIdentifyGolds::f_hand_rook  , EvalIndex::f_hand_rook  ,  3},
    {EvalIndexIdentifyGolds::e_hand_rook  , EvalIndex::e_hand_rook  ,  3},
    {EvalIndexIdentifyGolds::f_pawn       , EvalIndex::f_pawn       , 81},
    {EvalIndexIdentifyGolds::e_pawn       , EvalIndex::e_pawn       , 81},
    {EvalIndexIdentifyGolds::f_lance      , EvalIndex::f_lance      , 81},
    {EvalIndexIdentifyGolds::e_lance      , EvalIndex::e_lance      , 81},
    {EvalIndexIdentifyGolds::f_knight     , EvalIndex::f_knight     , 81},
    {EvalIndexIdentifyGolds::e_knight     , EvalIndex::e_knight     , 81},
    {EvalIndexIdentifyGolds::f_silver     , EvalIndex::f_silver     , 81},
    {EvalIndexIdentifyGolds::e_silver     , EvalIndex::e_silver     , 81},
    {EvalIndexIdentifyGolds::f_gold       , EvalIndex::f_gold       , 81},
    {EvalIndexIdentifyGolds::e_gold       , EvalIndex::e_gold       , 81},
    {EvalIndexIdentifyGolds::f_gold       , EvalIndex::f_pro_pawn   , 81},
    {EvalIndexIdentifyGolds::e_gold       , EvalIndex::e_pro_pawn   , 81},
    {EvalIndexIdentifyGolds::f_gold       , EvalIndex::f_pro_lance  , 81},
    {EvalIndexIdentifyGolds::e_gold       , EvalIndex::e_pro_lance  , 81},
    {EvalIndexIdentifyGolds::f_gold       , EvalIndex::f_pro_knight , 81},
    {EvalIndexIdentifyGolds::e_gold       , EvalIndex::e_pro_knight , 81},
    {EvalIndexIdentifyGolds::f_gold       , EvalIndex::f_pro_silver , 81},
    {EvalIndexIdentifyGolds::e_gold       , EvalIndex::e_pro_silver , 81},
    {EvalIndexIdentifyGolds::f_bishop     , EvalIndex::f_bishop     , 81},
    {EvalIndexIdentifyGolds::e_bishop     , EvalIndex::e_bishop     , 81},
    {EvalIndexIdentifyGolds::f_horse      , EvalIndex::f_horse      , 81},
    {EvalIndexIdentifyGolds::e_horse      , EvalIndex::e_horse      , 81},
    {EvalIndexIdentifyGolds::f_rook       , EvalIndex::f_rook       , 81},
    {EvalIndexIdentifyGolds::e_rook       , EvalIndex::e_rook       , 81},
    {EvalIndexIdentifyGolds::f_dragon     , EvalIndex::f_dragon     , 81},
    {EvalIndexIdentifyGolds::e_dragon     , EvalIndex::e_dragon     , 81},
};

struct Buffers {
    int16_t KPP_Identify_Golds[81][(int)EvalIndexIdentifyGolds::fe_end][(int)EvalIndexIdentifyGolds::fe_end][2];
    int16_t KKP_Identify_Golds[81][81][(int)EvalIndexIdentifyGolds::fe_end][2];
    int16_t KPP[81][(int)EvalIndex::fe_end][(int)EvalIndex::fe_end][2];
    int16_t KKP[81][81][(int)EvalIndex::fe_end][2];
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "USAGE: " << argv[0] << " <input an evaluator (identified golds) directory> <output an evluator (tell golds apart) directory>\n" << std::endl;
        return 0;
    }
    std::unique_ptr<Buffers> buf(new Buffers);
    std::string idir = argv[1];
    if (idir.back() != '/')
        idir += "/";
    std::ifstream ifskpp((idir + "KPP_synthesized.bin").c_str(), std::ios::binary);
    ifskpp.read((char*)buf->KPP_Identify_Golds, sizeof(buf->KPP_Identify_Golds));
    std::ifstream ifskkp((idir + "KKP_synthesized.bin").c_str(), std::ios::binary);
    ifskkp.read((char*)buf->KKP_Identify_Golds, sizeof(buf->KKP_Identify_Golds));

    for (int sq = 0; sq < 81; ++sq) {
        for (auto& elem0 : array) {
            for (int i = 0; i < std::get<2>(elem0); ++i) {
                for (auto& elem1 : array) {
                    for (int j = 0; j < std::get<2>(elem1); ++j) {
                        buf->KPP[sq][(int)std::get<1>(elem0) + i][(int)std::get<1>(elem1) + j][0] = buf->KPP_Identify_Golds[sq][(int)std::get<0>(elem0) + i][(int)std::get<0>(elem1) + j][0];
                        buf->KPP[sq][(int)std::get<1>(elem0) + i][(int)std::get<1>(elem1) + j][1] = buf->KPP_Identify_Golds[sq][(int)std::get<0>(elem0) + i][(int)std::get<0>(elem1) + j][1];
                    }
                }
            }
        }
    }

    for (int sq0 = 0; sq0 < 81; ++sq0) {
        for (int sq1 = 0; sq1 < 81; ++sq1) {
            for (auto& elem : array) {
                for (int i = 0; i < std::get<2>(elem); ++i) {
                    buf->KKP[sq0][sq1][(int)std::get<1>(elem) + i][0] = buf->KKP_Identify_Golds[sq0][sq1][(int)std::get<0>(elem) + i][0];
                    buf->KKP[sq0][sq1][(int)std::get<1>(elem) + i][1] = buf->KKP_Identify_Golds[sq0][sq1][(int)std::get<0>(elem) + i][1];
                }
            }
        }
    }

    std::string odir = argv[2];
    if (odir.back() != '/')
        odir += "/";
    std::ofstream ofskpp((odir + "KPP_synthesized.bin").c_str(), std::ios::binary);
    ofskpp.write((char*)buf->KPP, sizeof(buf->KPP));
    std::ofstream ofskkp((odir + "KKP_synthesized.bin").c_str(), std::ios::binary);
    ofskkp.write((char*)buf->KKP, sizeof(buf->KKP));
}
