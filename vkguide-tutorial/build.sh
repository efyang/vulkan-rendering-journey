BUILD=build

CC=clang CXX=clang++ CC_LD=mold CXX_LD=mold meson $BUILD -D buildtype=debugoptimized -D cpp_debugstl=true
CC=clang CXX=clang++ CC_LD=mold CXX_LD=mold meson compile -j16 -C $BUILD