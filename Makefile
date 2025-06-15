# setup and run cmake

SRC_DIR=src

all:	build sampler filter scope build/convert_date

build:
	mkdir build
	(cd build; cmake ..; make )

xcode:
	[ -d "xcode" ] || mkdir xcode
	cd xcode && cmake -G Xcode .. && xcodebuild -parallelizeTargets -jobs 6

build/Scope:	src/scope.cpp

build/Filter:	src/filter.cpp

build/Sampler:	src/sampler.cpp

build/convert_date:	src/convert_date.cpp src/convert_date.h
	(cd build; cmake ..; make )

scope:	Makefile build/Scope src/scope.cpp
	(cd build; cmake ..; make )
	rm -f scope
	cp build/Scope scope

filter:	Makefile build/Filter src/filter.cpp
	(cd build; cmake ..; make )
	rm -f filter
	cp build/Filter filter

sampler:	Makefile build/Sampler src/sampler.cpp
	(cd build; cmake ..; make )
	rm -f sampler
	cp build/Sampler sampler

style:
	astyle --options=.astylerc $(SRC_DIR)/*.cpp,*.h

clean:
	rm -f sampler filter scope
	rm -rf build
