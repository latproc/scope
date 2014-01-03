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

/*
 This sampler collects STATE and VALUE changes from a zmq channel and displays
the received data on stdout with the addition of a timestamp indicating when
the message was received.

Devices are mapped to a unique integer that is displayed or republished 
instead of the device name. A second channel is used to transmit new machine names
as they are discovered. A command channel can be used to request that all 
device names are resent.
*/
#include <iostream>
#include <iterator>
#include <string>
#include <list>
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
#include "cJSON.h"
#include <MessagingInterface.h>
#include <value.h>

using namespace std;

namespace po = boost::program_options;

uint64_t get_diff_in_microsecs(struct timeval *now, struct timeval *then) {
    uint64_t t = (now->tv_sec - then->tv_sec);
    t = t * 1000000 + (now->tv_usec - then->tv_usec);
    return t;
}

void interrupt_handler(int sig) {
	exit(0);
}

map<string, int> state_map;
map<string, int> device_map;

void save_devices() {
	ofstream device_file("devices.dat");
	map<string, int>::iterator iter = device_map.begin();
	while (iter != device_map.end()) {
		device_file << (*iter).first << "\t" << (*iter).second << "\n";
		iter++;
	}
}

void save_state_names() {
	ofstream states_file("states.dat");
	map<string, int>::iterator iter = state_map.begin();
	while (iter != state_map.end()) {
		states_file << (*iter).first << "\t" << (*iter).second << "\n";
		iter++;
	}
}

class SamplerOptions {
    int subscribe_to_port;
    string subscribe_to_host;
    int publish_to_port;
    string publish_to_interface;
    bool republish;
    bool quiet;
	bool raw;
	bool ignore_values;
	bool only_numeric_values;
  public:
    SamplerOptions() : subscribe_to_port(5556), subscribe_to_host("localhost"),
        publish_to_port(5560), publish_to_interface("*"), 
		republish(false), quiet(false), raw(false), ignore_values(false), only_numeric_values(false)
	 {}
	bool parseCommandLine(int argc, const char *argv[]);

	bool publish() { return republish; }
    int subscriberPort() const { return subscribe_to_port; }
    const std::string &subscriberHost() const { return subscribe_to_host; }
    int publisherPort() const { return publish_to_port; }
    const std::string &publisherInterface() const { return publish_to_interface; }
	bool quietMode() { return quiet; }
	bool rawMode() { return raw; }
	bool ignoreValues() { return ignore_values; }
	bool onlyNumericValues() { return only_numeric_values; }
};

bool SamplerOptions::parseCommandLine(int argc, const char *argv[]) {
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
        ("help", "produce help message")
        ("republish", "publish received messages")
        ("subscribe", po::value<string>(), "host to subscribe to [localhost]")
        ("subscribe-port", po::value<int>(), "port to subscribe to [5556]")
        ("interface",po::value<string>(),  "local interface to publish to [*]")
        ("publish-port", po::value<int>(), "port to subscribe to [5560]")
		("quiet", "quiet mode, no output on stdout")
		("raw", "process all received messages, not just state and value changes")
		("ignore-values", "ignore value changes, only process state changes")
		("only-numeric-values", "ignore value changes for non-numeric values")
        ;
        po::variables_map vm;        
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);    
        if (vm.count("help")) {
            cout << desc << "\n";
            return false;
        }
		if (vm.count("republish")) 
			republish = true;
		if (vm.count("subscribe")) 
			subscribe_to_host = vm["subscribe"].as<string>();
		if (vm.count("interface")) 
			publish_to_interface = vm["interface"].as<string>();
		if (vm.count("subscribe-port")) 
			subscribe_to_port = vm["subscribe-port"].as<int>();
		if (vm.count("publish-port")) 
			publish_to_port = vm["publish-port"].as<int>();
		if (vm.count("quiet")) quiet = true;
		if (vm.count("raw")) raw = true;
		if (vm.count("ignore-values")) ignore_values = true;
		if (vm.count("only-numeric-values")) only_numeric_values = true;
	}
    catch(exception& e) {
        cerr << "error: " << e.what() << "\n";
        return false;
    }
    catch(...) {
        cerr << "Exception of unknown type!\n";
		return false;
    }
	return true;
}

struct CommandThread {
    void operator()();
    CommandThread();
    void stop();
    bool done;
private:
};

struct Command {
public:
    Command()  : done(false), error_str(""), result_str("") {}
    virtual ~Command(){ }
    bool done;
    const char *error() { return error_str.c_str(); }
    const char *result() { return result_str.c_str(); }
    
	bool operator()(std::vector<Value> &params) {
		done = run(params);
		return done;
	}
    
protected:
	virtual bool run(std::vector<Value> &params) = 0;
	std::string error_str;
	std::string result_str;
};

struct CommandRefresh : public Command {
	bool run(std::vector<Value> &params);
};

bool CommandRefresh::run(std::vector<Value> &params) {
	return false;
}

void sendMessage(zmq::socket_t &socket, const char *message) {
    const char *msg = (message) ? message : "";
    size_t len = strlen(msg);
    zmq::message_t reply (len);
    memcpy ((void *) reply.data (), msg, len);
    socket.send (reply);
}

void CommandThread::operator()() {
    std::cout << "------------------ Command Thread Started -----------------\n";
    zmq::context_t context (3);
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind ("tcp://*:5555");
    
    while (!done) {
        try {
            
             zmq::pollitem_t items[] = { { socket, 0, ZMQ_POLLERR | ZMQ_POLLIN, 0 } };
             zmq::poll( &items[0], 1, 500*1000);
             if ( !(items[0].revents & ZMQ_POLLIN) ) continue;

        	zmq::message_t request;
            if (!socket.recv (&request)) continue; // interrupted system call
            size_t size = request.size();
            char *data = (char *)malloc(size+1);
            memcpy(data, request.data(), size);
            data[size] = 0;
            std::cout << "Command thread received " << data << std::endl;
            std::istringstream iss(data);

            std::list<Value> *parts = 0;

			// read the command, attempt to read a structured form first, 
			// and try a simple form if that fails
			std::string cmd;
			if (!MessagingInterface::getCommand(data, cmd, &parts)) {
				parts = new std::list<Value>;
	            int count = 0;
				std::string s;
	            while (iss >> s) {
	                parts->push_back(Value(s.c_str()));
	                ++count;
	            }
			}
			
            std::vector<Value> params(0);
			params.push_back(cmd.c_str());
            std::copy(parts->begin(), parts->end(), std::back_inserter(params));
			delete parts;
            if (params.empty()) {
                sendMessage(socket, "Empty message received\n");
                goto cleanup;
            }
			{
				Command *command = 0;
				std::string ds(params[0].asString());
				if (ds == "") {
					command = new CommandRefresh();
				}
				else {
					command = new CommandRefresh();
				}
	            if ((*command)(params)) {
	                //std::cout << command->result() << "\n";
	                sendMessage(socket, command->result());
	            }
	            else {
	                NB_MSG << command->error() << "\n";
	                sendMessage(socket, command->error());
	            }
	            delete command;
	        }
        cleanup:
            free(data);
        }
        catch (std::exception e) {
			if (errno) std::cout << "error during client communication: " << strerror(errno) << "\n" << std::flush;
            if (zmq_errno())
                std::cerr << zmq_strerror(zmq_errno()) << "\n" << std::flush;
            else
                std::cerr << " Exception: " << e.what() << "\n" << std::flush;
			if (zmq_errno() != EINTR && zmq_errno() != EAGAIN) abort();
        }
    }
    socket.close();
}

int outputNumeric(std::ostream &out, std::istream &in, long &val) {
	// convert the input to an integer value and output the name and value if 
	// the conversion is successful
	string s_value;
    in >> s_value;
	char *remainder;
	val = strtol(s_value.c_str(), &remainder, 0);
	return (*remainder == 0);
}

std::string outputRemaining(std::ostream &out, std::istream &in) {
	// copy the remainder of the stream as raw data
	char buf[25];
	char *rbuf;
	int pos = in.tellg();
	in.seekg(0,in.end);
	int endpos = in.tellg();
	int size = endpos - pos;
	in.seekg(pos,in.beg);
	if (size>=25)
		rbuf = new char [size+1];
	else
		rbuf = buf;
	std::streambuf *pbuf = in.rdbuf();
	pbuf->sgetn (rbuf,size);
	rbuf[size] = 0;

	std::string res(rbuf);
	if (size>=25) delete rbuf;
	return res;
}

static int next_device_num = 0;
static int next_state_num = 0;

int lookupState(std::string &state) {
	int state_num = 0;
	map<string, int>::iterator idx = state_map.find(state);
	if (idx == state_map.end()) {// new state
		state_num = next_state_num;
		state_map[state] = next_state_num++;
		save_state_names();
	}
	else
		state_num = (*idx).second;
	return state_num;
}

int main(int argc, const char * argv[])
{
	SamplerOptions options;
	if (!options.parseCommandLine(argc, argv)) return 1;
	
	if (options.quietMode() && !options.publish())
		cerr << "Warning: not writing to stdout or zmq\n";

	MessagingInterface *mif = 0;
    if (options.publish())
	 	mif = MessagingInterface::create("*", options.publisherPort());
	
	atexit(save_devices);
	atexit(save_state_names);
	signal(SIGINT, interrupt_handler);
	signal(SIGTERM, interrupt_handler);
        // this should be a separate thread
    try {
        // client
        int res;
		stringstream ss;
		ss << "tcp://" << options.subscriberHost() << ":" << options.subscriberPort();
        zmq::context_t context (1);
        zmq::socket_t subscriber (context, ZMQ_SUB);
        res = zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
        assert (res == 0);
        subscriber.connect(ss.str().c_str());
		struct timeval start;
		gettimeofday(&start, 0);
		stringstream output;
        for (;;) {
            zmq::message_t update;
            subscriber.recv(&update);

			struct timeval now;
			gettimeofday(&now, 0);

			long len = update.size();
            char *data = (char *)malloc(len+1);
            memcpy(data, update.data(), len);
            data[len] = 0;
			output.str("");	
			output.clear();
			if (options.rawMode()) {
				output << data;
			}
            else {
				std::list<Value> *message = 0;
				string machine, property, op, state;
				Value val(SymbolTable::Null);
				if (MessagingInterface::getCommand(data, op, &message)) {
					if (op == "STATE" && message->size() == 2) {
						machine = message->front().asString();
						message->pop_front();
						state = message->front().asString();
						int state_num = lookupState(state);
						map<string, int>::iterator idx = device_map.find(machine);
						if (idx == device_map.end()) //new machine
							device_map[machine] = next_device_num++;
						output << get_diff_in_microsecs(&now, &start) << "\t" 
							<< machine << "\t" << state << "\t" << state_num;
					}
					else if (op == "PROPERTY" && message->size() == 3) {
						std::string machine = message->front().asString();
						message->pop_front();
						std::string prop = message->front().asString();
						message->pop_front();
						property = machine + "." + prop;
						val = message->front();

						map<string, int>::iterator idx = device_map.find(property);
						if (idx == device_map.end()) //new machine
							device_map[property] = next_device_num++;
						
						output << get_diff_in_microsecs(&now, &start) << "\t" 
							<< property << "\tvalue\t";
						if (val.kind == Value::t_string)
						 	output << "\"" << val.asString() << "\"";
						else
							output << val.asString();
					}
				}
				else {
		            istringstream iss(data);
					std::string machine;
		            iss >> machine >> op;
					if (op == "STATE") {					
		                iss >> state;
						int state_num = lookupState(state);
						map<string, int>::iterator idx = device_map.find(machine);
						if (idx == device_map.end()) //new machine
							device_map[machine] = next_device_num++;
				
						output << get_diff_in_microsecs(&now, &start) << "\t" 
							<< machine << "\t" << state << "\t" << state_num;
		            }
					else if (op == "VALUE" && !options.ignoreValues()) {
						property = machine;
						map<string, int>::iterator idx = device_map.find(property);
						if (idx == device_map.end()) //new machine
							device_map[property] = next_device_num++;

	
						if (options.onlyNumericValues()) {
							long val;
							if (outputNumeric(output, iss, val)) 
								output << get_diff_in_microsecs(&now, &start) << "\t" 
								<< property << "\tvalue\t" << val;
						}
						else {
							std::string val(outputRemaining(output, iss));
							output << get_diff_in_microsecs(&now, &start) << "\t" 
								<< property << "\tvalue\t" << val;
						}
					}
				}
			}
			if (!output.str().empty()) {
				if (!options.quietMode()) 
					cout << output.str() << "\n" << flush;
				if (options.publish()) {
					mif->send(output.str().c_str());
				}
			}
			delete data;
        }
    }
    catch(exception& e) {
        cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch(...) {
        cerr << "Exception of unknown type!\n";
    }

    return 0;
}

