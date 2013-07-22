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

namespace po = boost::program_options;

long get_diff_in_microsecs(struct timeval *now, struct timeval *then) {
    uint64_t t = (now->tv_sec - then->tv_sec);
    t = t * 1000000 + (now->tv_usec - then->tv_usec);
    return t;
}

int main(int argc, const char * argv[])
{
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
			std::map<std::string, int> state_map;
			int port = 5556;
			if (argc > 2 && strcmp(argv[1],"-p") == 0) {
				port = strtol(argv[2], 0, 0);
			}
			std::cout << "Listening on port " << port << "\n";
	        // client
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
	            if (op == "STATE" || op == "VALUE") {					
	                iss >> state;
					std::string key = point + "." + state;
					int state_num = 0;
					std::map<std::string, int>::iterator idx = state_map.find(key);
					if (idx == state_map.end()) { // new state
						std::map<std::string, int>::iterator idx = state_map.find(point);
						if (idx == state_map.end()) { //new point
							state_map[point] = 0;
							state_map[key] = 0;
						}
						else {
							(*idx).second++; // calc next state number
							state_num = (*idx).second;
						}
					}
					else
						state_num = (*idx).second;
					state_map[key] = state_num;
					
					std::cout << get_diff_in_microsecs(&now, &start) << "\t" << point << "\t" << state << "\t" << state_num<< "\n" << std::flush;
	            	//std::cout << point << " "<< op;
	                //std::cout << " " << state << "\n";
	            }
	            else std::cout << data << "\n";
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

