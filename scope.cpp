/** 
This program reads input of the form
  time machine_name state_name state_value
and converts it to output of the form:
  time machine1_state_value machine2_state_value... etc
*/

#include <iostream>
#include <fstream>
#include <map>

long last_t=0, t;
std::map<std::string, int> device_map;

void emit() {
	std::cout << last_t;
	std::map<std::string, int>::iterator iter = device_map.begin();
	while (iter != device_map.end()) {
		std::cout << "\t" << (*iter).second;
		iter++;
	}
	std::cout << "\n" << std::flush;
}

void labels() {
	std::cout << "\"Time\"";
	std::map<std::string, int>::iterator iter = device_map.begin();
	while (iter != device_map.end()) {
		std::cout << "\t\"" << (*iter).first << "\"";
		iter++;
	}
	std::cout << "\n" << std::flush;
}


int main(int argc, char *argv[])
{
	std::ifstream device_file("scope.txt");
	while (!device_file.eof()) {
		std::string name;
		int id;
		device_file >> name >> id;
		if (device_file.good())	device_map[name] = 0;
	}
	
	labels();	

	std::string dev, state;
	int value;
	std::cin >> t >> dev >> state >> value;
	while (std::cin.good()) {
		t = t/1000; // we work in milliseconds
		
		if (t != last_t) {
			emit();
			if (t>last_t+1) { last_t = t-1; emit(); }
			last_t = t;
		}
		if (device_map.find(dev) != device_map.end()) {
			device_map[dev] = value;
		}
		std::cin >> t >> dev >> state >> value;
	}
	last_t = t;
	emit();
}