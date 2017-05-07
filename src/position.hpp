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

#ifndef APERY_POSITION_HPP
#define APERY_POSITION_HPP

#include "piece.hpp"
#include "common.hpp"
#include "hand.hpp"
#include "bitboard.hpp"
#include "pieceScore.hpp"
#include "evalList.hpp"
#include <stack>
#include <memory>

class Position;

enum GameResult : int8_t {
    Draw, BlackWin, WhiteWin, GameResultNum
};

enum RepetitionType {
    NotRepetition, RepetitionDraw, RepetitionWin, RepetitionLose,
    RepetitionSuperior, RepetitionInferior
};

struct CheckInfo {
    explicit CheckInfo(const Position&);
    Bitboard dcBB; // discoverd check candidates bitboard
    Bitboard pinned;
    Bitboard checkBB[PieceTypeNum];
};

struct ChangedListPair {
    int newlist[2];
    int oldlist[2];
};

struct ChangedLists {
    ChangedListPair clistpair[2]; // 一手で動く駒は最大2つ。(動く駒、取られる駒)
    int listindex[2]; // 一手で動く駒は最大2つ。(動く駒、取られる駒)
    size_t size;
};

struct StateInfo {
    // Copied when making a move
    Score material; // stocfish の npMaterial は 先手、後手の点数を配列で持っているけど、
                    // 特に分ける必要は無い気がする。
    int pliesFromNull;
    int continuousCheck[ColorNum]; // Stockfish には無い。

    // Not copied when making a move (will be recomputed anyhow)
    Key boardKey;
    Key handKey;
    Bitboard checkersBB; // 手番側の玉へ check している駒の Bitboard
#if 0
    Piece capturedPiece;
#endif
    StateInfo* previous;
    Hand hand; // 手番側の持ち駒
    ChangedLists cl;

    Key key() const { return boardKey + handKey; }
};

using StateListPtr = std::unique_ptr<std::deque<StateInfo>>;

class BitStream {
public:
    // 読み込む先頭データのポインタをセットする。
    BitStream(u8* d) : data_(d), curr_() {}
    // 読み込む先頭データのポインタをセットする。
    void set(u8* d) {
        data_ = d;
        curr_ = 0;
    }
    // １ bit 読み込む。どこまで読み込んだかを表す bit の位置を 1 個進める。
    u8 getBit() {
        const u8 result = (*data_ & (1 << curr_++)) ? 1 : 0;
        if (curr_ == 8) {
            ++data_;
            curr_ = 0;
        }
        return result;
    }
    // numOfBits bit読み込む。どこまで読み込んだかを表す bit の位置を numOfBits 個進める。
    u8 getBits(const int numOfBits) {
        assert(numOfBits <= 8);
        u8 result = 0;
        for (int i = 0; i < numOfBits; ++i)
            result |= getBit() << i;
        return result;
    }
    // 1 bit 書き込む。
    void putBit(const u8 bit) {
        assert(bit <= 1);
        *data_ |= bit << curr_++;
        if (curr_ == 8) {
            ++data_;
            curr_ = 0;
        }
    }
    // val の値を numOfBits bit 書き込む。8 bit まで。
    void putBits(u8 val, const int numOfBits) {
        assert(numOfBits <= 8);
        for (int i = 0; i < numOfBits; ++i) {
            const u8 bit = val & 1;
            val >>= 1;
            putBit(bit);
        }
    }
    u8* data() const { return data_; }
    int curr() const { return curr_; }

private:
    u8* data_;
    int curr_; // 1byte 中の bit の位置
};

union HuffmanCode {
    struct {
        u8 code;      // 符号化時の bit 列
        u8 numOfBits; // 使用 bit 数
    };
    u16 key; // std::unordered_map の key として使う。
};

struct HuffmanCodeToPieceHash : public std::unordered_map<u16, Piece> {
    Piece value(const u16 key) const {
        const auto it = find(key);
        if (it == std::end(*this))
            return PieceNone;
        return it->second;
    }
};

// Huffman 符号化された局面のデータ構造。256 bit で局面を表す。
struct HuffmanCodedPos {
    static const HuffmanCode boardCodeTable[PieceNone];
    static const HuffmanCode handCodeTable[HandPieceNum][ColorNum];
    static HuffmanCodeToPieceHash boardCodeToPieceHash;
    static HuffmanCodeToPieceHash handCodeToPieceHash;
    static void init() {
        for (Piece pc = Empty; pc <= BDragon; ++pc)
            if (pieceToPieceType(pc) != King) // 玉は位置で符号化するので、駒の種類では符号化しない。
                boardCodeToPieceHash[boardCodeTable[pc].key] = pc;
        for (Piece pc = WPawn; pc <= WDragon; ++pc)
            if (pieceToPieceType(pc) != King) // 玉は位置で符号化するので、駒の種類では符号化しない。
                boardCodeToPieceHash[boardCodeTable[pc].key] = pc;
        for (HandPiece hp = HPawn; hp < HandPieceNum; ++hp)
            for (Color c = Black; c < ColorNum; ++c)
                handCodeToPieceHash[handCodeTable[hp][c].key] = colorAndPieceTypeToPiece(c, handPieceToPieceType(hp));
    }
    void clear() { std::fill(std::begin(data), std::end(data), 0); }

    u8 data[32];
};
static_assert(sizeof(HuffmanCodedPos) == 32, "");

struct HuffmanCodedPosAndEval {
    HuffmanCodedPos hcp;
    s16 eval;
    u16 bestMove16; // 使うかは分からないが教師データ生成時についでに取得しておく。
    GameResult gameResult; // 自己対局で勝ったかどうか。
};
static_assert(sizeof(HuffmanCodedPosAndEval) == 38, "");

class Move;
struct Thread;
struct Searcher;

class Position {
public:
    Position() {}
    explicit Position(Searcher* s) : searcher_(s) {}
    Position(const Position& pos) { *this = pos; }
    Position(const Position& pos, Thread* th) {
        *this = pos;
        thisThread_ = th;
    }
    Position(const std::string& sfen, Thread* th, Searcher* s) {
        set(sfen, th);
        setSearcher(s);
    }

    Position& operator = (const Position& pos);
    void set(const std::string& sfen, Thread* th);
    bool set(const HuffmanCodedPos& hcp, Thread* th);

    Bitboard bbOf(const PieceType pt) const                                            { return byTypeBB_[pt]; }
    Bitboard bbOf(const Color c) const                                                 { return byColorBB_[c]; }
    Bitboard bbOf(const PieceType pt, const Color c) const                             { return bbOf(pt) & bbOf(c); }
    Bitboard bbOf(const PieceType pt1, const PieceType pt2) const                      { return bbOf(pt1) | bbOf(pt2); }
    Bitboard bbOf(const PieceType pt1, const PieceType pt2, const Color c) const       { return bbOf(pt1, pt2) & bbOf(c); }
    Bitboard bbOf(const PieceType pt1, const PieceType pt2, const PieceType pt3) const { return bbOf(pt1, pt2) | bbOf(pt3); }
    Bitboard bbOf(const PieceType pt1, const PieceType pt2, const PieceType pt3, const PieceType pt4) const {
        return bbOf(pt1, pt2, pt3) | bbOf(pt4);
    }
    Bitboard bbOf(const PieceType pt1, const PieceType pt2, const PieceType pt3,
                  const PieceType pt4, const PieceType pt5) const
    {
        return bbOf(pt1, pt2, pt3, pt4) | bbOf(pt5);
    }
    Bitboard occupiedBB() const { return bbOf(Occupied); }
    // emptyBB() よりもわずかに速いはず。
    // emptyBB() とは異なり、全く使用しない位置(0 から数えて、right の 63bit目、left の 18 ~ 63bit目)
    // の bit が 1 になっても構わないとき、こちらを使う。
    // todo: SSEにビット反転が無いので実はそんなに速くないはず。不要。
    Bitboard nOccupiedBB() const          { return ~occupiedBB(); }
    Bitboard emptyBB() const              { return occupiedBB() ^ allOneBB(); }
    // 金、成り金 の Bitboard
    Bitboard goldsBB() const              { return goldsBB_; }
    Bitboard goldsBB(const Color c) const { return goldsBB() & bbOf(c); }

    Piece piece(const Square sq) const    { return piece_[sq]; }

    // hand
    Hand hand(const Color c) const { return hand_[c]; }

    // turn() 側が pin されている Bitboard を返す。
    // checkersBB が更新されている必要がある。
    Bitboard pinnedBB() const { return hiddenCheckers<true, true>(); }
    // 間の駒を動かすことで、turn() 側が空き王手が出来る駒のBitboardを返す。
    // checkersBB が更新されている必要はない。
    // BetweenIsUs == true  : 間の駒が自駒。
    // BetweenIsUs == false : 間の駒が敵駒。
    template <bool BetweenIsUs = true> Bitboard discoveredCheckBB() const { return hiddenCheckers<false, BetweenIsUs>(); }

    // toFile と同じ筋に us の歩がないなら true
    bool noPawns(const Color us, const File toFile) const { return !bbOf(Pawn, us).andIsAny(fileMask(toFile)); }
    bool isPawnDropCheckMate(const Color us, const Square sq) const;
    // Pinされているfromの駒がtoに移動出来なければtrueを返す。
    template <bool IsKnight = false>
    bool isPinnedIllegal(const Square from, const Square to, const Square ksq, const Bitboard& pinned) const {
        // 桂馬ならどこに動いても駄目。
        return pinned.isSet(from) && (IsKnight || !isAligned<true>(from, to, ksq));
    }
    // 空き王手かどうか。
    template <bool IsKnight = false>
    bool isDiscoveredCheck(const Square from, const Square to, const Square ksq, const Bitboard& dcBB) const {
        // 桂馬ならどこに動いても空き王手になる。
        return dcBB.isSet(from) && (IsKnight || !isAligned<true>(from, to, ksq));
    }

    Bitboard checkersBB() const     { return st_->checkersBB; }
    Bitboard prevCheckersBB() const { return st_->previous->checkersBB; }
    // 王手が掛かっているか。
    bool inCheck() const            { return checkersBB().isAny(); }

    Score material() const { return st_->material; }
    Score materialDiff() const { return st_->material - st_->previous->material; }

    FORCE_INLINE Square kingSquare(const Color c) const {
        assert(kingSquare_[c] == bbOf(King, c).constFirstOneFromSQ11());
        return kingSquare_[c];
    }

    bool moveGivesCheck(const Move m) const;
    bool moveGivesCheck(const Move move, const CheckInfo& ci) const;
    Piece movedPiece(const Move m) const;

    // attacks
    Bitboard attackersTo(const Square sq, const Bitboard& occupied) const;
    Bitboard attackersTo(const Color c, const Square sq) const { return attackersTo(c, sq, occupiedBB()); }
    Bitboard attackersTo(const Color c, const Square sq, const Bitboard& occupied) const;
    Bitboard attackersToExceptKing(const Color c, const Square sq) const;
    // todo: 利きをデータとして持ったとき、attackersToIsAny() を高速化すること。
    bool attackersToIsAny(const Color c, const Square sq) const { return attackersTo(c, sq).isAny(); }
    bool attackersToIsAny(const Color c, const Square sq, const Bitboard& occupied) const {
        return attackersTo(c, sq, occupied).isAny();
    }
    // 移動王手が味方の利きに支えられているか。false なら相手玉で取れば詰まない。
    bool unDropCheckIsSupported(const Color c, const Square sq) const { return attackersTo(c, sq).isAny(); }
    // 利きの生成

    // 任意の occupied に対する利きを生成する。
    template <PieceType PT> static Bitboard attacksFrom(const Color c, const Square sq, const Bitboard& occupied);
    // 任意の occupied に対する利きを生成する。
    template <PieceType PT> Bitboard attacksFrom(const Square sq, const Bitboard& occupied) const {
        static_assert(PT == Bishop || PT == Rook || PT == Horse || PT == Dragon, "");
        // Color は何でも良い。
        return attacksFrom<PT>(ColorNum, sq, occupied);
    }

    template <PieceType PT> Bitboard attacksFrom(const Color c, const Square sq) const {
        static_assert(PT == Gold, ""); // Gold 以外は template 特殊化する。
        return goldAttack(c, sq);
    }
    template <PieceType PT> Bitboard attacksFrom(const Square sq) const {
        static_assert(PT == Bishop || PT == Rook || PT == King || PT == Horse || PT == Dragon, "");
        // Color は何でも良い。
        return attacksFrom<PT>(ColorNum, sq);
    }
    Bitboard attacksFrom(const PieceType pt, const Color c, const Square sq) const { return attacksFrom(pt, c, sq, occupiedBB()); }
    static Bitboard attacksFrom(const PieceType pt, const Color c, const Square sq, const Bitboard& occupied);

    // 次の手番
    Color turn() const { return turn_; }

    // pseudoLegal とは
    // ・玉が相手駒の利きがある場所に移動する
    // ・pin の駒を移動させる
    // ・連続王手の千日手の手を指す
    // これらの反則手を含めた手の事と定義する。
    // よって、打ち歩詰めや二歩の手は pseudoLegal では無い。
    template <bool MUSTNOTDROP, bool FROMMUSTNOTBEKING>
    bool pseudoLegalMoveIsLegal(const Move move, const Bitboard& pinned) const;
    bool pseudoLegalMoveIsEvasion(const Move move, const Bitboard& pinned) const;
    template <bool Searching = true> bool moveIsPseudoLegal(const Move move) const;
#if !defined NDEBUG
    bool moveIsLegal(const Move move) const;
#endif

    void doMove(const Move move, StateInfo& newSt);
    void doMove(const Move move, StateInfo& newSt, const CheckInfo& ci, const bool moveIsCheck);
    void undoMove(const Move move);
    template <bool DO> void doNullMove(StateInfo& backUpSt);

    Score see(const Move move, const int asymmThreshold = 0) const;
    Score seeSign(const Move move) const;

    template <Color US> Move mateMoveIn1Ply();
    Move mateMoveIn1Ply();

    Ply gamePly() const         { return gamePly_; }

    Key getBoardKey() const     { return st_->boardKey; }
    Key getHandKey() const      { return st_->handKey; }
    Key getKey() const          { return st_->key(); }
    Key getExclusionKey() const { return st_->key() ^ zobExclusion_; }
    Key getKeyExcludeTurn() const {
        static_assert(zobTurn_ == 1, "");
        return getKey() >> 1;
    }
    void print() const;
    std::string toSFEN(const Ply ply) const;
    std::string toSFEN() const { return toSFEN(gamePly()); }

    HuffmanCodedPos toHuffmanCodedPos() const;

    s64 nodesSearched() const          { return nodes_; }
    void setNodesSearched(const s64 n) { nodes_ = n; }
    RepetitionType isDraw(const int checkMaxPly = std::numeric_limits<int>::max()) const;

    Thread* thisThread() const { return thisThread_; }

    void setStartPosPly(const Ply ply) { gamePly_ = ply; }

    static constexpr int nlist() { return EvalList::ListSize; }
    int list0(const int index) const { return evalList_.list0[index]; }
    int list1(const int index) const { return evalList_.list1[index]; }
    int squareHandToList(const Square sq) const { return evalList_.squareHandToList[sq]; }
    Square listToSquareHand(const int i) const { return evalList_.listToSquareHand[i]; }
    int* plist0() { return &evalList_.list0[0]; }
    int* plist1() { return &evalList_.list1[0]; }
    const int* cplist0() const { return &evalList_.list0[0]; }
    const int* cplist1() const { return &evalList_.list1[0]; }
    const ChangedLists& cl() const { return st_->cl; }

    const Searcher* csearcher() const { return searcher_; }
    Searcher* searcher() const { return searcher_; }
    void setSearcher(Searcher* s) { searcher_ = s; }

#if !defined NDEBUG
    // for debug
    bool isOK() const;
#endif

    static void initZobrist();

    static Score pieceScore(const Piece pc)            { return PieceScore[pc]; }
    // Piece を index としても、 PieceType を index としても、
    // 同じ値が取得出来るようにしているので、PieceType => Piece への変換は必要ない。
    static Score pieceScore(const PieceType pt)        { return PieceScore[pt]; }
    static Score capturePieceScore(const Piece pc)     { return CapturePieceScore[pc]; }
    // Piece を index としても、 PieceType を index としても、
    // 同じ値が取得出来るようにしているので、PieceType => Piece への変換は必要ない。
    static Score capturePieceScore(const PieceType pt) { return CapturePieceScore[pt]; }
    static Score promotePieceScore(const PieceType pt) {
        assert(pt < Gold);
        return PromotePieceScore[pt];
    }

private:
    void clear();
    void setPiece(const Piece piece, const Square sq) {
        const Color c = pieceToColor(piece);
        const PieceType pt = pieceToPieceType(piece);

        piece_[sq] = piece;

        byTypeBB_[pt].setBit(sq);
        byColorBB_[c].setBit(sq);
        byTypeBB_[Occupied].setBit(sq);
    }
    void setHand(const HandPiece hp, const Color c, const int num) { hand_[c].orEqual(num, hp); }
    void setHand(const Piece piece, const int num) {
        const Color c = pieceToColor(piece);
        const PieceType pt = pieceToPieceType(piece);
        const HandPiece hp = pieceTypeToHandPiece(pt);
        setHand(hp, c, num);
    }

    // 手番側の玉へ check している駒を全て探して checkersBB_ にセットする。
    // 最後の手が何か覚えておけば、attackersTo() を使用しなくても良いはずで、処理が軽くなる。
    void findCheckers() { st_->checkersBB = attackersToExceptKing(oppositeColor(turn()), kingSquare(turn())); }

    Score computeMaterial() const;

    void xorBBs(const PieceType pt, const Square sq, const Color c);
    // turn() 側が
    // pin されて(して)いる駒の Bitboard を返す。
    // BetweenIsUs == true  : 間の駒が自駒。
    // BetweenIsUs == false : 間の駒が敵駒。
    template <bool FindPinned, bool BetweenIsUs> Bitboard hiddenCheckers() const {
        Bitboard result = allZeroBB();
        const Color us = turn();
        const Color them = oppositeColor(us);
        // pin する遠隔駒
        // まずは自駒か敵駒かで大雑把に判別
        Bitboard pinners = bbOf(FindPinned ? them : us);

        const Square ksq = kingSquare(FindPinned ? us : them);

        // 障害物が無ければ玉に到達出来る駒のBitboardだけ残す。
        pinners &= (bbOf(Lance) & lanceAttackToEdge((FindPinned ? us : them), ksq)) |
            (bbOf(Rook, Dragon) & rookAttackToEdge(ksq)) | (bbOf(Bishop, Horse) & bishopAttackToEdge(ksq));

        while (pinners) {
            const Square sq = pinners.firstOneFromSQ11();
            // pin する遠隔駒と玉の間にある駒の位置の Bitboard
            const Bitboard between = betweenBB(sq, ksq) & occupiedBB();

            // pin する遠隔駒と玉の間にある駒が1つで、かつ、引数の色のとき、その駒は(を) pin されて(して)いる。
            if (between
                && between.isOneBit<false>()
                && between.andIsAny(bbOf(BetweenIsUs ? us : them)))
            {
                result |= between;
            }
        }

        return result;
    }

#if !defined NDEBUG
    int debugSetEvalList() const;
#endif
    void setEvalList() { evalList_.set(*this); }

    Key computeBoardKey() const;
    Key computeHandKey() const;
    Key computeKey() const { return computeBoardKey() + computeHandKey(); }

    void printHand(const Color c) const;

    static Key zobrist(const PieceType pt, const Square sq, const Color c) { return zobrist_[pt][sq][c]; }
    static Key zobTurn()                                                   { return zobTurn_; }
    static Key zobHand(const HandPiece hp, const Color c)                  { return zobHand_[hp][c]; }

    // byTypeBB は敵、味方の駒を区別しない。
    // byColorBB は駒の種類を区別しない。
    Bitboard byTypeBB_[PieceTypeNum];
    Bitboard byColorBB_[ColorNum];
    Bitboard goldsBB_;

    // 各マスの状態
    Piece piece_[SquareNum];
    Square kingSquare_[ColorNum];

    // 手駒
    Hand hand_[ColorNum];
    Color turn_;

    EvalList evalList_;

    StateInfo startState_;
    StateInfo* st_;
    // 時間管理に使用する。
    Ply gamePly_;
    Thread* thisThread_;
    s64 nodes_;

    Searcher* searcher_;

    static Key zobrist_[PieceTypeNum][SquareNum][ColorNum];
    static const Key zobTurn_ = 1;
    static Key zobHand_[HandPieceNum][ColorNum];
    static Key zobExclusion_; // todo: これが必要か、要検討
};

template <> inline Bitboard Position::attacksFrom<Lance >(const Color c, const Square sq, const Bitboard& occupied) { return  lanceAttack(c, sq, occupied); }
template <> inline Bitboard Position::attacksFrom<Bishop>(const Color  , const Square sq, const Bitboard& occupied) { return bishopAttack(   sq, occupied); }
template <> inline Bitboard Position::attacksFrom<Rook  >(const Color  , const Square sq, const Bitboard& occupied) { return   rookAttack(   sq, occupied); }
template <> inline Bitboard Position::attacksFrom<Horse >(const Color  , const Square sq, const Bitboard& occupied) { return  horseAttack(   sq, occupied); }
template <> inline Bitboard Position::attacksFrom<Dragon>(const Color  , const Square sq, const Bitboard& occupied) { return dragonAttack(   sq, occupied); }

template <> inline Bitboard Position::attacksFrom<Pawn  >(const Color c, const Square sq) const { return   pawnAttack(c, sq              ); }
template <> inline Bitboard Position::attacksFrom<Lance >(const Color c, const Square sq) const { return  lanceAttack(c, sq, occupiedBB()); }
template <> inline Bitboard Position::attacksFrom<Knight>(const Color c, const Square sq) const { return knightAttack(c, sq              ); }
template <> inline Bitboard Position::attacksFrom<Silver>(const Color c, const Square sq) const { return silverAttack(c, sq              ); }
template <> inline Bitboard Position::attacksFrom<Bishop>(const Color  , const Square sq) const { return bishopAttack(   sq, occupiedBB()); }
template <> inline Bitboard Position::attacksFrom<Rook  >(const Color  , const Square sq) const { return   rookAttack(   sq, occupiedBB()); }
template <> inline Bitboard Position::attacksFrom<King  >(const Color  , const Square sq) const { return   kingAttack(   sq              ); }
template <> inline Bitboard Position::attacksFrom<Horse >(const Color  , const Square sq) const { return  horseAttack(   sq, occupiedBB()); }
template <> inline Bitboard Position::attacksFrom<Dragon>(const Color  , const Square sq) const { return dragonAttack(   sq, occupiedBB()); }

// position sfen R8/2K1S1SSk/4B4/9/9/9/9/9/1L1L1L3 b PLNSGBR17p3n3g 1
// の局面が最大合法手局面で 593 手。番兵の分、+ 1 しておく。
const int MaxLegalMoves = 593 + 1;

class CharToPieceUSI : public std::map<char, Piece> {
public:
    CharToPieceUSI() {
        (*this)['P'] = BPawn;   (*this)['p'] = WPawn;
        (*this)['L'] = BLance;  (*this)['l'] = WLance;
        (*this)['N'] = BKnight; (*this)['n'] = WKnight;
        (*this)['S'] = BSilver; (*this)['s'] = WSilver;
        (*this)['B'] = BBishop; (*this)['b'] = WBishop;
        (*this)['R'] = BRook;   (*this)['r'] = WRook;
        (*this)['G'] = BGold;   (*this)['g'] = WGold;
        (*this)['K'] = BKing;   (*this)['k'] = WKing;
    }
    Piece value(char c) const      { return this->find(c)->second; }
    bool isLegalChar(char c) const { return (this->find(c) != this->end()); }
};
extern const CharToPieceUSI g_charToPieceUSI;

#endif // #ifndef APERY_POSITION_HPP
