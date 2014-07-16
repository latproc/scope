# setup and run cmake

all:	build build/CMakeFiles

build:
	mkdir build

build/CMakeFiles:
	(cd build; cmake ..; make )
