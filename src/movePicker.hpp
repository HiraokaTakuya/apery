#ifndef APERY_MOVEPICKER_HPP
#define APERY_MOVEPICKER_HPP

#include "move.hpp"
#include "position.hpp"
#include "search.hpp"

enum GenerateMovePhase {
	MainSearch, PH_TacticalMoves0, PH_Killers, PH_NonTacticalMoves0, PH_NonTacticalMoves1, PH_BadCaptures,
	EvasionSearch, PH_Evasions,
	QSearch, PH_QCaptures0,
	QEvasionSearch, PH_QEvasions,
	ProbCut, PH_TacticalMoves1,
	QRecapture, PH_QCaptures1,
	QSearchNoTT, PH_QCaptures2,
	PH_Stop
};
OverloadEnumOperators(GenerateMovePhase); // ++phase_ の為。

class MovePicker {
public:
	MovePicker(const Position& pos, const Move ttm, const Depth depth,
			   const History& history, SearchStack* searchStack, const Score beta);
	MovePicker(const Position& pos, Move ttm, const Depth depth, const History& history, const Square sq);
	MovePicker(const Position& pos, const Move ttm, const History& history, const PieceType pt);
	Move nextMove();

private:
	void scoreCaptures();
	template <bool IsDrop> void scoreNonCapturesMinusPro();
	void scoreEvasions();
	void goNextPhase();
	MoveStack* firstMove() { return &legalMoves_[1]; } // [0] は番兵
	MoveStack* currMove() const { return currMove_; }
	MoveStack* lastMove() const { return lastMove_; }
	MoveStack* lastNonCapture() const { return lastNonCapture_; }
	MoveStack* endBadCaptures() const { return endBadCaptures_; }

	const Position& pos() const { return pos_; }

	GenerateMovePhase phase() const { return phase_; }
	const History& history() const { return history_; }

	const Position& pos_;
	const History& history_;
	SearchStack* ss_;
	Depth depth_;
	Move ttMove_; // transposition table move
	MoveStack killerMoves_[2];
	Square recaptureSquare_;
	int captureThreshold_; // int で良いのか？
	GenerateMovePhase phase_;
	MoveStack* currMove_;
	MoveStack* lastMove_;
	MoveStack* lastNonCapture_;
	MoveStack* endBadCaptures_;
	// std::array にした方が良さそう。
	MoveStack legalMoves_[MaxLegalMoves];
};

#endif // #ifndef APERY_MOVEPICKER_HPP
