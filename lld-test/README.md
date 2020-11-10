emcc -c foo.cc -o foo.o -O2

emcc -c main.cc -o main.o -O2

emar rcS libfoo.a foo.o

emar rcS libmain.a main.o

emcc init.cc -O2 libfoo.a -Wl,--whole-archive libmain.a -Wl,--no-whole-archive
emcc init.cc -O2 -Wl,--whole-archive libmain.a -Wl,--no-whole-archive libfoo.a
