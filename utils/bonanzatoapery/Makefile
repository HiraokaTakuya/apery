COMPILER = g++
CFLAGS   = -std=c++11 -fno-exceptions -fno-rtti -Wextra -Ofast -MMD -MP
LDFLAGS  = #-lpthread
LIBS     =
INCLUDE  = #-I../include
ifeq ($(OS),Windows_NT)
  TARGET = bonanzatoapery.exe
  LDFLAGS += -static
else
  TARGET = bonanzatoapery
endif
OBJDIR   = .
ifeq "$(strip $(OBJDIR))" ""
  OBJDIR = ..
endif
#SOURCES  = $(wildcard *.cpp)
SOURCES  = main.cpp
OBJECTS  = $(addprefix $(OBJDIR)/, $(SOURCES:.cpp=.o))
DEPENDS  = $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS) $(LIBS)
	$(COMPILER) -o $@ $^ $(LDFLAGS) $(CFLAGS)

$(OBJDIR)/%.o: %.cpp
	@[ -d $(OBJDIR) ] || mkdir -p $(OBJDIR)
	$(COMPILER) $(CFLAGS) $(INCLUDE) -o $@ -c $<

all: clean $(TARGET)

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET)

-include $(DEPENDS)
