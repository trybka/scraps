Compile and build the .o's and .a's...

`emcc -c foo.cc -o foo.o -O2`

`emcc -c main.cc -o main.o -O2`

`emar rcS libfoo.a foo.o`

`emar rcS libmain.a main.o`


Build and run -- exit code is 7

`emcc init.cc -O2 libfoo.a -Wl,--whole-archive libmain.a -Wl,--no-whole-archive`

Build and run -- exit code is 11. This is correct.

`emcc init.cc -O2 -Wl,--whole-archive libmain.a -Wl,--no-whole-archive libfoo.a`
