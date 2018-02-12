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

#ifndef APERY_OVERLOADENUMOPERATORS_HPP
#define APERY_OVERLOADENUMOPERATORS_HPP

#define OverloadEnumOperators(T)                                        \
    inline T& operator += (T& lhs, const int rhs) { return lhs  = static_cast<T>(static_cast<int>(lhs) + rhs); } \
    inline T& operator += (T& lhs, const T   rhs) { return lhs += static_cast<int>(rhs); } \
    inline T& operator -= (T& lhs, const int rhs) { return lhs  = static_cast<T>(static_cast<int>(lhs) - rhs); } \
    inline T& operator -= (T& lhs, const T   rhs) { return lhs -= static_cast<int>(rhs); } \
    inline T& operator *= (T& lhs, const int rhs) { return lhs  = static_cast<T>(static_cast<int>(lhs) * rhs); } \
    inline T& operator /= (T& lhs, const int rhs) { return lhs  = static_cast<T>(static_cast<int>(lhs) / rhs); } \
    inline constexpr T operator + (const T   lhs, const int rhs) { return static_cast<T>(static_cast<int>(lhs) + rhs); } \
    inline constexpr T operator + (const T   lhs, const T   rhs) { return lhs + static_cast<int>(rhs); } \
    inline constexpr T operator - (const T   lhs, const int rhs) { return static_cast<T>(static_cast<int>(lhs) - rhs); } \
    inline constexpr T operator - (const T   lhs, const T   rhs) { return lhs - static_cast<int>(rhs); } \
    inline constexpr T operator * (const T   lhs, const int rhs) { return static_cast<T>(static_cast<int>(lhs) * rhs); } \
    inline constexpr T operator * (const int lhs, const T   rhs) { return rhs * lhs; } \
    inline constexpr T operator * (const T   lhs, const T   rhs) { return lhs * static_cast<int>(rhs); } \
    inline constexpr T operator / (const T   lhs, const int rhs) { return static_cast<T>(static_cast<int>(lhs) / rhs); } \
    inline constexpr int operator / (const T   lhs, const T rhs) { return static_cast<int>(lhs) / static_cast<int>(rhs); } \
    inline constexpr T operator - (const T   rhs) { return static_cast<T>(-static_cast<int>(rhs)); } \
    inline T operator ++ (T& lhs) { lhs += 1; return lhs; } /* 前置 */  \
    inline T operator -- (T& lhs) { lhs -= 1; return lhs; } /* 前置 */  \
    inline T operator ++ (T& lhs, int) { const T temp = lhs; lhs += 1; return temp; } /* 後置 */ \
    /* inline T operator -- (T& lhs, int) { const T temp = lhs; lhs -= 1; return temp; } */ /* 後置 */

#endif // #ifndef APERY_OVERLOADENUMOPERATORS_HPP
