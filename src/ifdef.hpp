#ifndef APERY_IFDEF_HPP
#define APERY_IFDEF_HPP

#if 0
#define FIND_MAGIC
#endif

#if 0
#define LEARN
#if 0
#define MPI_LEARN
#endif
#endif

#if 1 && !defined LEARN
#define USE_GLOBAL
#define STATIC static
#else
#define STATIC
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
#define USE_K_FIX_OFFSET
#endif

#if defined (HAVE_SSE2) || defined (HAVE_SSE4)
#if 0
#define SSE_EVAL
#endif
#endif

#if 1
#define MAKE_SEARCHED_BOOK
#endif

#if 0
#define DENOUSEN_FINAL
#endif

#endif // #ifndef APERY_IFDEF_HPP
