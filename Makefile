# setup and run cmake

all:	build sampler filter scope

build:
	mkdir build
	(cd build; cmake ..; make )

scope:	
	(cd build; cmake ..; make )
	cp build/Scope scope

filter:	
	(cd build; cmake ..; make )
	cp build/Filter filter

sampler:
	(cd build; cmake ..; make )
	cp build/Sampler sampler

clean:
	rm -f sampler filter scope
	rm -rf build
