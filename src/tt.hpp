#ifndef APERY_TT_HPP
#define APERY_TT_HPP

#include "common.hpp"
#include "move.hpp"

enum Depth {
	OnePly                 = 2,
	Depth0                 = 0,
	Depth1                 = 1,
	DepthQChecks           = -1 * OnePly,
	DepthQNoChecks         = -2 * OnePly,
	DepthQNoTT             = -3 * OnePly,
	DepthQRecaptures       = -5 * OnePly,
	DepthNone              = -127 * OnePly
};
OverloadEnumOperators(Depth);

class TTEntry {
public:
	u16   key() const        { return key16_; }
	Depth depth() const      { return static_cast<Depth>(depth8_); }
	Score score() const      { return static_cast<Score>(score16_); }
	Move  move() const       { return static_cast<Move>(move16_); }
	Bound bound() const      { return static_cast<Bound>(genBound8_ & 0x3); }
	u8    generation() const { return genBound8_ & 0xfc; }
	Score evalScore() const  { return static_cast<Score>(evalScore_); }

	void save(const Key posKey, const Score score, const Bound bound, const Depth depth,
			  const Move move, const Score evalScore, const u8 generation)
	{
		if (!move.isNone() || (posKey>>48) != key16_)
			move16_ = static_cast<u16>(move.value());

		if ((posKey>>48) != key16_
			|| depth > depth8_ - 4
			|| bound == BoundExact)
		{
			key16_ = static_cast<u16>(posKey>>48);
			score16_ = static_cast<s16>(score);
			evalScore_ = static_cast<s16>(evalScore);
			genBound8_ = static_cast<u8>(generation | bound);
			depth8_ = static_cast<s8>(depth);
		}
	}

private:
    friend class TranspositionTable;

	u16 key16_;
	u16 move16_;
	s16 score16_;
	s16 evalScore_;
	u8 genBound8_;
	s8 depth8_;
};

const int ClusterSize = 3;

struct TTCluster {
	TTEntry entry[ClusterSize];
    s8 padding[2];
};

class TranspositionTable {
public:
	TranspositionTable();
	~TranspositionTable();
	void setSize(const size_t mbSize); // Mega Byte 指定
	void clear();
	TTEntry* probe(const Key posKey, bool& found) const;
	void newSearch();
	TTEntry* firstEntry(const Key posKey) const;

	u8 generation() const { return generation_; }

private:
	TranspositionTable(const TranspositionTable&);
	TranspositionTable& operator = (const TranspositionTable&);

	size_t clusterCount_;
	TTCluster* table_;
	void* mem_;
	// iterative deepening していくとき、過去の探索で調べたものかを判定する。
	u8 generation_;
};

inline TranspositionTable::TranspositionTable()
	: clusterCount_(0), table_(nullptr), mem_(nullptr), generation_(0) {}

inline TranspositionTable::~TranspositionTable() {
	free(mem_);
}

inline TTEntry* TranspositionTable::firstEntry(const Key posKey) const {
	// (clusterCount_ - 1) は置換表で使用するバイト数のマスク
	// posKey の下位 (clusterCount_ - 1) ビットを hash key として使用。
	// ここで posKey の下位ビットの一致を確認。
	// posKey の上位16ビットとの一致は probe 内で確認する。
	return &table_[posKey & (clusterCount_ - 1)].entry[0];
}

inline void TranspositionTable::newSearch() {
	generation_ += 4; // TTEntry::genBound8_ の Bound の部分を書き換えないように。
}

#endif // #ifndef APERY_TT_HPP
