# setup and run cmake

all:	build sampler filter scope

build:
	mkdir build
	(cd build; cmake ..; make )

build/Scope:	src/scope.cpp

build/Filter:	src/filter.cpp

build/Sampler: src/sampler.cpp

scope:	build/Scope src/scope.cpp
	(cd build; cmake ..; make )
	cp build/Scope scope

filter:	build/Filter src/filter.cpp
	(cd build; cmake ..; make )
	cp build/Filter filter

sampler:	build/Sampler src/sampler.cpp
	(cd build; cmake ..; make )
	cp build/Sampler sampler

clean:
	rm -f sampler filter scope
	rm -rf build
