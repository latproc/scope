#!/bin/sh
LATPROCDIR=../latproc
CFLAGS="-I$LATPROCDIR/include -g -I/opt/local/include/ -L/opt/local/lib/"
c++ -o sampler $CFLAGS -I$LATPROCDIR/iod sampler.cpp -lzmq -lboost_system-mt
c++ -o scope $CFLAGS scope.cpp
c++ -o filter $CFLAGS -I$LATPROCDIR/iod filter.cpp $LATPROCDIR/iod/regular_expressions.cpp
