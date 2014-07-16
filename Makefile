# setup and run cmake

all:	sampler filter scope

build:
	mkdir build

build/CMakeFiles:	build
	(cd build; cmake ..; make )

build/Scope:	build/CMakeFiles
build/Sampler:	build/CMakeFiles
build/Filter:	build/CMakeFiles

scope:	build/Scope
	cp build/Scope scope

filter:	build/Filter
	cp build/Filter filter

sampler:	build/Sampler
	cp build/Sampler sampler

clean:
	rm -f sampler filter scope
	rm -rf build
