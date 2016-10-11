COMPILER = g++
#COMPILER = mpicxx
CFLAGS   = -std=c++11 -fno-exceptions -fno-rtti -Wextra -Ofast -MMD -MP -fopenmp
#CFLAGS   += -march=native # make publish の時はここをコメントアウトする必要がある。
LDFLAGS  = -lpthread #-lboost_mpi -lboost_serialization
LIBS     =
INCLUDE  = #-I../include
TARGET   = apery
TARGET_BMI2  = $(TARGET)_bmi2
TARGET_SSE42 = $(TARGET)_sse42
TARGET_SSE41 = $(TARGET)_sse41
TARGET_SSE2  = $(TARGET)_sse2
ifeq ($(OS),Windows_NT)
  LDFLAGS += -static
endif
OBJDIR   = ../obj
ifeq "$(strip $(OBJDIR))" ""
  OBJDIR = ..
endif
#SOURCES  = $(wildcard *.cpp)
SOURCES  = main.cpp bitboard.cpp init.cpp mt64bit.cpp position.cpp evalList.cpp \
           move.cpp movePicker.cpp square.cpp usi.cpp generateMoves.cpp evaluate.cpp \
           search.cpp hand.cpp tt.cpp timeManager.cpp book.cpp benchmark.cpp \
           thread.cpp common.cpp pieceScore.cpp
OBJECTS  = $(addprefix $(OBJDIR)/, $(SOURCES:.cpp=.o))
DEPENDS  = $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS) $(LIBS)
	$(COMPILER) -o $@ $^ $(LDFLAGS) $(CFLAGS)

$(OBJDIR)/%.o: %.cpp
	@[ -d $(OBJDIR) ] || mkdir -p $(OBJDIR)
	$(COMPILER) $(CFLAGS) $(INCLUDE) -o $@ -c $<

all: clean $(TARGET)

bmi2:
	$(MAKE) CFLAGS='$(CFLAGS) -DNDEBUG -DHAVE_SSE4 -DHAVE_SSE42 -DHAVE_BMI2 -msse4.2 -mbmi2 -DHAVE_AVX2 -mavx2' LDFLAGS='$(LDFLAGS) -flto' $(TARGET)

sse:
	$(MAKE) CFLAGS='$(CFLAGS) -DNDEBUG -DHAVE_SSE4 -DHAVE_SSE42 -msse4.2' LDFLAGS='$(LDFLAGS) -flto' $(TARGET)

sse41:
	$(MAKE) CFLAGS='$(CFLAGS) -DNDEBUG -DHAVE_SSE4 -msse4.1' LDFLAGS='$(LDFLAGS) -flto' $(TARGET)

sse2:
	$(MAKE) CFLAGS='$(CFLAGS) -DNDEBUG -DHAVE_SSE2 -msse2' LDFLAGS='$(LDFLAGS) -flto' $(TARGET)

nosse:
	$(MAKE) CFLAGS='$(CFLAGS) -DNDEBUG' LDFLAGS='$(LDFLAGS) -flto' $(TARGET)

prof:
	$(MAKE) CFLAGS='$(CFLAGS) -pg' bmi2

profgen:
	$(MAKE) CFLAGS='$(CFLAGS) -fprofile-generate -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-generate -lgcov' bmi2

profuse:
	$(MAKE) CFLAGS='$(CFLAGS) -fprofile-use -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-use -lgcov' bmi2

profgen_sse:
	$(MAKE) CFLAGS='$(CFLAGS) -fprofile-generate -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-generate -lgcov' sse

profuse_sse:
	$(MAKE) CFLAGS='$(CFLAGS) -fprofile-use -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-use -lgcov' sse

profgen_sse41:
	$(MAKE) CFLAGS='$(CFLAGS) -fprofile-generate -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-generate -lgcov' sse41

profuse_sse41:
	$(MAKE) CFLAGS='$(CFLAGS) -fprofile-use -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-use -lgcov' sse41

profgen_sse2:
	$(MAKE) CFLAGS='$(CFLAGS) -fprofile-generate -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-generate -lgcov' sse2

profuse_sse2:
	$(MAKE) CFLAGS='$(CFLAGS) -fprofile-use -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-use -lgcov' sse2

pgo:
	$(MAKE) profgen
	@./$(TARGET) bench
	@touch $(SOURCES)
	$(MAKE) profuse

publish:
	$(MAKE) profgen
	@./$(TARGET) bench
	@touch $(SOURCES)
	$(MAKE) profuse
	#@strip $(TARGET)
	@mv $(TARGET) $(TARGET_BMI2)
	$(MAKE) clean
	$(MAKE) profgen_sse
	@./$(TARGET) bench
	@touch $(SOURCES)
	$(MAKE) profuse_sse
	#@strip $(TARGET)
	@mv $(TARGET) $(TARGET_SSE42)
	$(MAKE) clean
	$(MAKE) profgen_sse41
	@./$(TARGET) bench
	@touch $(SOURCES)
	$(MAKE) profuse_sse41
	#@strip $(TARGET)
	@mv $(TARGET) $(TARGET_SSE41)
	$(MAKE) clean
	$(MAKE) profgen_sse2
	@./$(TARGET) bench
	@touch $(SOURCES)
	$(MAKE) profuse_sse2
	#@strip $(TARGET)
	@mv $(TARGET) $(TARGET_SSE2)

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET) ${OBJECTS:.o=.gcda}

-include $(DEPENDS)
