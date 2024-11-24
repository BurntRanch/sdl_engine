CXX       	?= g++
PREFIX	  	?= /usr
VARS  	  	?=

DEBUG 		?= 1
LOG_FRAME   	?= 0
# https://stackoverflow.com/a/1079861
# WAY easier way to build debug and release builds
ifeq ($(DEBUG), 1)
        BUILDDIR  = build/debug
        CXXFLAGS := -ggdb3 -Wpedantic -Wall -Wextra -Wno-unused-parameter $(DEBUG_CXXFLAGS) $(CXXFLAGS)
	VARS 	 += -DDEBUG=1
else
        BUILDDIR  = build/release
        CXXFLAGS := -O2 $(CXXFLAGS)
endif

ifeq ($(LOG_FRAME), 1)
	VARS += -DLOG_FRAME=1
endif

NAME		 = BurntEngine
TARGET		 = libengine.so.1
SRC 	   	 = $(sort $(wildcard src/*.cpp))
OBJ 	   	 = $(SRC:.cpp=.o)
LDFLAGS   	+= -L$(BUILDDIR)/fmt -lassimp -lfmt -lSDL3 -lvulkan -lfreetype -shared -fno-PIE -Wl,-soname,$(TARGET)
CXXFLAGS  	?= -mtune=generic -march=native
CXXFLAGS        += -funroll-all-loops -Iinclude -isystem/usr/include/freetype2 -fPIC -std=c++17 $(VARS)

all: fmt toml $(TARGET)

fmt:
ifeq ($(wildcard $(BUILDDIR)/fmt/libfmt.a),)
	mkdir -p $(BUILDDIR)/fmt
	make -C src/fmt BUILDDIR=$(BUILDDIR)/fmt
endif

toml:
ifeq ($(wildcard $(BUILDDIR)/toml++/toml.o),)
	mkdir -p $(BUILDDIR)/toml++
	make -C src/toml++ BUILDDIR=$(BUILDDIR)/toml++
endif

$(TARGET): fmt toml $(OBJ)
	mkdir -p $(BUILDDIR)
	$(CXX) $(OBJ) $(BUILDDIR)/toml++/toml.o -o $(BUILDDIR)/$(TARGET) $(LDFLAGS)

dist: $(TARGET)
	bsdtar -zcf $(NAME)-v$(VERSION).tar.gz LICENSE README.md -C $(BUILDDIR) $(TARGET)

clean:
	rm -rf $(BUILDDIR)/$(TARGET) $(OBJ)

distclean:
	rm -rf $(BUILDDIR) $(OBJ)

.PHONY: $(TARGET) fmt toml clean all
