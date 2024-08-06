CXX       	?= g++
PREFIX	  	?= /usr
VARS  	  	?=

DEBUG 		?= 1
LOG_FRAME   ?= 0
# https://stackoverflow.com/a/1079861
# WAY easier way to build debug and release builds
ifeq ($(DEBUG), 1)
        BUILDDIR  = build/debug
        CXXFLAGS := -ggdb -Wall $(DEBUG_CXXFLAGS) $(CXXFLAGS)
else
        BUILDDIR  = build/release
        CXXFLAGS := -O2 $(CXXFLAGS)
endif

ifeq ($(LOG_FRAME), 1)
		VARS += -DLOG_FRAME=1
endif

NAME		 = BurntEngine
TARGET		 = main
SRC 	   	 = $(sort $(wildcard src/*.cpp))
OBJ 	   	 = $(SRC:.cpp=.o)
LDFLAGS   	+= -lassimp -lfmt -lSDL3 -lvulkan
CXXFLAGS  	?= -mtune=generic -march=native
CXXFLAGS        += -Wno-ignored-attributes -funroll-all-loops -fpermissive -Iinclude -std=c++17 $(VARS)

all: $(TARGET)

$(TARGET): $(OBJ)
	mkdir -p $(BUILDDIR)
	$(CXX) $(OBJ) -o $(BUILDDIR)/$(TARGET) $(LDFLAGS)

dist: $(TARGET)
	bsdtar -zcf $(NAME)-v$(VERSION).tar.gz LICENSE README.md -C $(BUILDDIR) $(TARGET)

clean:
	rm -rf $(BUILDDIR)/$(TARGET) $(OBJ)

distclean:
	rm -rf $(BUILDDIR) $(OBJ)

.PHONY: $(TARGET) clean all
