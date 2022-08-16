BUILD=build

CC=clang CXX=clang++ CC_LD=mold CXX_LD=mold meson $BUILD
CC=clang CXX=clang++ CC_LD=mold CXX_LD=mold meson compile -j16 -C $BUILD
