/** 
This program reads input of the form
  time machine_name state_name state_value
and converts it to output of the form:
  time machine1_state_value machine2_state_value... etc
*/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <string.h>

long last_t=0, t;

bool emit_state_names = false;
bool emit_state_ids = true;
bool square_wave = false;
bool help = false;
bool graph = false;
bool only_show_changes = true;
long min_y = 1000000;
long max_y = -1000000;

struct TimeSeriesGraph {
	long min_value;
	long max_value;
	int screen_width;
	char *row;
	char *last_row;
	std::map<std::string, long> series;
	TimeSeriesGraph() : min_value(min_y), max_value(max_y), screen_width(140) {
		row = new char[screen_width+1];
		memset(row, ' ', screen_width);
		row[screen_width] = 0;
		last_row = new char[screen_width+1];
		memset(last_row, ' ', screen_width);
		last_row[screen_width] = 0;
	}
	void emit();
	bool plot(long v, char symbol);
	bool rescaleGraph(long val);
};


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

bool TimeSeriesGraph::rescaleGraph(long val) {
	bool rescaled = false;
	if (val > max_value) { max_value = ((val>0) ? 1.1f : 0.9f) * val; rescaled = true; }
	if (val < min_value) { min_value = ((val>0) ? 0.9f : 1.1f) * val; rescaled = true; }
	memset(last_row, ' ', screen_width);
	return rescaled;
}

bool TimeSeriesGraph::plot(long v, char symbol) {
	bool changed = false;
	int col =  (int) (0.95f * screen_width * (v - min_value) / (max_value - min_value) );
	if (col >= 0 && col <screen_width) {
		if (last_row[col] == ' ') changed = true;
		row[ col ] = symbol;
	}
	else std::cout <<" col: " << col << "\n";
	return changed;
}
void TimeSeriesGraph::emit() {
	const char *symbols = "*@#%ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int max_sym = strlen(symbols)-1;
	if (max_value <= min_value) { std::cout << "\n"; return; }
	
	std::map<std::string, long>::const_iterator iter = series.begin();
	int symbol_idx = 0;
	bool changed = false;
	while (iter != series.end()) {
		const std::pair<std::string, long> &pt = *iter++;
		changed = plot(pt.second, symbols[symbol_idx]);
		if (symbol_idx++ > max_sym) symbol_idx = max_sym;
	}
	if (!only_show_changes || (only_show_changes && changed) ) {
		std::cout << std::setw(8) << last_t << " " << row << "\n";
		memcpy(last_row, row, screen_width);
		memset(row, ' ', screen_width);
		if (min_value <= 0 && max_value >= 0) plot(0, '|');
		for (long x=-30000; x<=30000; x+=10000) {
			if (min_value <= x && max_value >= x) plot(x, (x==0)?'|':'!');
		}
	}
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
		<< "  -h   show this help text\n"
		<< "  -g   graphical output (adds -s and -i)"
		<< "  -m val  minimum of the graph range"
		<< "  -x val  maximum of the graph range"
		;
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
		else if (strcmp(argv[i],"-g") == 0) { graph = true; emit_state_ids = false; emit_state_names = false; }
		else if (strcmp(argv[i],"-m") == 0 && i+1<argc) {
			char *q;
			long x = strtol(argv[++i], &q, 10);
			if (*q == 0) min_y = x;
		}
		else if (strcmp(argv[i],"-x") == 0 && i+1<argc) {
			char *q;
			long x = strtol(argv[++i], &q, 10);
			if (*q == 0) max_y = x;
		}
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
	
	TimeSeriesGraph g;

	if (!graph) labels();

	std::string dev, state;
	int value;
	std::cin >> t >> dev >> state >> value;
	while (!std::cin.eof()) {
		t = t/1000; // we work in milliseconds
		
		if (!graph) {
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
		}
		else {
			g.series[dev] = value;
			if (g.rescaleGraph(value)) std::cout << "...\n";
			g.emit();
		}
		std::cin >> t >> dev >> state >> value;
		last_t = t;
	}
	emit();
	std::cout << "End of Scope\n";
}
