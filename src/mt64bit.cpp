#include "mt64bit.hpp"

//MT64bit g_mt64bit(std::chrono::system_clock::now().time_since_epoch().count());
MT64bit g_mt64bit; // seed が固定値である必要は特に無い。
