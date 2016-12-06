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
enum {
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

const int FVScale = 32;

const int KPPIndexArray[] = {
    f_hand_pawn, e_hand_pawn, f_hand_lance, e_hand_lance, f_hand_knight,
    e_hand_knight, f_hand_silver, e_hand_silver, f_hand_gold, e_hand_gold,
    f_hand_bishop, e_hand_bishop, f_hand_rook, e_hand_rook, /*fe_hand_end,*/
    f_pawn, e_pawn, f_lance, e_lance, f_knight, e_knight, f_silver, e_silver,
    f_gold, e_gold, f_bishop, e_bishop, f_horse, e_horse, f_rook, e_rook,
    f_dragon, e_dragon, fe_end
};

inline Square kppIndexToSquare(const int i) {
    const auto it = std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i);
    return static_cast<Square>(i - *(it - 1));
}
inline int kppIndexBegin(const int i) {
    return *(std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i) - 1);
}
inline bool kppIndexIsBlack(const int i) {
    // f_xxx と e_xxx が交互に配列に格納されているので、インデックスが偶数の時は Black
    return !((std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i) - 1) - std::begin(KPPIndexArray) & 1);
}
inline int kppBlackIndexToWhiteBegin(const int i) {
    assert(kppIndexIsBlack(i));
    return *std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i);
}
inline int kppWhiteIndexToBlackBegin(const int i) {
    return *(std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i) - 2);
}
inline int kppIndexToOpponentBegin(const int i, const bool isBlack) {
    return *(std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i) - static_cast<int>(!isBlack) * 2);
}
inline int kppIndexToOpponentBegin(const int i) {
    // todo: 高速化
    return kppIndexToOpponentBegin(i, kppIndexIsBlack(i));
}

inline int inverseFileIndexIfLefterThanMiddle(const int index) {
    if (index < fe_hand_end) return index;
    const int begin = kppIndexBegin(index);
    const Square sq = static_cast<Square>(index - begin);
    if (sq <= SQ59) return index;
    return static_cast<int>(begin + inverseFile(sq));
};
inline int inverseFileIndexIfOnBoard(const int index) {
    if (index < fe_hand_end) return index;
    const int begin = kppIndexBegin(index);
    const Square sq = static_cast<Square>(index - begin);
    return static_cast<int>(begin + inverseFile(sq));
};
inline int inverseFileIndexOnBoard(const int index) {
    assert(f_pawn <= index);
    const int begin = kppIndexBegin(index);
    const Square sq = static_cast<Square>(index - begin);
    return static_cast<int>(begin + inverseFile(sq));
};

struct KPPBoardIndexStartToPiece : public std::unordered_map<int, Piece> {
    KPPBoardIndexStartToPiece() {
        (*this)[f_pawn  ] = BPawn;
        (*this)[e_pawn  ] = WPawn;
        (*this)[f_lance ] = BLance;
        (*this)[e_lance ] = WLance;
        (*this)[f_knight] = BKnight;
        (*this)[e_knight] = WKnight;
        (*this)[f_silver] = BSilver;
        (*this)[e_silver] = WSilver;
        (*this)[f_gold  ] = BGold;
        (*this)[e_gold  ] = WGold;
        (*this)[f_bishop] = BBishop;
        (*this)[e_bishop] = WBishop;
        (*this)[f_horse ] = BHorse;
        (*this)[e_horse ] = WHorse;
        (*this)[f_rook  ] = BRook;
        (*this)[e_rook  ] = WRook;
        (*this)[f_dragon] = BDragon;
        (*this)[e_dragon] = WDragon;
    }
    Piece value(const int i) const {
        const auto it = find(i);
        if (it == std::end(*this))
            return PieceNone;
        return it->second;
    }
};
extern KPPBoardIndexStartToPiece g_kppBoardIndexStartToPiece;

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

const int KPPIndicesMax = 3000;
const int KKPIndicesMax = 130;
const int KKIndicesMax = 7;

template <typename KPPType, typename KKPType, typename KKType> struct EvaluatorBase {
    static const int R_Mid = 8; // 相対位置の中心のindex
#if defined EVAL_ONLINE
    constexpr int MaxWeight() const { return 1; }
#else
    // todo: Bonanza Method とか 低次元要素を削除する時にこれも消す。
    constexpr int MaxWeight() const { return 1 << 22; } // KPE自体が1/32の寄与。更にKPEの遠隔駒の利きが1マスごとに1/2に減衰する分(最大でKEEの際に8マス離れが2枚)
                                                        // 更に重みを下げる場合、MaxWeightを更に大きくしておく必要がある。
                                                        // なぜか clang で static const int MaxWeight を使っても Undefined symbols for architecture x86_64 と言われる。
#endif
    constexpr int TurnWeight() const { return 8; }
    // 冗長に配列を確保しているが、対称な関係にある時は常に若いindexの方にアクセスすることにする。
    // 例えば kpp だったら、k が優先的に小さくなるようする。左右の対称も含めてアクセス位置を決める。
    // ただし、kkp に関する項目 (kkp, r_kkp_b, r_kkp_h) のみ、p は味方の駒として扱うので、k0 < k1 となるとは限らない。
    struct KPPElements {
        KPPType dummy; // 一次元配列に変換したとき、符号で +- を表すようにしているが、index = 0 の時は符号を付けられないので、ダミーを置く。
        KPPType kpp[SquareNoLeftNum][fe_end][fe_end];
    };
    KPPElements kpps;

    struct KKPElements {
        KKPType dummy; // 一次元配列に変換したとき、符号で +- を表すようにしているが、index = 0 の時は符号を付けられないので、ダミーを置く。
        KKPType kkp[SquareNoLeftNum][SquareNum][fe_end];
    };
    KKPElements kkps;

    struct KKElements {
        KKType dummy; // 一次元配列に変換したとき、符号で +- を表すようにしているが、index = 0 の時は符号を付けられないので、ダミーを置く。
        KKType kk[SquareNoLeftNum][SquareNum];
    };
    KKElements kks;

    // これらは↑のメンバ変数に一次元配列としてアクセスする為のもの。
    // 配列の要素数は上のstructのサイズから分かるはずだが無名structなのでsizeof()使いにくいから使わない。
    // 先頭さえ分かれば良いので要素数1で良い。
    KPPType* oneArrayKPP(const u64 i) { return reinterpret_cast<KPPType*>(&kpps) + i; }
    KKPType* oneArrayKKP(const u64 i) { return reinterpret_cast<KKPType*>(&kkps) + i; }
    KKType* oneArrayKK(const u64 i) { return reinterpret_cast<KKType*>(&kks) + i; }

    // todo: これらややこしいし汚いので使わないようにする。
    //       型によっては kkps_begin_index などの値が異なる。
    //       ただ、end - begin のサイズは型によらず一定。
    constexpr size_t kpps_end_index() const { return sizeof(kpps)/sizeof(KPPType); }
    constexpr size_t kkps_end_index() const { return sizeof(kkps)/sizeof(KKPType); }
    constexpr size_t kks_end_index() const { return sizeof(kks)/sizeof(KKType); }

    // KPP に関する相対位置などの次元を落とした位置などのインデックスを全て返す。
    // 負のインデックスは、正のインデックスに変換した位置の点数を引く事を意味する。
    // 0 の時だけは正負が不明だが、0 は歩の持ち駒 0 枚を意味していて無効な値なので問題なし。
    // ptrdiff_t はインデックス、int は寄与の大きさ。MaxWeight分のいくつかで表記することにする。
    void kppIndices(ptrdiff_t ret[KPPIndicesMax], Square ksq, int i, int j) {
        int retIdx = 0;
        auto pushLastIndex = [&] {
            ret[retIdx++] = std::numeric_limits<ptrdiff_t>::max();
            assert(retIdx <= KPPIndicesMax);
        };
        // i == j のKP要素はKKPの方で行うので、こちらでは何も有効なindexを返さない。
        if (i == j) {
            pushLastIndex();
            return;
        }
        if (j < i) std::swap(i, j);
        // 盤上の駒のSquareが同じ位置の場合と、持ち駒の0枚目は参照する事は無いので、有効なindexを返さない。
        if (j < fe_hand_end) {
            // i, j 共に持ち駒
            if ((i < fe_hand_end && i == kppIndexBegin(i)) // 0 枚目の持ち駒
                || (j < fe_hand_end && j == kppIndexBegin(j))) // 0 枚目の持ち駒
            {
                pushLastIndex();
                return;
            }
        }
        else if (i < fe_hand_end) {
            // i 持ち駒、 j 盤上
            const Square jsq = static_cast<Square>(j - kppIndexBegin(j));
            if ((i < fe_hand_end && i == kppIndexBegin(i)) // 0 枚目の持ち駒
                || ksq == jsq)
            {
                pushLastIndex();
                return;
            }
        }
        else {
            // i, j 共に盤上
            const Square isq = static_cast<Square>(i - kppIndexBegin(i));
            const Square jsq = static_cast<Square>(j - kppIndexBegin(j));
            if (ksq == isq || ksq == jsq || isq == jsq) {
                pushLastIndex();
                return;
            }
        }

        if (SQ59 < ksq) {
            ksq = inverseFile(ksq);
            i = inverseFileIndexIfOnBoard(i);
            j = inverseFileIndexIfOnBoard(j);
            if (j < i) std::swap(i, j);
        }
        else if (makeFile(ksq) == File5) {
            assert(i < j);
            if (f_pawn <= i) {
                const int ibegin = kppIndexBegin(i);
                const Square isq = static_cast<Square>(i - ibegin);
                const int jbegin = kppIndexBegin(j);
                const Square jsq = static_cast<Square>(j - jbegin);
                if (ibegin == jbegin) {
                    if (std::min(inverseFile(isq), inverseFile(jsq)) < std::min(isq, jsq)) {
                        i = ibegin + inverseFile(isq);
                        j = jbegin + inverseFile(jsq);
                    }
                }
                else if (SQ59 < isq) {
                    i = ibegin + inverseFile(isq);
                    j = jbegin + inverseFile(jsq);
                }
                else if (makeFile(isq) == File5)
                    j = inverseFileIndexIfLefterThanMiddle(j);
            }
            else if (f_pawn <= j)
                j = inverseFileIndexIfLefterThanMiddle(j);
        }
        if (j < i) std::swap(i, j);
        ret[retIdx++] = &kpps.kpp[ksq][i][j] - oneArrayKPP(0);
        pushLastIndex();
    }
    void kkpIndices(ptrdiff_t ret[KKPIndicesMax], Square ksq0, Square ksq1, int i) {
        int retIdx = 0;
        auto pushLastIndex = [&] {
            ret[retIdx++] = std::numeric_limits<ptrdiff_t>::max();
            assert(retIdx <= KKPIndicesMax);
        };
        if (ksq0 == ksq1) {
            ret[retIdx++] = std::numeric_limits<ptrdiff_t>::max();
            assert(retIdx <= KKPIndicesMax);
            return;
        }
        if (i < fe_hand_end) { // i 持ち駒
            if (i == kppIndexBegin(i)) {
                pushLastIndex();
                return;
            }
        }
        else { // i 盤上
            const Square isq = static_cast<Square>(i - kppIndexBegin(i));
            if (ksq0 == isq || ksq1 == isq) {
                pushLastIndex();
                return;
            }
        }
        int sign = 1;
        if (!kppIndexIsBlack(i)) {
            const Square tmp = ksq0;
            ksq0 = inverse(ksq1);
            ksq1 = inverse(tmp);
            const int ibegin = kppIndexBegin(i);
            const int opp_ibegin = kppWhiteIndexToBlackBegin(i);
            i = opp_ibegin + (i < fe_hand_end ? i - ibegin : inverse(static_cast<Square>(i - ibegin)));
            sign = -1;
        }
        if (SQ59 < ksq0) {
            ksq0 = inverseFile(ksq0);
            ksq1 = inverseFile(ksq1);
            i = inverseFileIndexIfOnBoard(i);
        }
        else if (makeFile(ksq0) == File5 && SQ59 < ksq1) {
            ksq1 = inverseFile(ksq1);
            i = inverseFileIndexIfOnBoard(i);
        }
        else if (makeFile(ksq0) == File5 && makeFile(ksq1) == File5)
            i = inverseFileIndexIfLefterThanMiddle(i);
        ret[retIdx++] = sign*(&kkps.kkp[ksq0][ksq1][i] - oneArrayKKP(0));
        ret[retIdx++] = std::numeric_limits<ptrdiff_t>::max();
        assert(retIdx <= KKPIndicesMax);
    }
    void kkIndices(ptrdiff_t ret[KKIndicesMax], Square ksq0, Square ksq1) {
        int retIdx = 0;
        auto kk_func = [this, &retIdx, &ret](Square ksq0, Square ksq1, int sign) {
            {
                // 常に ksq0 < ksq1 となるテーブルにアクセスする為の変換
                const Square ksq0Arr[] = {
                    ksq0,
                    inverseFile(ksq0),
                };
                const Square ksq1Arr[] = {
                    inverse(ksq1),
                    inverse(inverseFile(ksq1)),
                };
                auto ksq0ArrIdx = std::min_element(std::begin(ksq0Arr), std::end(ksq0Arr)) - std::begin(ksq0Arr);
                auto ksq1ArrIdx = std::min_element(std::begin(ksq1Arr), std::end(ksq1Arr)) - std::begin(ksq1Arr);
                if (ksq0Arr[ksq0ArrIdx] <= ksq1Arr[ksq1ArrIdx]) {
                    ksq0 = ksq0Arr[ksq0ArrIdx];
                    ksq1 = inverse(ksq1Arr[ksq0ArrIdx]);
                }
                else {
                    sign = -sign; // ksq0 と ksq1 を入れ替えるので符号反転
                    ksq0 = ksq1Arr[ksq1ArrIdx];
                    ksq1 = inverse(ksq0Arr[ksq1ArrIdx]);
                }
            }
            ret[retIdx++] = sign*(&kks.kk[ksq0][ksq1] - oneArrayKK(0));
            assert(ksq0 <= SQ59);
        };
        kk_func(ksq0         , ksq1         ,  1);
        kk_func(inverse(ksq1), inverse(ksq0), -1);
        ret[retIdx++] = std::numeric_limits<ptrdiff_t>::max();
        assert(retIdx <= KKIndicesMax);
    }
    void clear() { memset(this, 0, sizeof(*this)); } // float 型とかだと規格的に 0 は保証されなかった気がするが実用上問題ないだろう。
};

using KPPType = std::array<s16, 2>;
using KKPType = std::array<s32, 2>;
using KKType = std::array<s32, 2>;
struct Evaluator : public EvaluatorBase<KPPType, KKPType, KKType> {
    using Base = EvaluatorBase<KPPType, KKPType, KKType>;
    static KPPType KPP[SquareNum][fe_end][fe_end];
    static KKPType KKP[SquareNum][SquareNum][fe_end];
    static KKType KK[SquareNum][SquareNum];

    static std::string addSlashIfNone(const std::string& str) {
        std::string ret = str;
        if (ret == "")
            ret += ".";
        if (ret.back() != '/')
            ret += "/";
        return ret;
    }

    void setEvaluate() {
#if !defined LEARN
        SYNCCOUT << "info string start setting eval table" << SYNCENDL;
#endif
#define FOO(indices, oneArray, sum)                                     \
        for (auto index : indices) {                                    \
            if (index == std::numeric_limits<ptrdiff_t>::max()) break;  \
            if (0 <= index) {                                           \
                sum[0] += static_cast<s64>((*oneArray( index))[0]);     \
                sum[1] += static_cast<s64>((*oneArray( index))[1]);     \
            }                                                           \
            else {                                                      \
                sum[0] -= static_cast<s64>((*oneArray(-index))[0]);     \
                sum[1] += static_cast<s64>((*oneArray(-index))[1]);     \
            }                                                           \
        }                                                               \
        sum[1] /= Base::TurnWeight();

#if defined _OPENMP
#pragma omp parallel
#endif
        // KPP
        {
#ifdef _OPENMP
#pragma omp for
#endif
            // OpenMP対応したら何故か ksq を Square 型にすると ++ksq が定義されていなくてコンパイルエラーになる。
            for (int ksq = SQ11; ksq < SquareNum; ++ksq) {
                // indices は更に for ループの外側に置きたいが、OpenMP 使っているとアクセス競合しそうなのでループの中に置く。
                ptrdiff_t indices[KPPIndicesMax];
                for (int i = 0; i < fe_end; ++i) {
                    for (int j = 0; j < fe_end; ++j) {
                        EvaluatorBase<KPPType, KKPType, KKType>::kppIndices(indices, static_cast<Square>(ksq), i, j);
                        std::array<s64, 2> sum = {{}};
                        FOO(indices, Base::oneArrayKPP, sum);
                        KPP[ksq][i][j] += sum;
                    }
                }
            }
        }
        // KKP
        {
#ifdef _OPENMP
#pragma omp for
#endif
            for (int ksq0 = SQ11; ksq0 < SquareNum; ++ksq0) {
                ptrdiff_t indices[KKPIndicesMax];
                for (Square ksq1 = SQ11; ksq1 < SquareNum; ++ksq1) {
                    for (int i = 0; i < fe_end; ++i) {
                        EvaluatorBase<KPPType, KKPType, KKType>::kkpIndices(indices, static_cast<Square>(ksq0), ksq1, i);
                        std::array<s64, 2> sum = {{}};
                        FOO(indices, Base::oneArrayKKP, sum);
                        KKP[ksq0][ksq1][i] += sum;
                    }
                }
            }
        }
        // KK
        {
#ifdef _OPENMP
#pragma omp for
#endif
            for (int ksq0 = SQ11; ksq0 < SquareNum; ++ksq0) {
                ptrdiff_t indices[KKIndicesMax];
                for (Square ksq1 = SQ11; ksq1 < SquareNum; ++ksq1) {
                    EvaluatorBase<KPPType, KKPType, KKType>::kkIndices(indices, static_cast<Square>(ksq0), ksq1);
                    std::array<s64, 2> sum = {{}};
                    FOO(indices, Base::oneArrayKK, sum);
                    KK[ksq0][ksq1][0] += sum[0] / 2;
                    KK[ksq0][ksq1][1] += sum[1] / 2;
                }
            }
        }
#undef FOO

#if !defined LEARN
        SYNCCOUT << "info string end setting eval table" << SYNCENDL;
#endif
    }

    void init(const std::string& dirName, const bool Synthesized, const bool readBase = true) {
        // 合成された評価関数バイナリがあればそちらを使う。
        if (Synthesized) {
            if (readSynthesized(dirName))
                return;
        }
        if (readBase)
            clear();
        readSomeSynthesized(dirName);
        if (readBase)
            read(dirName);
        setEvaluate();
    }

#define ALL_SYNTHESIZED_EVAL {                  \
        FOO(KPP);                               \
        FOO(KKP);                               \
        FOO(KK);                                \
    }
    static bool readSynthesized(const std::string& dirName) {
#define FOO(x) {                                                        \
            std::ifstream ifs((addSlashIfNone(dirName) + #x "_synthesized.bin").c_str(), std::ios::binary); \
            if (ifs) ifs.read(reinterpret_cast<char*>(x), sizeof(x));   \
            else     return false;                                      \
        }
        ALL_SYNTHESIZED_EVAL;
#undef FOO
        return true;
    }
    static void writeSynthesized(const std::string& dirName) {
#define FOO(x) {                                                        \
            std::ofstream ofs((addSlashIfNone(dirName) + #x "_synthesized.bin").c_str(), std::ios::binary); \
            ofs.write(reinterpret_cast<char*>(x), sizeof(x));           \
        }
        ALL_SYNTHESIZED_EVAL;
#undef FOO
    }
    static void readSomeSynthesized(const std::string& dirName) {
#define FOO(x) {                                                        \
            std::ifstream ifs((addSlashIfNone(dirName) + #x "_some_synthesized.bin").c_str(), std::ios::binary); \
            if (ifs) ifs.read(reinterpret_cast<char*>(x), sizeof(x));   \
            else     memset(x, 0, sizeof(x));                           \
        }
        ALL_SYNTHESIZED_EVAL;
#undef FOO
    }
    static void writeSomeSynthesized(const std::string& dirName) {
#define FOO(x) {                                                        \
            std::ofstream ofs((addSlashIfNone(dirName) + #x "_some_synthesized.bin").c_str(), std::ios::binary); \
            ofs.write(reinterpret_cast<char*>(x), sizeof(x));           \
        }
        ALL_SYNTHESIZED_EVAL;
#undef FOO
    }
#undef ALL_SYNTHESIZED_EVAL

#if defined EVAL_PHASE1
#define BASE_PHASE1 {                           \
        FOO(kpps.kee);                          \
        FOO(kpps.r_kpe_b);                      \
        FOO(kpps.r_kpe_h);                      \
        FOO(kpps.r_kee);                        \
        FOO(kpps.xee);                          \
        FOO(kpps.yee);                          \
        FOO(kpps.pe);                           \
        FOO(kpps.ee);                           \
        FOO(kpps.r_pe_b);                       \
        FOO(kpps.r_pe_h);                       \
        FOO(kpps.r_ee);                         \
        FOO(kkps.ke);                           \
        FOO(kkps.r_kke);                        \
        FOO(kkps.r_ke);                         \
        FOO(kks.k);                             \
    }
#else
#define BASE_PHASE1
#endif

#if defined EVAL_PHASE2
#define BASE_PHASE2 {                           \
        FOO(kpps.r_pp_bb);                      \
        FOO(kpps.r_pp_hb);                      \
        FOO(kkps.r_kp_b);                       \
        FOO(kkps.r_kp_h);                       \
        FOO(kks.r_kk);                          \
    }
#else
#define BASE_PHASE2
#endif

#if defined EVAL_PHASE3
#define BASE_PHASE3 {                           \
        FOO(kpps.r_kpp_bb);                     \
        FOO(kpps.r_kpp_hb);                     \
        FOO(kpps.pp);                           \
        FOO(kpps.kpe);                          \
        FOO(kpps.xpe);                          \
        FOO(kpps.ype);                          \
        FOO(kkps.kp);                           \
        FOO(kkps.r_kkp_b);                      \
        FOO(kkps.r_kkp_h);                      \
        FOO(kkps.kke);                          \
        FOO(kks.kk);                            \
    }
#else
#define BASE_PHASE3
#endif

#if defined EVAL_PHASE4
#define BASE_PHASE4 {                           \
        FOO(kpps.kpp);                          \
        FOO(kpps.xpp);                          \
        FOO(kpps.ypp);                          \
        FOO(kkps.kkp);                          \
    }
#else
#define BASE_PHASE4
#endif

#if defined EVAL_ONLINE
#define BASE_ONLINE {                           \
        FOO(kpps.kpp);                          \
        FOO(kkps.kkp);                          \
        FOO(kks.kk);                            \
    }
#else
#define BASE_ONLINE
#endif

#define READ_BASE_EVAL {                        \
        BASE_PHASE1;                            \
        BASE_PHASE2;                            \
        BASE_PHASE3;                            \
        BASE_PHASE4;                            \
        BASE_ONLINE;                            \
    }
#define WRITE_BASE_EVAL {                       \
        BASE_PHASE1;                            \
        BASE_PHASE2;                            \
        BASE_PHASE3;                            \
        BASE_PHASE4;                            \
        BASE_ONLINE;                            \
    }
    void read(const std::string& dirName) {
#define FOO(x) {                                                        \
            std::ifstream ifs((addSlashIfNone(dirName) + #x ".bin").c_str(), std::ios::binary); \
            ifs.read(reinterpret_cast<char*>(x), sizeof(x));            \
        }
        READ_BASE_EVAL;
#undef FOO
    }
    void write(const std::string& dirName) {
#define FOO(x) {                                                        \
            std::ofstream ofs((addSlashIfNone(dirName) + #x ".bin").c_str(), std::ios::binary); \
            ofs.write(reinterpret_cast<char*>(x), sizeof(x));           \
        }
        WRITE_BASE_EVAL;
#undef FOO
    }
#undef READ_BASE_EVAL
#undef WRITE_BASE_EVAL
};

extern const int kppArray[31];
extern const int kkpArray[15];
extern const int kppHandArray[ColorNum][HandPieceNum];

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
        std::array<std::array<s32, 2>, 3> p;
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
