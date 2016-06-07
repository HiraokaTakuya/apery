#
# Makefile
#
#

CXX=g++

TARGET_BASE=shuffle_teacher
ifeq ($(OS),Windows_NT)
	TARGET=${TARGET_BASE}.exe
else
	TARGET=${TARGET_BASE}
endif

CPPSRCS=main.cpp
CPPOBJECTS=${CPPSRCS:.cpp=.o}
LDFLAGS=
OPT=-Wall -std=c++11
#OPT+= -Winline

assert:
	$(MAKE) CPPFLAGS='$(OPT) -O3' All

release:
	$(MAKE) CPPFLAGS='$(OPT) -O3 -DNDEBUG' All

All: ${CPPOBJECTS}
	$(CXX) $(CPPOBJECTS) $(CPPFLAGS) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f ${CPPOBJECTS} ${TARGET} ${CPPSRCS:.cpp=.gcda}

depend:
	@$(CXX) -MM $(OPT) $(CPPSRCS) > .depend

-include .depend
