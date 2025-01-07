# Variables taken from {fmt} cmake build files
# I don't wanna use cmake
# cmd: cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
# CXX_DEFINES   = -DFMT_LIB_EXPORT -Dfmt_EXPORTS
        # $(CXX) -fPIC -O3 -DNDEBUG -shared -Wl,-soname,libfmt.so.10 -o libfmt.so format.cc.o os.cc.o

CXX		?= g++
CXX_INCLUDES 	= -I../../include
CXX_FLAGS 	= -O3 -DNDEBUG -fvisibility=hidden -fvisibility-inlines-hidden

all: fmt

fmt: format.cc os.cc
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -fPIC -c format.cc -o format.cc.o
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -fPIC -c os.cc -o os.cc.o
	ar qc libfmt.a os.cc.o format.cc.o
	ranlib libfmt.a
	mv -f libfmt.a ../../$(BUILDDIR)/libfmt.a

clean:
	rm -f *.o *.so *.a ../../$(BUILDDIR)/fmt/.*a

.PHONY: clean all fmt
