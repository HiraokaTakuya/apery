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

#include "common.hpp"
#include "search.hpp"
#include "usi.hpp"
#include "timeManager.hpp"

namespace {
    enum TimeType {
        OptimumTime, MaxTime
    };

    const int MoveHorizon = 50;
    const double MaxRatio = 7.09;
    const double StealRatio = 0.35;

    double moveImportance(const Ply ply) {
        const double XScale = 7.64;
        const double XShift = 58.4;
        const double Skew   = 0.183;
        return pow((1 + exp((ply - XShift) / XScale)), -Skew) + std::numeric_limits<double>::min();
    }

    template <TimeType T> int remaining(const int myTime, const int movesToGo, const Ply ply, const int slowMover) {
        const float TMaxRatio   = (T == OptimumTime ? 1 : MaxRatio);
        const float TStealRatio = (T == OptimumTime ? 0 : StealRatio);

        const double moveImportanceSlowed = moveImportance(ply) * slowMover / 100;
        double otherMoveImportance = 0;

        for (int i = 1; i < movesToGo; ++i)
            otherMoveImportance += moveImportance(ply + 2 * i);

        const double ratio1 = (TMaxRatio * moveImportanceSlowed) / (TMaxRatio * moveImportanceSlowed + otherMoveImportance);
        const double ratio2 = (moveImportanceSlowed + TStealRatio * otherMoveImportance) / (moveImportanceSlowed + otherMoveImportance);

        return static_cast<int>(myTime * std::min(ratio1, ratio2));
    }
}

void TimeManager::init(LimitsType& limits, const Color us, const Ply ply, const Position& pos, Searcher* s) {
    const int minThinkingTime = s->options["Minimum_Thinking_Time"];
    const int moveOverhead    = s->options["Move_Overhead"];
    const int slowMover       = (pos.gamePly() < 10 ? s->options["Slow_Mover_10"] :
                                 pos.gamePly() < 16 ? s->options["Slow_Mover_16"] :
                                 pos.gamePly() < 20 ? s->options["Slow_Mover_20"] : s->options["Slow_Mover"]);
    const Ply drawPly         = s->options["Draw_Ply"];
    // Draw_Ply までで引き分けになるから、そこまでで時間を使い切る。
    auto moveHorizon = [&](const Ply p) { return std::min(MoveHorizon, drawPly - p); };

    startTime_ = limits.startTime;
    optimumTime_ = maximumTime_ = std::max(limits.time[us], minThinkingTime);

    const int MaxMTG = limits.movesToGo ? std::min(limits.movesToGo, moveHorizon(ply)) : moveHorizon(ply);

    for (int hypMTG = 1; hypMTG <= MaxMTG; ++hypMTG) {
        int hypMyTime = (limits.time[us]
                         + limits.inc[us] * (hypMTG - 1)
                         - moveOverhead * (2 + std::min(hypMTG, 40)));

        hypMyTime = std::max(hypMyTime, 0);

        const int t1 = minThinkingTime + remaining<OptimumTime>(hypMyTime, hypMTG, ply, slowMover);
        const int t2 = minThinkingTime + remaining<MaxTime    >(hypMyTime, hypMTG, ply, slowMover);

        optimumTime_ = std::min(t1, optimumTime_);
        maximumTime_ = std::min(t2, maximumTime_);
    }

    if (s->options["USI_Ponder"])
        optimumTime_ += optimumTime_ / 4;

#if 1
    // 秒読み対応

    // こちらも minThinkingTime 以上にする。
    optimumTime_ = std::max(optimumTime_, minThinkingTime);
    optimumTime_ = std::min(optimumTime_, maximumTime_);

    if (limits.moveTime != 0) {
        if (pos.gamePly() >= 20) {
            if (optimumTime_ < limits.moveTime)
                optimumTime_ = std::min(limits.time[us], limits.moveTime);
            if (maximumTime_ < limits.moveTime)
                maximumTime_ = std::min(limits.time[us], limits.moveTime);
            optimumTime_ += limits.moveTime;
            maximumTime_ += limits.moveTime;
        }
        if (limits.time[us] != 0)
            limits.moveTime = 0;
    }
#endif

    SYNCCOUT << "info string optimum_time = " << optimumTime_ << SYNCENDL;
    SYNCCOUT << "info string maximum_time = " << maximumTime_ << SYNCENDL;
}
