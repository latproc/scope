#!/bin/sh
LATPROCDIR=../latproc
CFLAGS="-I$LATPROCDIR/include -g -I/opt/local/include/ -L/opt/local/lib/"
CFLAGS="${CFLAGS} -I/usr/local/include -L/usr/local/lib"
cc -c -o cJSON.o $LATPROCDIR/iod/cJSON.c
c++ -o sampler $CFLAGS -I$LATPROCDIR/iod sampler.cpp -lzmq -lboost_system -lboost_program_options \
	$LATPROCDIR/iod/MessagingInterface.cpp $LATPROCDIR/iod/Logger.cpp $LATPROCDIR/iod/cJSON.c \
	$LATPROCDIR/iod/DebugExtra.cpp $LATPROCDIR/iod/value.cpp $LATPROCDIR/iod/symboltable.cpp
c++ -o scope $CFLAGS scope.cpp
c++ -o filter $CFLAGS -I$LATPROCDIR/iod filter.cpp $LATPROCDIR/iod/regular_expressions.cpp 
