#ifndef TT_HPP
#define TT_HPP

#include "common.hpp"
#include "move.hpp"

enum Depth {
	OnePly                 = 2,
	Depth0                 = 0,
	Depth1                 = 1,
	DepthQChecks           = -1 * OnePly,
	DepthQNoChecks         = -2 * OnePly,
	DepthQRecaptures       = -5 * OnePly,
	DepthNone              = -127 * OnePly
};
OverloadEnumOperators(Depth);

class TTEntry {
public:
	u32   key() const        { return key32_; }
	Depth depth() const      { return static_cast<Depth>(depth16_); }
	Score score() const      { return static_cast<Score>(score16_); }
	Move  move() const       { return static_cast<Move>(move16_); }
	Bound type() const       { return static_cast<Bound>(bound_); }
	u8    generation() const { return generation8_; }
	Score evalScore() const  { return static_cast<Score>(evalScore_); }
	void setGeneration(const u8 g) { generation8_ = g; }

	void save(const Depth depth, const Score score, const Move move,
			  const u32 posKeyHigh32, const Bound bound, const u8 generation,
			  const Score evalScore)
	{
		key32_ = posKeyHigh32;
		move16_ = static_cast<u16>(move.value());
		bound_ = static_cast<u8>(bound);
		generation8_ = generation;
		score16_ = static_cast<s16>(score);
		depth16_ = static_cast<s16>(depth);
		evalScore_ = static_cast<s16>(evalScore);
	}

private:
	u32 key32_;
	u16 move16_;
	u8 bound_;
	u8 generation8_;
	s16 score16_;
	s16 depth16_;
	s16 evalScore_;
};

const int ClusterSize = CacheLineSize / sizeof(TTEntry);
STATIC_ASSERT(0 < ClusterSize);

struct TTCluster {
	TTEntry data[ClusterSize];
};

class TranspositionTable {
public:
	TranspositionTable();
	~TranspositionTable();
	void setSize(const size_t mbSize); // Mega Byte 指定
	void clear();
	void store(const Key posKey, const Score score, const Bound bound, Depth depth,
			   Move move, const Score evalScore);
	TTEntry* probe(const Key posKey) const;
	void newSearch();
	TTEntry* firstEntry(const Key posKey) const;
	void refresh(const TTEntry* tte) const;

	size_t size() const { return size_; }
	TTCluster* entries() const { return entries_; }
	u8 generation() const { return generation_; }

private:
	TranspositionTable(const TranspositionTable&);
	TranspositionTable& operator = (const TranspositionTable&);

	size_t size_; // 置換表のバイト数。2のべき乗である必要がある。
	TTCluster* entries_;
	// iterative deepening していくとき、過去の探索で調べたものかを判定する。
	u8 generation_;
};

inline TranspositionTable::TranspositionTable()
	: size_(0), entries_(nullptr), generation_(0) {}

inline TranspositionTable::~TranspositionTable() {
	delete [] entries_;
}

inline TTEntry* TranspositionTable::firstEntry(const Key posKey) const {
	// (size() - 1) は置換表で使用するバイト数のマスク
	// posKey の下位 (size() - 1) ビットを hash key として使用。
	// ここで posKey の下位ビットの一致を確認。
	// posKey の上位32ビットとの一致は probe, store 内で確認するので、
	// ここでは下位32bit 以上が確認出来れば完璧。
	// 置換表のサイズを小さく指定しているときは下位32bit の一致は確認出来ないが、
	// 仕方ない。
	return entries_[posKey & (size() - 1)].data;
}

inline void TranspositionTable::refresh(const TTEntry* tte) const {
	const_cast<TTEntry*>(tte)->setGeneration(this->generation());
}

inline void TranspositionTable::newSearch() {
	++generation_;
}

#endif // #ifndef TT_HPP
