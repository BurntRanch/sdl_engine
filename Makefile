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
SRC 	   	 = $(sort $(wildcard src/*.cpp) $(wildcard src/renderer/*.cpp) $(wildcard src/networking/*.cpp) $(wildcard src/Nodes/*.cpp))
OBJ 	   	 = $(SRC:.cpp=.o)
LDFLAGS   	+= -L$(BUILDDIR)/fmt -L$(BUILDDIR)/steam -lGameNetworkingSockets -lassimp -lBulletSoftBody -lBulletDynamics -lBulletCollision -lLinearMath -l:libfmt.a -lSDL3 -lvulkan -lfreetype -shared -fno-PIE -Wl,-soname,$(TARGET)
CXXFLAGS  	?= -mtune=generic -march=native
CXXFLAGS        += -funroll-all-loops -Iinclude -Wall -Iinclude/steam -I/usr/include/bullet -isystem/usr/include/freetype2 -std=c++17 -fPIC $(VARS)

all: fmt toml gamenetworkingsockets $(TARGET)

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

gamenetworkingsockets:
ifeq ($(wildcard $(BUILDDIR)/steam/libGameNetworkingSockets.so),)
	mkdir -p $(BUILDDIR)/steam
	mkdir -p steam/GameNetworkingSockets/build
	cd steam/GameNetworkingSockets/build && cmake .. && make -j11
	cp steam/GameNetworkingSockets/build/bin/libGameNetworkingSockets.so $(BUILDDIR)/steam/
endif

$(TARGET): fmt toml gamenetworkingsockets $(OBJ)
	mkdir -p $(BUILDDIR)
	$(CXX) $(OBJ) $(BUILDDIR)/toml++/toml.o -o $(BUILDDIR)/$(TARGET) $(LDFLAGS)

dist: $(TARGET)
	bsdtar -zcf $(NAME)-v$(VERSION).tar.gz LICENSE README.md -C $(BUILDDIR) $(TARGET)

clean:
	rm -rf $(BUILDDIR)/$(TARGET) $(OBJ)

distclean:
	rm -rf $(BUILDDIR) $(OBJ)

.PHONY: $(TARGET) fmt toml clean all
