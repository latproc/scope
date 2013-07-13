#!/bin/sh
c++ -o sampler -I../latproc/iod sampler.cpp -lzmq
c++ -o scope scope.cpp
c++ -o filter -I../latproc/iod filter.cpp ../latproc/iod/regular_expressions.cpp
