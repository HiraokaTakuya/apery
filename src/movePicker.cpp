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

#include "movePicker.hpp"
#include "generateMoves.hpp"
#include "thread.hpp"

MovePicker::MovePicker(const Position& pos, const Move ttm, const Score th) : pos_(pos), threshold_(th) {
    assert(!pos.inCheck());
    moves_[0].score = std::numeric_limits<std::underlying_type<Score>::type>::max(); // 番兵のセット
    stage_ = Probcut;

    ttMove_ = (ttm
               && pos.moveIsPseudoLegal(ttm)
               && ttm.isCaptureOrPawnPromotion()
               && pos.see(ttm) > threshold_ ? ttm : Move::moveNone());
    stage_ += (ttMove_ == Move::moveNone());
}

MovePicker::MovePicker(const Position& pos, const Move ttm, const Depth depth, const Square sq) : pos_(pos) {
    assert(depth <= Depth0);
    moves_[0].score = std::numeric_limits<std::underlying_type<Score>::type>::max(); // 番兵のセット
    if (pos.inCheck())
        stage_ = EvasionSearch;
#if defined USE_QCHECKS
    else if (depth > DepthQNoChecks)
        stage_ = QSearchWithChecks;
#endif
    else if (depth > DepthQRecaptures)
        stage_ = QSearchNoChecks;
    else {
        stage_ = QSearchRecaptures;
        recaptureSquare_ = sq;
        return;
    }
    ttMove_ = ttm && pos.moveIsPseudoLegal(ttm) ? ttm : Move::moveNone();
    stage_ += (ttMove_ == Move::moveNone());
}

MovePicker::MovePicker(const Position& pos, const Move ttm, const Depth depth, SearchStack* searchStack)
    : pos_(pos), ss_(searchStack), depth_(depth)
{
    assert(depth > Depth0);
    moves_[0].score = std::numeric_limits<std::underlying_type<Score>::type>::max(); // 番兵のセット
    const Square prevSq = (ss_-1)->currentMove.to();
    counterMove_ = pos.thisThread()->counterMoves[pos.piece(prevSq)][prevSq];

    stage_ = pos.inCheck() ? EvasionSearch : MainSearch;
    ttMove_ = ttm && pos.moveIsPseudoLegal(ttm) ? ttm : Move::moveNone();
    stage_ += (ttMove_ == Move::moveNone());
}

Move MovePicker::nextMove() {
    Move move;
    switch (stage_) {
    case MainSearch: case EvasionSearch:
#if defined USE_QCHECKS
    case QSearchWithChecks:
#endif
    case QSearchNoChecks: case Probcut:
        ++stage_;
        return ttMove_;
    case TacticalInit:
        endBadCaptures_ = cur_ = first();
        endMoves_ = generateMoves<CapturePlusPro>(cur_, pos_);
        scoreCaptures();
        ++stage_;
        // fallthrough
    case GoodTacticals:
        while (cur_ < endMoves_) {
            move = pickBest(cur_++, endMoves_);
            if (move != ttMove_) {
                if (pos_.seeSign(move) >= ScoreZero)
                    return move;
                (*endBadCaptures_++).move = move;
            }
        }
        ++stage_;
        move = ss_->killers[0];
        if (move != Move::moveNone()
            && move != ttMove_
            && pos_.moveIsPseudoLegal(move)
            && pos_.piece(move.to()) == Empty)
        {
            return move;
        }
        // fallthrough
    case Killers:
        ++stage_;
        move = ss_->killers[1];
        if (move != Move::moveNone()
            && move != ttMove_
            && pos_.moveIsPseudoLegal(move)
            && pos_.piece(move.to()) == Empty)
        {
            return move;
        }
        // fallthrough
    case Countermove:
        ++stage_;
        move = counterMove_;
        if (move != Move::moveNone()
            && move != ttMove_
            && move != ss_->killers[0]
            && move != ss_->killers[1]
            && pos_.moveIsPseudoLegal(move)
            && pos_.piece(move.to()) == Empty)
        {
            return move;
        }
        // fallthrough
    case QuietInit:
        cur_ = endBadCaptures_;
        endMoves_ = generateMoves<NonCaptureMinusPro>(cur_, pos_);
        scoreNonCapturesMinusPro<false>();
        cur_ = endMoves_;
        endMoves_ = generateMoves<Drop>(cur_, pos_);
        scoreNonCapturesMinusPro<true>();
        cur_ = endBadCaptures_;
        if (depth_ < 3 * OnePly) {
            ExtMove* goodQuiet = std::partition(cur_, endMoves_, [](const ExtMove& m) { return m.score > ScoreZero; });
            insertionSort<ExtMove*, false>(cur_, goodQuiet);
        }
        else
            insertionSort<ExtMove*, false>(cur_, endMoves_);
        ++stage_;
        // fallthrough
    case Quiet:
        while (cur_ < endMoves_) {
            move = (*cur_++).move;
            if (move != ttMove_
                && move != ss_->killers[0]
                && move != ss_->killers[1]
                && move != counterMove_)
            {
                return move;
            }
        }
        ++stage_;
        cur_ = first(); // Point to beginning of bad captures
        // fallthrough
    case BadCaptures:
        if (cur_ < endBadCaptures_)
            return (*cur_++).move;
        break;
    case EvasionsInit:
        cur_ = first();
        endMoves_ = generateMoves<Evasion>(cur_, pos_);
        scoreEvasions();
        ++stage_;
        // fallthrough
    case AllEvasions:
        while (cur_ < endMoves_) {
            move = pickBest(cur_++, endMoves_);
            if (move != ttMove_)
                return move;
        }
        break;
    case ProbcutInit:
        cur_ = first();
        endMoves_ = generateMoves<CapturePlusPro>(cur_, pos_);
        scoreCaptures();
        ++stage_;
        // fallthrough
    case ProbcutCaptures:
        while (cur_ < endMoves_) {
            move = pickBest(cur_++, endMoves_);
            if (move != ttMove_
                && pos_.see(move) > threshold_)
            {
                return move;
            }
        }
        break;
#if defined USE_QCHECKS
    case QCaptures1Init:
#endif
    case QCaptures2Init:
        cur_ = first();
        endMoves_ = generateMoves<CapturePlusPro>(cur_, pos_);
        scoreCaptures();
        ++stage_;
        // fallthrough
    case QCaptures2:
#if defined USE_QCHECKS
    case QCaptures1:
#endif
        while (cur_ < endMoves_) {
            move = pickBest(cur_++, endMoves_);
            if (move != ttMove_)
                return move;
        }
#if defined USE_QCHECKS
        if (stage_ == QCaptures2)
            break;
        cur_ = first();
        endMoves_ = generateMoves<QuietChecks>(cur_, pos_);
        ++stage_;
        // fallthrough
#else
        break;
#endif
#if defined USE_QCHECKS
    case QChecks:
        while (cur_ < endMoves_) {
            move = cur_++->move;
            if (move != ttMove_)
                return move;
        }
        break;
#endif
    case QSearchRecaptures:
        cur_ = first();
        endMoves_ = generateMoves<Recapture>(cur_, pos_, recaptureSquare_);
        scoreCaptures();
        ++stage_;
        // fallthrough
    case QRecaptures:
        while (cur_ < endMoves_) {
            move = pickBest(cur_++, endMoves_);
            return move;
        }
        break;
    default: UNREACHABLE;
    }
    return Move::moveNone();
}

const Score LVATable[PieceTypeNum] = {
    Score(0), Score(1), Score(2), Score(3), Score(4), Score(7), Score(8), Score(6), Score(10000),
    Score(5), Score(5), Score(5), Score(5), Score(9), Score(10)
};
inline Score LVA(const PieceType pt) { return LVATable[pt]; }

void MovePicker::scoreCaptures() {
    for (ExtMove& m : *this) {
        assert(!m.move.isDrop());
        m.score = Position::pieceScore(pos_.piece(m.move.to())) - LVA(m.move.pieceTypeFrom());
    }
}

template <bool IsDrop> void MovePicker::scoreNonCapturesMinusPro() {
    const HistoryStats& history = pos_.thisThread()->history;
    const FromToStats& fromTo = pos_.thisThread()->fromTo;

    const CounterMoveStats* cm = (ss_-1)->counterMoves;
    const CounterMoveStats* fm = (ss_-2)->counterMoves;
    const CounterMoveStats* f2 = (ss_-4)->counterMoves;

    const Color c = pos_.turn();

    for (auto& m : *this) {
        const Piece movedPiece = pos_.movedPiece(m.move);
        const Square to = m.move.to();
        m.score = history[movedPiece][to]
            + (cm ? (*cm)[movedPiece][to] : ScoreZero)
            + (fm ? (*fm)[movedPiece][to] : ScoreZero)
            + (f2 ? (*f2)[movedPiece][to] : ScoreZero)
            + fromTo.get(c, m.move);
    }
}

void MovePicker::scoreEvasions() {
    const HistoryStats& history = pos_.thisThread()->history;
    const FromToStats& fromTo = pos_.thisThread()->fromTo;
    Color c = pos_.turn();

    for (ExtMove& m : *this)
        if (m.move.isCaptureOrPawnPromotion()) {
            m.score = pos_.capturePieceScore(pos_.piece(m.move.to())) + HistoryStats::Max;
            if (m.move.isPromotion()) {
                const PieceType pt = pieceToPieceType(pos_.piece(m.move.from()));
                m.score += pos_.promotePieceScore(pt);
            }
        }
        else
            m.score = history[pos_.movedPiece(m.move)][m.move.to()] + fromTo.get(c, m.move);
}
