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

#ifndef APERY_EVALUATE_HPP
#define APERY_EVALUATE_HPP

#include "overloadEnumOperators.hpp"
#include "common.hpp"
#include "square.hpp"
#include "piece.hpp"
#include "pieceScore.hpp"
#include "position.hpp"

// 評価関数テーブルのオフセット。
// f_xxx が味方の駒、e_xxx が敵の駒
// Bonanza の影響で持ち駒 0 の場合のインデックスが存在するが、参照する事は無い。
// todo: 持ち駒 0 の位置を詰めてテーブルを少しでも小さくする。(キャッシュに少しは乗りやすい?)
enum EvalIndex : int32_t { // TriangularArray で計算する為に 32bit にしている。
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
OverloadEnumOperators(EvalIndex);

const int FVScale = 32;

extern EvalIndex KPPIndexBeginArray[fe_end];
extern bool KPPIndexIsBlackArray[fe_end];

inline EvalIndex kppIndexBegin(const EvalIndex i) {
    return KPPIndexBeginArray[i];
}
inline bool kppIndexIsBlack(const EvalIndex i) {
    return KPPIndexIsBlackArray[i];
}
inline EvalIndex kppIndexBeginToOpponentBegin(const EvalIndex indexBegin) {
    switch (indexBegin) {
    case f_hand_pawn  : return e_hand_pawn;
    case e_hand_pawn  : return f_hand_pawn;
    case f_hand_lance : return e_hand_lance;
    case e_hand_lance : return f_hand_lance;
    case f_hand_knight: return e_hand_knight;
    case e_hand_knight: return f_hand_knight;
    case f_hand_silver: return e_hand_silver;
    case e_hand_silver: return f_hand_silver;
    case f_hand_gold  : return e_hand_gold;
    case e_hand_gold  : return f_hand_gold;
    case f_hand_bishop: return e_hand_bishop;
    case e_hand_bishop: return f_hand_bishop;
    case f_hand_rook  : return e_hand_rook;
    case e_hand_rook  : return f_hand_rook;
    case f_pawn       : return e_pawn;
    case e_pawn       : return f_pawn;
    case f_lance      : return e_lance;
    case e_lance      : return f_lance;
    case f_knight     : return e_knight;
    case e_knight     : return f_knight;
    case f_silver     : return e_silver;
    case e_silver     : return f_silver;
    case f_gold       : return e_gold;
    case e_gold       : return f_gold;
    case f_bishop     : return e_bishop;
    case e_bishop     : return f_bishop;
    case f_horse      : return e_horse;
    case e_horse      : return f_horse;
    case f_rook       : return e_rook;
    case e_rook       : return f_rook;
    case f_dragon     : return e_dragon;
    case e_dragon     : return f_dragon;
    default: UNREACHABLE;
    }
}
inline EvalIndex inverseFileIndexIfLefterThanMiddle(const EvalIndex index) {
    if (index < fe_hand_end) return index;
    const auto begin = kppIndexBegin(index);
    const Square sq = static_cast<Square>(index - begin);
    if (sq <= SQ59) return index;
    return static_cast<EvalIndex>(begin + inverseFile(sq));
};
inline EvalIndex inverseFileIndexOnBoard(const EvalIndex index, const EvalIndex indexBegin) {
    assert(f_pawn <= index);
    assert(kppIndexBegin(index) == indexBegin); // この前提で引数を取る。
    const Square sq = static_cast<Square>(index - indexBegin);
    return static_cast<EvalIndex>(indexBegin + inverseFile(sq));
};
inline EvalIndex inverseFileIndexOnBoard(const EvalIndex index) {
    return inverseFileIndexOnBoard(index, kppIndexBegin(index));
};
inline EvalIndex inverseFileIndexIfOnBoard(const EvalIndex index, const EvalIndex indexBegin) {
    if (index < fe_hand_end) return index;
    return inverseFileIndexOnBoard(index, indexBegin);
};
inline EvalIndex inverseFileIndexIfOnBoard(const EvalIndex index) {
    if (index < fe_hand_end) return index;
    return inverseFileIndexOnBoard(index, kppIndexBegin(index));
};
inline EvalIndex kppIndexToOpponentIndex(const EvalIndex index, const EvalIndex indexBegin, const EvalIndex opponentBegin) {
    return opponentBegin + (index < fe_hand_end ? index - indexBegin : (EvalIndex)inverse((Square)(index - indexBegin)));
}
inline EvalIndex kppIndexToOpponentIndex(const EvalIndex index, const EvalIndex indexBegin) {
    return kppIndexToOpponentIndex(index, indexBegin, kppIndexBeginToOpponentBegin(indexBegin));
}
inline EvalIndex kppIndexToOpponentIndex(const EvalIndex index) {
    const EvalIndex indexBegin = kppIndexBegin(index);
    return kppIndexToOpponentIndex(index, indexBegin);
}

template <typename Tl, typename Tr>
inline std::array<Tl, 2> operator += (std::array<Tl, 2>& lhs, const std::array<Tr, 2>& rhs) {
    lhs[0] += rhs[0];
    lhs[1] += rhs[1];
    return lhs;
}
template <typename Tl, typename Tr>
inline std::array<Tl, 2> operator -= (std::array<Tl, 2>& lhs, const std::array<Tr, 2>& rhs) {
    lhs[0] -= rhs[0];
    lhs[1] -= rhs[1];
    return lhs;
}

template <typename EvalElementType> struct EvaluatorBase {
    struct KPPElements {
        EvalElementType kpp[SquareNum][fe_end][fe_end];
    };
    KPPElements kpps;

    struct KKPElements {
        EvalElementType kkp[SquareNum][SquareNum][fe_end];
    };
    KKPElements kkps;

    // これらは↑のメンバ変数に一次元配列としてアクセスする為のもの。
    // 配列の要素数は上のstructのサイズから分かるはずだが無名structなのでsizeof()使いにくいから使わない。
    // 先頭さえ分かれば良いので要素数1で良い。
    EvalElementType*    oneArrayKPP(const u64 i) { return reinterpret_cast<EvalElementType*   >(&kpps) + i; }
    EvalElementType*    oneArrayKKP(const u64 i) { return reinterpret_cast<EvalElementType*   >(&kkps) + i; }

    // todo: これらややこしいし汚いので使わないようにする。
    //       型によっては kkps_begin_index などの値が異なる。
    //       ただ、end - begin のサイズは型によらず一定。
    constexpr size_t kpps_end_index() const { return sizeof(kpps)/sizeof(EvalElementType); }
    constexpr size_t kkps_end_index() const { return sizeof(kkps)/sizeof(EvalElementType); }

    // KPP に関してi, jを入れ替えた場合と比べてインデックスが若い方を返す。
    int64_t minKPPIndex(Square ksq, EvalIndex i, EvalIndex j) {
        // i == j のKP要素はKKPの方で行うので、こちらでは何も有効なindexを返さない。
        if (i == j)
            return std::numeric_limits<int64_t>::max();
        auto p = std::min({&kpps.kpp[ksq][i][j], &kpps.kpp[ksq][j][i]});
        return p - oneArrayKPP(0);
    }
    int64_t minKKPIndex(Square ksq0, Square ksq1, EvalIndex i) {
        if (ksq0 == ksq1)
            return std::numeric_limits<int64_t>::max();

        if (i < fe_hand_end) { // i 持ち駒
            if (i == kppIndexBegin(i))
                return std::numeric_limits<int64_t>::max();
        }
        else { // i 盤上
            const Square isq = static_cast<Square>(i - kppIndexBegin(i));
            if (ksq0 == isq || ksq1 == isq)
                return std::numeric_limits<int64_t>::max();
        }
        return &kkps.kkp[ksq0][ksq1][i] - oneArrayKKP(0);
    }
    void clear() { memset(this, 0, sizeof(*this)); } // float 型とかだと規格的に 0 は保証されなかった気がするが実用上問題ないだろう。
};

// float, double 型の atomic 加算。T は float, double を想定。
template <typename T0, typename T1>
inline T0 atomicAdd(std::atomic<T0> &x, const T1& diff) {
    T0 old = x.load(std::memory_order_consume);
    T0 desired = old + diff;
    while (!x.compare_exchange_weak(old, desired, std::memory_order_release, std::memory_order_consume))
        desired = old + diff;
    return desired;
}
// float, double 型の atomic 減算
template <typename T0, typename T1>
inline T0 atomicSub(std::atomic<T0> &x, const T1& diff) { return atomicAdd(x, -diff); }

struct EvaluatorGradient : public EvaluatorBase<std::array<std::atomic<float>, 2>> {
    void incParam(const Position& pos, const std::array<float, 2>& dinc) {
        const Square sq_bk = pos.kingSquare(Black);
        const Square sq_wk = pos.kingSquare(White);
        const EvalIndex* list0 = pos.cplist0();
        const EvalIndex* list1 = pos.cplist1();
        const std::array<float, 2> f = {{dinc[0] / FVScale, dinc[1] / FVScale}};

        for (int i = 0; i < pos.nlist(); ++i) {
            const EvalIndex k0 = list0[i];
            const EvalIndex k1 = list1[i];
            atomicAdd(kkps.kkp[sq_bk][sq_wk][k0][0], f[0]);
            atomicAdd(kkps.kkp[sq_bk][sq_wk][k0][1], f[1]);
            for (int j = 0; j < i; ++j) {
                const EvalIndex l0 = list0[j];
                const EvalIndex l1 = list1[j];
                atomicAdd(kpps.kpp[sq_bk         ][k0][l0][0], f[0]);
                atomicAdd(kpps.kpp[sq_bk         ][k0][l0][1], f[1]);
                atomicSub(kpps.kpp[inverse(sq_wk)][k1][l1][0], f[0]);
                atomicAdd(kpps.kpp[inverse(sq_wk)][k1][l1][1], f[1]);
            }
        }
    }

    // 2駒を入れ替えた場合と比べてインデックスが若い方に足しこむ。
    void sumMirror() {
#if defined _OPENMP
#pragma omp parallel
#endif
        {
#ifdef _OPENMP
#pragma omp for
#endif
            for (int ksq = SQ11; ksq < SquareNum; ++ksq) {
                for (EvalIndex i = (EvalIndex)0; i < fe_end; ++i) {
                    for (EvalIndex j = (EvalIndex)0; j < fe_end; ++j) {
                        const int64_t index = minKPPIndex((Square)ksq, i, j);
                        if (index == std::numeric_limits<int64_t>::max())
                            continue;
                        else if (index < 0) {
                            // 内容を負として扱う。
                            atomicSub((*oneArrayKPP(-index))[0], kpps.kpp[ksq][i][j][0]);
                            atomicAdd((*oneArrayKPP(-index))[1], kpps.kpp[ksq][i][j][1]);
                        }
                        else if (&kpps.kpp[ksq][i][j] != oneArrayKPP(index)) {
                            atomicAdd((*oneArrayKPP( index))[0], kpps.kpp[ksq][i][j][0]);
                            atomicAdd((*oneArrayKPP( index))[1], kpps.kpp[ksq][i][j][1]);
                        }
                    }
                }
            }
#ifdef _OPENMP
#pragma omp for
#endif
            for (int ksq0 = SQ11; ksq0 < SquareNum; ++ksq0) {
                for (Square ksq1 = SQ11; ksq1 < SquareNum; ++ksq1) {
                    for (EvalIndex i = (EvalIndex)0; i < fe_end; ++i) {
                        const int64_t index = minKKPIndex((Square)ksq0, ksq1, i);
                        if (index == std::numeric_limits<int64_t>::max())
                            continue;
                        else if (index < 0) {
                            // 内容を負として扱う。
                            atomicSub((*oneArrayKKP(-index))[0], kkps.kkp[ksq0][ksq1][i][0]);
                            atomicAdd((*oneArrayKKP(-index))[1], kkps.kkp[ksq0][ksq1][i][1]);
                        }
                        else if (&kkps.kkp[ksq0][ksq1][i] != oneArrayKKP(index)) {
                            atomicAdd((*oneArrayKKP( index))[0], kkps.kkp[ksq0][ksq1][i][0]);
                            atomicAdd((*oneArrayKKP( index))[1], kkps.kkp[ksq0][ksq1][i][1]);
                        }
                    }
                }
            }
        }
    }
};

using EvalElementType = std::array<s16, 2>;
using KPPEvalElementType0 = EvalElementType[fe_end];
using KPPEvalElementType1 = KPPEvalElementType0[fe_end];
using KPPEvalElementType2 = KPPEvalElementType1[SquareNum];
using KKPEvalElementType0 = EvalElementType[fe_end];
using KKPEvalElementType1 = KKPEvalElementType0[SquareNum];
using KKPEvalElementType2 = KKPEvalElementType1[SquareNum];
struct Evaluator /*: public EvaluatorBase<EvalElementType>*/ {
    using Base = EvaluatorBase<EvalElementType>;
    static bool allocated;
    static KPPEvalElementType1* KPP;
    static KKPEvalElementType1* KKP;

    static std::string addSlashIfNone(const std::string& str) {
        std::string ret = str;
        if (ret == "")
            ret += ".";
        if (ret.back() != '/')
            ret += "/";
        return ret;
    }

    static void init(const std::string& dirName) {
        if (!allocated) {
            allocated = true;
            KPP = new KPPEvalElementType1[SquareNum];
            KKP = new KKPEvalElementType1[SquareNum];
            memset(KPP, 0, sizeof(KPPEvalElementType1) * (size_t)SquareNum);
            memset(KKP, 0, sizeof(KKPEvalElementType1) * (size_t)SquareNum);
        }
        readEvalFile(dirName);
    }

    // 2GB を超えるファイルは Msys2 環境では std::ifstream では一度に read 出来ず、分割して read する必要がある。
    static bool readEvalFile(const std::string& dirName) {
#define FOO(x) {                                                        \
            std::ifstream fs((addSlashIfNone(dirName) + #x ".bin").c_str(), std::ios::binary); \
            if (!fs)                                                    \
                return false;                                           \
            auto end = (char*)x + sizeof(x ## EvalElementType2);        \
            for (auto it = (char*)x; it < end; it += (1 << 30)) {       \
                size_t size = (it + (1 << 30) < end ? (1 << 30) : end - it); \
                fs.read(it, size);                                      \
            }                                                           \
        }
        FOO(KPP);
        FOO(KKP);
#undef FOO
        return true;
    }
    static bool writeEvalFile(const std::string& dirName) {
#define FOO(x) {                                                        \
            std::ofstream fs((addSlashIfNone(dirName) + #x ".bin").c_str(), std::ios::binary); \
            if (!fs)                                                    \
                return false;                                           \
            auto end = (char*)x + sizeof(x ## EvalElementType2);        \
            for (auto it = (char*)x; it < end; it += (1 << 30)) {       \
                size_t size = (it + (1 << 30) < end ? (1 << 30) : end - it); \
                fs.write(it, size);                                     \
            }                                                           \
        }
        FOO(KPP);
        FOO(KKP);
#undef FOO
        return true;
    }
};

extern const EvalIndex kppArray[31];
extern const EvalIndex kppHandArray[ColorNum][HandPieceNum];

struct EvalSum {
#if defined USE_AVX2_EVAL
    EvalSum(const EvalSum& es) {
        _mm256_store_si256(&mm, es.mm);
    }
    EvalSum& operator = (const EvalSum& rhs) {
        _mm256_store_si256(&mm, rhs.mm);
        return *this;
    }
#elif defined USE_SSE_EVAL
    EvalSum(const EvalSum& es) {
        _mm_store_si128(&m[0], es.m[0]);
        _mm_store_si128(&m[1], es.m[1]);
    }
    EvalSum& operator = (const EvalSum& rhs) {
        _mm_store_si128(&m[0], rhs.m[0]);
        _mm_store_si128(&m[1], rhs.m[1]);
        return *this;
    }
#endif
    EvalSum() {}
    s32 sum(const Color c) const {
        const s32 scoreBoard = p[0][0] - p[1][0] + p[2][0];
        const s32 scoreTurn  = p[0][1] + p[1][1] + p[2][1];
        return (c == Black ? scoreBoard : -scoreBoard) + scoreTurn;
    }
    EvalSum& operator += (const EvalSum& rhs) {
#if defined USE_AVX2_EVAL
        mm = _mm256_add_epi32(mm, rhs.mm);
#elif defined USE_SSE_EVAL
        m[0] = _mm_add_epi32(m[0], rhs.m[0]);
        m[1] = _mm_add_epi32(m[1], rhs.m[1]);
#else
        p[0][0] += rhs.p[0][0];
        p[0][1] += rhs.p[0][1];
        p[1][0] += rhs.p[1][0];
        p[1][1] += rhs.p[1][1];
        p[2][0] += rhs.p[2][0];
        p[2][1] += rhs.p[2][1];
#endif
        return *this;
    }
    EvalSum& operator -= (const EvalSum& rhs) {
#if defined USE_AVX2_EVAL
        mm = _mm256_sub_epi32(mm, rhs.mm);
#elif defined USE_SSE_EVAL
        m[0] = _mm_sub_epi32(m[0], rhs.m[0]);
        m[1] = _mm_sub_epi32(m[1], rhs.m[1]);
#else
        p[0][0] -= rhs.p[0][0];
        p[0][1] -= rhs.p[0][1];
        p[1][0] -= rhs.p[1][0];
        p[1][1] -= rhs.p[1][1];
        p[2][0] -= rhs.p[2][0];
        p[2][1] -= rhs.p[2][1];
#endif
        return *this;
    }
    EvalSum operator + (const EvalSum& rhs) const { return EvalSum(*this) += rhs; }
    EvalSum operator - (const EvalSum& rhs) const { return EvalSum(*this) -= rhs; }

    // ehash 用。
    void encode() {
#if defined USE_AVX2_EVAL
        // EvalSum は atomic にコピーされるので key が合っていればデータも合っている。
#else
        key ^= data[0] ^ data[1] ^ data[2];
#endif
    }
    void decode() { encode(); }

    union {
        std::array<std::array<s32, 2>, 4> p;
        struct {
            u64 data[3];
            u64 key; // ehash用。
        };
#if defined USE_AVX2_EVAL
        __m256i mm;
#endif
#if defined USE_AVX2_EVAL || defined USE_SSE_EVAL
        __m128i m[2];
#endif
    };
};

class Position;
struct SearchStack;

const size_t EvaluateTableSize = 0x400000; // 134MB
//const size_t EvaluateTableSize = 0x2000000; // 1GB
//const size_t EvaluateTableSize = 0x10000000; // 8GB
//const size_t EvaluateTableSize = 0x20000000; // 17GB

using EvaluateHashEntry = EvalSum;
struct EvaluateHashTable : HashTable<EvaluateHashEntry, EvaluateTableSize> {};
extern EvaluateHashTable g_evalTable;

Score evaluateUnUseDiff(const Position& pos);
Score evaluate(Position& pos, SearchStack* ss);

#endif // #ifndef APERY_EVALUATE_HPP
