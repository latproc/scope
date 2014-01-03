#!/bin/sh
LATPROCDIR=../latproc
BOOST_EXTRA=-mt
CFLAGS="-I$LATPROCDIR/include -g -I/opt/local/include/ -L/opt/local/lib/"
c++ -o sampler $CFLAGS -I$LATPROCDIR/iod sampler.cpp -lzmq -lboost_system${BOOST_EXTRA} -lboost_program_options${BOOST_EXTRA} \
	$LATPROCDIR/iod/MessagingInterface.cpp $LATPROCDIR/iod/Logger.cpp
c++ -o scope $CFLAGS scope.cpp
c++ -o filter $CFLAGS -I$LATPROCDIR/iod filter.cpp $LATPROCDIR/iod/regular_expressions.cpp 
