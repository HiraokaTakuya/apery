#include "tt.hpp"

void TranspositionTable::setSize(const size_t mbSize) { // Mega Byte 指定
	// 確保する要素数を取得する。
	size_t newSize = (mbSize << 20) / sizeof(TTCluster);
	newSize = std::max(static_cast<size_t>(1024), newSize); // 最小値は 1024 としておく。
	// 確保する要素数は 2 のべき乗である必要があるので、MSB以外を捨てる。
	const int msbIndex = 63 - firstOneFromMSB(static_cast<u64>(newSize));
	newSize = UINT64_C(1) << msbIndex;

	if (newSize == this->size())
		// 現在と同じサイズなら何も変更する必要がない。
		return;

	size_ = newSize;
	delete [] entries_;
	entries_ = new (std::nothrow) TTCluster[newSize];
	if (!entries_) {
		std::cerr << "Failed to allocate transposition table: " << mbSize << "MB";
		exit(EXIT_FAILURE);
	}
	clear();
}

void TranspositionTable::clear() {
	memset(entries_, 0, size() * sizeof(TTCluster));
}

void TranspositionTable::store(const Key posKey, const Score score, const Bound bound, Depth depth,
							   Move move, const Score evalScore)
{
	TTEntry* tte = firstEntry(posKey);
	TTEntry* replace = tte;
	const u32 posKeyHigh32 = posKey >> 32;

	if (depth < Depth0)
		depth = Depth0;

	for (int i = 0; i < ClusterSize; ++i, ++tte) {
		// 置換表が空か、keyが同じな古い情報が入っているとき
		if (!tte->key() || tte->key() == posKeyHigh32) {
			// move が無いなら、とりあえず古い情報でも良いので、他の指し手を保存する。
			if (move.isNone())
				move = tte->move();

			tte->save(depth, score, move, posKeyHigh32,
					  bound, this->generation(), evalScore);
			return;
		}

		int c = (replace->generation() == this->generation() ? 2 : 0);
		c    += (tte->generation() == this->generation() || tte->type() == BoundExact ? -2 : 0);
		c    += (tte->depth() < replace->depth() ? 1 : 0);

		if (0 < c)
			replace = tte;
	}
	replace->save(depth, score, move, posKeyHigh32,
				  bound, this->generation(), evalScore);
}

TTEntry* TranspositionTable::probe(const Key posKey) const {
	const Key posKeyHigh32 = posKey >> 32;
	TTEntry* tte = firstEntry(posKey);

	// firstEntry() で、posKey の下位 (size() - 1) ビットを hash key に使用した。
	// ここでは posKey の上位 32bit が 保存されている hash key と同じか調べる。
	for (int i = 0; i < ClusterSize; ++i, ++tte) {
		if (tte->key() == posKeyHigh32)
			return tte;
	}
	return nullptr;
}
