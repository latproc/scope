#include <iostream>
#include <regular_expressions.h>
#include <list>	
#include <string.h>

int main(int argc, char *argv[]) {
	bool continuous = false; // ignore problems reading stdin, loop continuously
	std::list<rexp_info*>patterns;
	for (int i=1; i<argc; ++i) {
		if (strcmp(argv[i],"-c") ==0 )
			continuous = true;
		int result = 1;
		rexp_info *info = create_pattern(argv[i]);
		if (info->compilation_result == 0)
			patterns.push_back(info);
		else {
			std::cerr << "failed to compile regexp: " << argv[i] << "\n";
			release_pattern(info);
		}
	}
	
	char buf[100];
	
	do {
		while (std::cin.getline(buf, 100, '\n') ) {
			std::list<rexp_info*>::iterator iter = patterns.begin();
			while (iter != patterns.end()) {
				rexp_info *info = *iter++;
				if (execute_pattern(info, buf) == 0) {
					std::cout << buf << "\n" << std::flush;
					break;
				}
			}
		}
	} while (continuous);
	return 0;
}
