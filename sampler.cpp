/*
  Copyright (C) 2012 Martin Leadbeater, Michael O'Connor

  This file is part of Latproc

  Latproc is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  Latproc is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Latproc; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <iostream>
#include <iterator>
#include <stdio.h>
#include <boost/program_options.hpp>
#include <zmq.hpp>
#include <sstream>
#include <string.h>
#include "Logger.h"
#include <inttypes.h>
#include <stdlib.h>
#include <fstream>
#include <signal.h>

namespace po = boost::program_options;

long get_diff_in_microsecs(struct timeval *now, struct timeval *then) {
    uint64_t t = (now->tv_sec - then->tv_sec);
    t = t * 1000000 + (now->tv_usec - then->tv_usec);
    return t;
}

void interrupt_handler(int sig) {
	exit(0);
}

std::map<std::string, int> state_map;
std::map<std::string, int> device_map;

void save_devices() {
	std::ofstream device_file("devices.txt");
	std::map<std::string, int>::iterator iter = device_map.begin();
	while (iter != device_map.end()) {
		device_file << (*iter).first << "\t" << (*iter).second << "\n";
		iter++;
	}
}
void save_state_names() {
	std::ofstream states_file("devices.txt");
	std::map<std::string, int>::iterator iter = state_map.begin();
	while (iter != state_map.end()) {
		states_file << (*iter).first << "\t" << (*iter).second << "\n";
		iter++;
	}
}

int main(int argc, const char * argv[])
{
	atexit(save_devices);
	atexit(save_state_names);
	signal(SIGINT, interrupt_handler);
	signal(SIGTERM, interrupt_handler);
    try {
        
#if 0
        po::options_description desc("Allowed options");
        desc.add_options()
        ("help", "produce help message")
        ("server", "run as server")
        ;
        po::variables_map vm;        
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);    
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }
        if (vm.count("server")) {
#endif
		if (argc > 1 && strcmp(argv[1],"--server") == 0) {
	        //server
	        int count = 0;
	        zmq::context_t context (1);
	        zmq::socket_t publisher (context, ZMQ_PUB);
	        publisher.bind("tcp://*:5556");
	        for (;;) {
	            zmq::message_t message(20);
	            snprintf ((char *) message.data(), 20, "%d", ++count);
	            publisher.send(message);
	            sleep(1);
	        }
	    }
	    else {
	        // client
			int next_device_num = 0;
			int next_state_num = 0;
			int port = 5556;
			if (argc > 2 && strcmp(argv[1],"-p") == 0) {
				port = strtol(argv[2], 0, 0);
			}
	        int res;
			std::stringstream ss;
			ss << "tcp://localhost:" << port;
	        zmq::context_t context (1);
	        zmq::socket_t subscriber (context, ZMQ_SUB);
	        res = zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
	        assert (res == 0);
	        subscriber.connect(ss.str().c_str());
			struct timeval start;
			gettimeofday(&start, 0);
	        //subscriber.connect("ipc://ecat.ipc");
	        for (;;) {
	            zmq::message_t update;
	            subscriber.recv(&update);
				struct timeval now;
				gettimeofday(&now, 0);
				long len = update.size();
                char *data = (char *)malloc(len+1);
                memcpy(data, update.data(), len);
                data[len] = 0;
	            std::istringstream iss(data);
	            //iss >> count;
	            std::string point, op, state;
	            iss >> point >> op;
	            if (op == "STATE") {					
	                iss >> state;
					int state_num = 0;
					std::map<std::string, int>::iterator idx = state_map.find(state);
					if (idx == state_map.end()) {// new state
						state_num = next_state_num;
						state_map[state] = next_state_num++;
					}
					else
						state_num = (*idx).second;
					idx = device_map.find(point);
					if (idx == device_map.end()) //new machine
						device_map[point] = next_device_num++;
					
					std::cout << get_diff_in_microsecs(&now, &start) << "\t" << point << "\t" << state << "\t" << state_num<< "\n" << std::flush;
	            	//std::cout << point << " "<< op;
	                //std::cout << " " << state << "\n";
	            }
				else if (op == "VALUE") {
					std::map<std::string, int>::iterator idx = device_map.find(point);
					if (idx == device_map.end()) //new machine
						device_map[point] = next_device_num++;

					std::string s_value;
	                iss >> s_value;
					long val;
					char *remainder;
					val = strtol(s_value.c_str(), &remainder, 0);
					if (*remainder == 0)
						std::cout << get_diff_in_microsecs(&now, &start) << "\t" << point << "\tvalue\t" << val << "\n" << std::flush;
				}
	            //else std::cout << data << "\n";
				delete data;
	        }
	    }
    }
    catch(std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch(...) {
        std::cerr << "Exception of unknown type!\n";
    }

    return 0;
}

