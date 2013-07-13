#!/bin/sh
LATPROCDIR=../latproc
CFLAGS=-I$LATPROCDIR/include
c++ -o sampler $CFLAGS -I$LATPROCDIR/iod sampler.cpp -lzmq
c++ -o scope $CFLAGS scope.cpp
c++ -o filter $CFLAGS -I$LATPROCDIR/iod filter.cpp $LATPROCDIR/iod/regular_expressions.cpp
