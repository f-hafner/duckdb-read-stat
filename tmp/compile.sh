#!/bin/bash

#https://www.reddit.com/r/learnprogramming/comments/11gxbuu/c_gcc_compiler_claiming_theres_an_undefined/
#https://stackoverflow.com/questions/3209721/gcc-gives-error-while-using-fmod
gcc -o test_bin  \
	../third_party/readstat/src/readstat_metadata.c \
	../third_party/readstat/src/readstat_variable.c \
	-I../third_party/readstat/src \
	-L../build/release/ \
	-lread_stat \
    -lm \
	main.cc

# Using the binary
#LD_LIBRARY_PATH=../build/release ./test_bin
