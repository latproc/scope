#!/bin/sh
LATPROCDIR=../latproc
#BOOST_EXTRA=-mt
CFLAGS="-g ${CFLAGS} -I/usr/local/include -I/opt/local/include -L/usr/local/lib -L/opt/local/lib"
cc -O -c -o cJSON.o $LATPROCDIR/iod/cJSON.c
c++ -O -o sampler $CFLAGS -I$LATPROCDIR/iod sampler.cpp -lzmq -lboost_system${BOOST_EXTRA} -lboost_program_options${BOOST_EXTRA} \
	$LATPROCDIR/iod/MessagingInterface.cpp $LATPROCDIR/iod/Logger.cpp $LATPROCDIR/iod/cJSON.c \
	$LATPROCDIR/iod/DebugExtra.cpp $LATPROCDIR/iod/value.cpp $LATPROCDIR/iod/symboltable.cpp \
	$LATPROCDIR/iod/options.cpp $LATPROCDIR/iod/message.cpp $LATPROCDIR/iod/MessageLog.cpp \
	$LATPROCDIR/iod/MessageEncoding.cpp $LATPROCDIR/iod/anet.c
c++ -O -o scope $CFLAGS scope.cpp
c++ -O -o filter $CFLAGS -I$LATPROCDIR/iod filter.cpp $LATPROCDIR/iod/regular_expressions.cpp \
	$LATPROCDIR/iod/message.cpp $LATPROCDIR/iod/symboltable.cpp $LATPROCDIR/iod/value.cpp -lboost_system \
	$LATPROCDIR/iod/Logger.cpp $LATPROCDIR/iod/DebugExtra.cpp
