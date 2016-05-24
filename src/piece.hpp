#ifndef APERY_PIECE_HPP
#define APERY_PIECE_HPP

#include "common.hpp"
#include "overloadEnumOperators.hpp"
#include "color.hpp"
#include <iostream>
#include <string>
#include <cassert>

enum PieceType {
	// Pro* は 元の 駒の種類に 8 を加算したもの。
	PTPromote = 8,
	Occupied = 0, // 各 PieceType の or をとったもの。
	Pawn, Lance, Knight, Silver, Bishop, Rook, Gold, King,
	ProPawn, ProLance, ProKnight, ProSilver, Horse, Dragon,
	PieceTypeNum,

	GoldHorseDragon // 単にtemnplate引数として使用
};
OverloadEnumOperators(PieceType);

enum Piece {
	// B* に 16 を加算することで、W* を表す。
	// Promoted を加算することで、成りを表す。
	Empty = 0, UnPromoted = 0, Promoted = 8,
	BPawn = 1, BLance, BKnight, BSilver, BBishop, BRook, BGold, BKing,
	BProPawn, BProLance, BProKnight, BProSilver, BHorse, BDragon, // BDragon = 14
	WPawn = 17, WLance, WKnight, WSilver, WBishop, WRook, WGold, WKing,
	WProPawn, WProLance, WProKnight, WProSilver, WHorse, WDragon,
	PieceNone // PieceNone = 31  これを 32 にした方が多重配列のときに有利か。
};
OverloadEnumOperators(Piece);

inline Piece inverse(const Piece pc) { return static_cast<Piece>(pc ^ 0x10); }

// 持ち駒を表すときに使用する。
// todo: HGold を HRook の後ろに持っていき、PieceType との変換を簡単に出来るようにする。
enum HandPiece {
	HPawn, HLance, HKnight, HSilver, HGold, HBishop, HRook, HandPieceNum
};
OverloadEnumOperators(HandPiece);

// p == Empty のとき、PieceType は OccuPied になってしまうので、
// Position::bbOf(pieceToPieceType(p)) とすると、
// Position::emptyBB() ではなく Position::occupiedBB() になってしまうので、
// 注意すること。出来れば修正したい。
inline PieceType pieceToPieceType(const Piece p) { return static_cast<PieceType>(p & 15); }

inline Color pieceToColor(const Piece p) {
	assert(p != Empty);
	return static_cast<Color>(p >> 4);
}

inline Piece colorAndPieceTypeToPiece(const Color c, const PieceType pt) { return static_cast<Piece>((c << 4) | pt); }

const u32 IsSliderVal = 0x60646064;
// pc が遠隔駒であるか
inline bool isSlider(const Piece     pc) { return (IsSliderVal & (1 << pc)) != 0; }
inline bool isSlider(const PieceType pt) { return (IsSliderVal & (1 << pt)) != 0; }

const HandPiece PieceTypeToHandPieceTable[PieceTypeNum] = {
	HandPieceNum, HPawn, HLance, HKnight, HSilver, HBishop, HRook, HGold, HandPieceNum, HPawn, HLance, HKnight, HSilver, HBishop, HRook
};
inline HandPiece pieceTypeToHandPiece(const PieceType pt) { return PieceTypeToHandPieceTable[pt]; }

const PieceType HandPieceToPieceTypeTable[HandPieceNum] = {
	Pawn, Lance, Knight, Silver, Gold, Bishop, Rook
};
inline PieceType handPieceToPieceType(const HandPiece hp) { return HandPieceToPieceTypeTable[hp]; }
inline Piece colorAndHandPieceToPiece(const Color c, const HandPiece hp) {
	return colorAndPieceTypeToPiece(c, handPieceToPieceType(hp));
}

#endif // #ifndef APERY_PIECE_HPP
