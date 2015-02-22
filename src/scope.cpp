/** 
This program reads input of the form
  time machine_name state_name state_value
and converts it to output of the form:
  time machine1_state_value machine2_state_value... etc
*/

#include <iostream>
#include <fstream>
#include <map>
#include <string.h>

long last_t=0, t;

bool emit_state_names = false;
bool emit_state_ids = true;
bool square_wave = false;
bool help = false;

struct StateInfo {
	std::string name;
	int id;
	StateInfo(const char *name_, int id_) : name(name_), id(id_) { }
};

typedef std::map<std::string, StateInfo> DeviceMap;
DeviceMap device_map;

void emit() {
	std::cout << last_t;
	std::map<std::string, StateInfo>::iterator iter = device_map.begin();
	while (iter != device_map.end()) {
		if (emit_state_names) std::cout << "\t" << (*iter).second.name;
		if (emit_state_ids) std::cout << "\t" << (*iter).second.id;
		iter++;
	}
	std::cout << "\n" << std::flush;
}

void labels() {
	std::cout << "\"Time\"";
	std::map<std::string, StateInfo>::iterator iter = device_map.begin();
	while (iter != device_map.end()) {
		if (emit_state_names) std::cout << "\t\"" << (*iter).first << ".state\"";
		if (emit_state_ids) std::cout << "\t\"" << (*iter).first << "\"";
		iter++;
	}
	std::cout << "\n" << std::flush;
}


void usage(const char *prog) {
	std::cerr << "Usage: \n"
		<< prog << " [options]\n"
		<< " where options are any of: "
		<< "  -S   emit state names\n"
		<< "  -s   do not emit state names\n"
		<< "  -I   emit state ids\n"
		<< "  -i   do not emit state ids\n"
		<< "  -R   synthesize a square wave display\n"
		<< "  -r   do not synthesize a square wave\n"
		<< "  -h   show this help text\n";
}

int main(int argc, char *argv[])
{
	for (int i=0; i<argc; ++i) {
		if (strcmp(argv[i],"-S") == 0) emit_state_names = true;
		else if (strcmp(argv[i],"-s") == 0) emit_state_names = false;
		else if (strcmp(argv[i],"-I") == 0) emit_state_ids = true;
		else if (strcmp(argv[i],"-i") == 0) emit_state_ids = false;
		else if (strcmp(argv[i],"-R") == 0) square_wave = true;
		else if (strcmp(argv[i],"-r") == 0) square_wave = false;
		else if (strcmp(argv[i],"-h") == 0) help = true;
	}
	if (help) { usage(argv[0]); return 0;}
	if (!emit_state_names && !emit_state_ids) emit_state_ids = true;

	/* prime the device list from a file. only devices listed there will 
		be reported
	*/
	std::ifstream device_file("scope.dat");
	if (!device_file.good())
		device_file.open("devices.dat");
	while (!device_file.eof()) {
		std::string name;
		int id;
		device_file >> name >> id;

		if (device_file.good())	device_map.insert(make_pair(name, StateInfo("",0)));
	}
	
	labels();	

	std::string dev, state;
	int value;
	std::cin >> t >> dev >> state >> value;
	while (!std::cin.eof()) {
		t = t/1000; // we work in milliseconds
		
		if (t != last_t) {
			emit();
			if (square_wave && t>last_t+1) { last_t = t-1; emit(); }
			last_t = t;
		}
		
		DeviceMap::iterator di = device_map.find(dev);
		if (di != device_map.end()) {
			(*di).second.name = state;
			(*di).second.id = value;
		}
		std::cin >> t >> dev >> state >> value;
	}
	last_t = t;
	emit();
	std::cout << "End of Scope\n";
}
