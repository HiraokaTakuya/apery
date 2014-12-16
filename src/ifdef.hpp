#ifndef IFDEF_HPP
#define IFDEF_HPP

#if 0
#define FIND_MAGIC
#endif

#if 0
#define LEARN
#endif

#if 0
#define BAN_BLACK_REPETITION
#elif 0
#define BAN_WHITE_REPETITION
#endif

#if 0
#define MINIMUL
#endif

#if 0
#define INANIWA_SHIFT
#endif

#if 1
#define USE_KING_SQUARE_SCORE
#endif

#if defined (HAVE_SSE2) || defined (HAVE_SSE4)
#if 0
#define SSE_EVAL
#endif
#endif

#if 0
#define USE_RELATIVE
#endif

#if 1
#define MAKE_SEARCHED_BOOK
#endif

#if 0
#define DENOUSEN_FINAL
#endif

#endif // #ifndef IFDEF_HPP
