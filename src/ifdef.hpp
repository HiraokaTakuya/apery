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

#ifndef APERY_IFDEF_HPP
#define APERY_IFDEF_HPP

#if 0
// 機械学習を行う際に使う。
#define LEARN
#endif

//#define EVAL_PHASE1
//#define EVAL_PHASE2
//#define EVAL_PHASE3
//#define EVAL_PHASE4
#define EVAL_ONLINE

#if 1 && !defined LEARN
// 対局時は1つの局面に対してしか探索を実行しないので、置換表などのデータをグローバルに置いて高速化する。
// 機械学習を行う時は、複数の局面に対して同時に探索を実行する為に、
// クラスで持つようにする。
#define USE_GLOBAL
#define STATIC static
#else
#define STATIC
#endif

#if 0
#define USE_QCHECKS
#endif

#if 1
// 評価関数の SIMD 化
#if defined HAVE_AVX2
#define USE_AVX2_EVAL
#elif defined HAVE_SSE4
#define USE_SSE_EVAL
#endif
#endif

#if 1
// 定跡作成時に探索を用いて定跡に点数を付ける。
#define MAKE_SEARCHED_BOOK
#endif

#if 0
// 対局で使わない機能を全て省いたものにする。
// todo: 現状メンテナンスされていないのでやること。
#define MINIMUL
#endif

#if 0
// 稲庭判定、稲庭対策を有効にする。
#define INANIWA_SHIFT
#endif

#if 0
// △２八角、△７八角、△３八角 を打たないようにする。
#define BISHOP_IN_DANGER
#endif

#if 0
// 入玉を24点法にする。
#define LAW_24
#endif

#if 0
// 探索時に片方だけが千日手を禁止して考える。
#define BAN_BLACK_REPETITION
#elif 0
#define BAN_WHITE_REPETITION
#endif

#if 0
// Magic Bitboard で必要となるマジックナンバーを求める。
#define FIND_MAGIC
#endif

#endif // #ifndef APERY_IFDEF_HPP
