#ifndef APERY_INIT_HPP
#define APERY_INIT_HPP

#include "ifdef.hpp"
#include "common.hpp"
#include "bitboard.hpp"

void initTable();

#if defined FIND_MAGIC
u64 findMagic(const Square sqare, const bool isBishop);
#endif // #if defined FIND_MAGIC

#endif // #ifndef APERY_INIT_HPP
