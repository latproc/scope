# setup and run cmake

all:	build sampler filter scope

build:
	mkdir build
	(cd build; cmake ..; make )

scope:	build/Scope
	(cd build; cmake ..; make )
	cp build/Scope scope

filter:	build/Filter
	(cd build; cmake ..; make )
	cp build/Filter filter

sampler:	build/Sampler
	(cd build; cmake ..; make )
	cp build/Sampler sampler

clean:
	rm -f sampler filter scope
	rm -rf build
