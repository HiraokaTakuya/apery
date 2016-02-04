#ifndef APERY_TIMEMANAGER_HPP
#define APERY_TIMEMANAGER_HPP

#include "evaluate.hpp"

struct LimitsType;

class TimeManager {
public:
	void init(LimitsType& limits, const Ply currentPly, const Color us, Searcher* s);
	void pvInstability(const int currChanges, const int prevChanges);
	int availableTime() const { return optimumSearchTime_ + unstablePVExtraTime_; }
	int maximumTime() const { return maximumSearchTime_; }
	int optimumSearchTime() const { return optimumSearchTime_; } // todo: 後で消す。
	int elapsed() const { return startTime.elapsed(); }

private:
	Timer startTime;
	int optimumSearchTime_;
	int maximumSearchTime_;
	int unstablePVExtraTime_;
};

#endif // #ifndef APERY_TIMEMANAGER_HPP
