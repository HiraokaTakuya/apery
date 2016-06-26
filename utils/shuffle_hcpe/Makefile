#
# Makefile
#
#

CXX=g++

LDFLAGS=
TARGET_BASE=shuffle_hcpe
ifeq ($(OS),Windows_NT)
	TARGET=${TARGET_BASE}.exe
	LDFLAGS += -static
else
	TARGET=${TARGET_BASE}
endif

CPPSRCS=main.cpp
CPPOBJECTS=${CPPSRCS:.cpp=.o}

OPT=-Wall -std=c++11
#OPT+= -Winline

release:
	$(MAKE) CPPFLAGS='$(OPT) -O3 -DNDEBUG' LDFLAGS='$(LDFLAGS)' All

assert:
	$(MAKE) CPPFLAGS='$(OPT) -O3' All

All: ${CPPOBJECTS}
	$(CXX) $(CPPOBJECTS) $(CPPFLAGS) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f ${CPPOBJECTS} ${TARGET} ${CPPSRCS:.cpp=.gcda}

depend:
	@$(CXX) -MM $(OPT) $(CPPSRCS) > .depend

-include .depend
