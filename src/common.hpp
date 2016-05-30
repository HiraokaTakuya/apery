#ifndef APERY_COMMON_HPP
#define APERY_COMMON_HPP

#include "ifdef.hpp"
#include <cinttypes>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <array>
#include <tuple>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cassert>
#include <ctime>
#include <cmath>
#include <cstddef>
//#include <boost/align/aligned_alloc.hpp>

#if defined HAVE_BMI2
#include <immintrin.h>
#endif

#if defined (HAVE_SSE4)
#include <smmintrin.h>
#elif defined (HAVE_SSE2)
#include <emmintrin.h>
#endif

#if !defined(NDEBUG)
// デバッグ時は、ここへ到達してはいけないので、assert でプログラムを止める。
#define UNREACHABLE assert(false)
#elif defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define UNREACHABLE __assume(false)
#elif defined(__INTEL_COMPILER)
// todo: icc も __assume(false) で良いのか？ 一応ビルド出来るけど。
#define UNREACHABLE __assume(false)
#elif defined(__GNUC__) && (4 < __GNUC__ || (__GNUC__ == 4 && 4 < __GNUC_MINOR__))
#define UNREACHABLE __builtin_unreachable()
#else
#define UNREACHABLE assert(false)
#endif

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define FORCE_INLINE __forceinline
#elif defined(__INTEL_COMPILER)
#define FORCE_INLINE inline
#elif defined(__GNUC__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

// インラインアセンブリのコメントを使用することで、
// C++ コードのどの部分がアセンブラのどの部分に対応するかを
// 分り易くする。
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define ASMCOMMENT(s)
#elif defined(__INTEL_COMPILER)
#define ASMCOMMENT(s)
#elif defined(__clang__)
#define ASMCOMMENT(s)
#elif defined(__GNUC__)
#define ASMCOMMENT(s) __asm__("#"s)
#else
#define ASMCOMMENT(s)
#endif

#define DEBUGCERR(x) std::cerr << #x << " = " << (x) << " (L" << __LINE__ << ")" << " " << __FILE__ << std::endl;

// bit幅を指定する必要があるときは、以下の型を使用する。
using s8  =  int8_t;
using u8  = uint8_t;
using s16 =  int16_t;
using u16 = uint16_t;
using s32 =  int32_t;
using u32 = uint32_t;
using s64 =  int64_t;
using u64 = uint64_t;

// Binary表記
// Binary<11110>::value とすれば、30 となる。
// 符合なし64bitなので19桁まで表記可能。
template <u64 n> struct Binary {
	static const u64 value = n % 10 + (Binary<n / 10>::value << 1);
};
// template 特殊化
template <> struct Binary<0> {
	static const u64 value = 0;
};

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER) && defined(_WIN64)
#include <intrin.h>
FORCE_INLINE int firstOneFromLSB(const u64 b) {
	unsigned long index;
	_BitScanForward64(&index, b);
	return index;
}
FORCE_INLINE int firstOneFromMSB(const u64 b) {
	unsigned long index;
	_BitScanReverse64(&index, b);
	return 63 - index;
}
#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
FORCE_INLINE int firstOneFromLSB(const u64 b) {
	return __builtin_ctzll(b);
}
FORCE_INLINE int firstOneFromMSB(const u64 b) {
	return __builtin_clzll(b);
}
#else
// firstOneFromLSB() で使用する table
const int BitTable[64] = {
	63, 30, 3, 32, 25, 41, 22, 33, 15, 50, 42, 13, 11, 53, 19, 34, 61, 29, 2,
	51, 21, 43, 45, 10, 18, 47, 1, 54, 9, 57, 0, 35, 62, 31, 40, 4, 49, 5, 52,
	26, 60, 6, 23, 44, 46, 27, 56, 16, 7, 39, 48, 24, 59, 14, 12, 55, 38, 28,
	58, 20, 37, 17, 36, 8
};
// LSB から数えて初めに bit が 1 になるのは何番目の bit かを返す。
// b = 8 だったら 3 を返す。
// b = 0 のとき、63 を返す。
FORCE_INLINE int firstOneFromLSB(const u64 b) {
	const u64 tmp = b ^ (b - 1);
	const u32 old = static_cast<u32>((tmp & 0xffffffff) ^ (tmp >> 32));
	return BitTable[(old * 0x783a9b23) >> 26];
}
// 超絶遅いコードなので後で書き換えること。
FORCE_INLINE int firstOneFromMSB(const u64 b) {
	for (int i = 63; 0 <= i; --i) {
		if (b >> i)
			return 63 - i;
	}
	return 0;
}
#endif

#if defined(HAVE_SSE42)
#include <nmmintrin.h>
inline int count1s(u64 x) {
	return _mm_popcnt_u64(x);
}
#else
inline int count1s(u64 x) //任意の値の1のビットの数を数える。( x is not a const value.)
{
	x = x - ((x >> 1) & UINT64_C(0x5555555555555555));
	x = (x & UINT64_C(0x3333333333333333)) + ((x >> 2) & UINT64_C(0x3333333333333333));
	x = (x + (x >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
	x = x + (x >> 8);
	x = x + (x >> 16);
	x = x + (x >> 32);
	return (static_cast<int>(x)) & 0x0000007f;
}
#endif

// for debug
// 2進表示
template <typename T>
inline std::string putb(const T value, const int msb = sizeof(T)*8 - 1, const int lsb = 0) {
	std::string str;
	u64 tempValue = (static_cast<u64>(value) >> lsb);

	for (int length = msb - lsb + 1; length; --length)
		str += ((tempValue & (UINT64_C(1) << (length - 1))) ? "1" : "0");

	return str;
}

enum SyncCout {
	IOLock,
	IOUnlock
};
std::ostream& operator << (std::ostream& os, SyncCout sc);
#define SYNCCOUT std::cout << IOLock
#define SYNCENDL std::endl << IOUnlock

#if defined LEARN
#undef SYNCCOUT
#undef SYNCENDL
class Eraser {};
extern Eraser SYNCCOUT;
extern Eraser SYNCENDL;
template <typename T> Eraser& operator << (Eraser& temp, const T&) { return temp; }
#endif

// N 回ループを展開させる。t は lambda で書くと良い。
// こんな感じに書くと、lambda がテンプレート引数の数値の分だけ繰り返し生成される。
// Unroller<5>()([&](const int i){std::cout << i << std::endl;});
template <int N> struct Unroller {
	template <typename T> FORCE_INLINE void operator () (T t) {
		Unroller<N-1>()(t);
		t(N-1);
	}
};
template <> struct Unroller<0> {
	template <typename T> FORCE_INLINE void operator () (T) {}
};

const size_t CacheLineSize = 64; // 64byte

// Stockfish ほとんどそのまま
template <typename T> inline void prefetch(T* addr) {
#if defined HAVE_SSE2 || defined HAVE_SSE4
#if defined(__INTEL_COMPILER)
	// これでプリフェッチが最適化で消えるのを防げるらしい。
	__asm__("");
#endif

	// 最低でも sizeof(T) のバイト数分をプリフェッチする。
	// Stockfish は TTCluster が 64byte なのに、なぜか 128byte 分 prefetch しているが、
	// 必要無いと思う。
	char* charAddr = reinterpret_cast<char*>(addr);
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
	Unroller<(sizeof(T) + CacheLineSize - 1)/CacheLineSize>()([&](const int) {
			// 1キャッシュライン分(64byte)のプリフェッチ。
			_mm_prefetch(charAddr, _MM_HINT_T0);
			charAddr += CacheLineSize;
		});
#else
	Unroller<(sizeof(T) + CacheLineSize - 1)/CacheLineSize>()([&](const int) {
			// 1キャッシュライン分(64byte)のプリフェッチ。
			__builtin_prefetch(charAddr);
			charAddr += CacheLineSize;
		});
#endif
#else
	// SSE が使えない時は、_mm_prefetch() とかが使えないので、prefetch無しにする。
	addr = addr; // warning 対策
#endif
}

using Key = u64;

// Size は 2のべき乗であること。
template <typename T, size_t Size>
struct HashTable {
	HashTable() {
		//entries_ = (T*)(boost::alignment::aligned_alloc(sizeof(T), sizeof(T)*Size));
		clear();
	}
	T* operator [] (const Key k) { return entries_ + (static_cast<size_t>(k) & (Size-1)); }
	void clear() { memset(entries_, 0, sizeof(T)*Size); }
	// Size が 2のべき乗であることのチェック
	static_assert((Size & (Size-1)) == 0, "");

private:
	//T* entries_;
	T entries_[Size];
};

// ミリ秒単位の時間を表すクラス
class Timer {
public:
	void restart() { t_ = std::chrono::system_clock::now(); }
	int elapsed() const {
		using std::chrono::duration_cast;
		using std::chrono::milliseconds;
		return static_cast<int>(duration_cast<milliseconds>(std::chrono::system_clock::now() - t_).count());
	}
	static Timer currentTime() {
		Timer t;
		t.restart();
		return t;
	}

private:
	std::chrono::time_point<std::chrono::system_clock> t_;
};

extern std::mt19937_64 g_randomTimeSeed;

#if defined _WIN32 && !defined _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX

struct Mutex {
	Mutex() { InitializeCriticalSection(&cs); }
	~Mutex() { DeleteCriticalSection(&cs); }
	void lock() { EnterCriticalSection(&cs); }
	void unlock() { LeaveCriticalSection(&cs); }

private:
	CRITICAL_SECTION cs;
};
using ConditionVariable = std::condition_variable_any;
#else
using Mutex = std::mutex;
using ConditionVariable = std::condition_variable;
#endif

#if 0
#include <boost/detail/endian.hpp>
template <typename T> inline void reverseEndian(T& r) {
	u8* begin = reinterpret_cast<u8*>(&r);
	u8* end = reinterpret_cast<u8*>(&r) + sizeof(T);
	for (; begin < end; ++begin, --end) {
		std::swap(*begin, *(end - 1));
	}
}
#endif

#endif // #ifndef APERY_COMMON_HPP
