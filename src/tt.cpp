#include "tt.hpp"

void TranspositionTable::setSize(const size_t mbSize) { // Mega Byte 指定
	// 確保する要素数を取得する。
	size_t newClusterCount = (mbSize << 20) / sizeof(TTCluster);
	newClusterCount = std::max(static_cast<size_t>(1024), newClusterCount); // 最小値は 1024 としておく。
	// 確保する要素数は 2 のべき乗である必要があるので、MSB以外を捨てる。
	const int msbIndex = 63 - firstOneFromMSB(static_cast<u64>(newClusterCount));
	newClusterCount = UINT64_C(1) << msbIndex;

	if (newClusterCount == clusterCount_)
		// 現在と同じサイズなら何も変更する必要がない。
		return;

	clusterCount_ = newClusterCount;
	free(mem_);
	mem_ = calloc(newClusterCount * sizeof(TTCluster) + CacheLineSize - 1, 1);
	if (!mem_) {
		std::cerr << "Failed to allocate transposition table: " << mbSize << "MB";
		exit(EXIT_FAILURE);
	}
	table_ = reinterpret_cast<TTCluster*>((uintptr_t(mem_) + CacheLineSize - 1) & ~(CacheLineSize - 1));
}

void TranspositionTable::clear() {
	memset(table_, 0, clusterCount_ * sizeof(TTCluster));
}

TTEntry* TranspositionTable::probe(const Key posKey, bool& found) const {
	const Key posKeyHigh16 = posKey >> 48;
	TTEntry* const tte = firstEntry(posKey);

	// firstEntry() で、posKey の下位 (size() - 1) ビットを hash key に使用した。
	// ここでは posKey の上位 32bit が 保存されている hash key と同じか調べる。
	for (int i = 0; i < ClusterSize; ++i) {
		if (!tte[i].key() || tte[i].key() == posKeyHigh16) {
			if (tte[i].generation() != generation() && tte[i].key())
				tte[i].genBound8_ = generation() | tte[i].bound();
			found = static_cast<bool>(tte[i].key());
			return &tte[i];
		}
	}
	TTEntry* replace = tte;
	for (int i = 1; i < ClusterSize; ++i) {
		if (replace->depth() - ((259 + generation() - replace->genBound8_) & 0xfc) * 2 * OnePly
			> tte[i].depth() - ((259 + generation() -   tte[i].genBound8_) & 0xfc) * 2 * OnePly)
		{
			replace = &tte[i];
		}
	}
	found = false;
	return replace;
}
