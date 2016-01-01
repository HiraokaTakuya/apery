#include "move.hpp"

namespace {
	const std::string HandPieceToStringTable[HandPieceNum] = {"P*", "L*", "N*", "S*", "G*", "B*", "R*"};
	inline std::string handPieceToString(const HandPiece hp) { return HandPieceToStringTable[hp]; }

	const std::string PieceTypeToStringTable[PieceTypeNum] = {
		"", "FU", "KY", "KE", "GI", "KA", "HI", "KI", "OU", "TO", "NY", "NK", "NG", "UM", "RY"
	};
	inline std::string pieceTypeToString(const PieceType pt) { return PieceTypeToStringTable[pt]; }
}

std::string Move::toUSI() const {
	if (this->isNone()) return "None";

	const Square from = this->from();
	const Square to = this->to();
	if (this->isDrop())
		return handPieceToString(this->handPieceDropped()) + squareToStringUSI(to);
	std::string usi = squareToStringUSI(from) + squareToStringUSI(to);
	if (this->isPromotion()) usi += "+";
	return usi;
}

std::string Move::toCSA() const {
	if (this->isNone()) return "None";

	std::string s = (this->isDrop() ? std::string("00") : squareToStringCSA(this->from()));
	s += squareToStringCSA(this->to()) + pieceTypeToString(this->pieceTypeTo());
	return s;
}
