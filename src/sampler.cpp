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
#include <boost/thread.hpp>
#include <zmq.hpp>
#include <sstream>
#include <string.h>
#include "Logger.h"
#include <inttypes.h>
#include <stdlib.h>
#include <fstream>
#include <signal.h>
#include "cJSON.h"
#include <value.h>
#include <MessageEncoding.h>
#include <MessagingInterface.h>
#include <SocketMonitor.h>
#include <ConnectionManager.h>
#include <ScopeConfig.h>

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
std::string current_channel;

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
	bool use_millis;
	string channel_name;
    string channel_url;
  public:
    SamplerOptions() : subscribe_to_port(5556), subscribe_to_host("localhost"),
        publish_to_port(5560), publish_to_interface("*"), 
		republish(false), quiet(false), raw(false), ignore_values(false), only_numeric_values(false),
		use_millis(false), channel_name("SAMPLER_CHANNEL")
	 {}
	bool parseCommandLine(int argc, const char *argv[]);

	bool publish() { return republish; }
    int subscriberPort() const { return subscribe_to_port; }
	void setSubscriberPort(int port) { subscribe_to_port = port; }
    const std::string &subscriberHost() const { return subscribe_to_host; }
    int publisherPort() const { return publish_to_port; }
    const std::string &publisherInterface() const { return publish_to_interface; }
	bool quietMode() { return quiet; }
	bool rawMode() { return raw; }
	bool ignoreValues() { return ignore_values; }
	bool onlyNumericValues() { return only_numeric_values; }
	bool reportMillis() { return use_millis; }
	string &channel() { return channel_name; }
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
		("millisec", "report time in milliseconds")
		("channel", po::value<string>(), "name of channel to use")
        ;
        po::variables_map vm;        
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);    
        if (vm.count("help")) {
            cerr << desc << "\n";
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
		if (vm.count("millisec")) use_millis = true;
		if (vm.count("channel")) channel_name = vm["channel"].as<string>();
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
    void stop() { socket.close(); done = true; }
    bool done;
private:
    zmq::socket_t socket;
	std::string channel_name;
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

CommandThread::CommandThread() : done(false), socket(*MessagingInterface::getContext(), ZMQ_REP) {
		int port = MessagingInterface::uniquePort(10000, 10999);
		char buf[40];
		snprintf(buf, 40, "tcp://0.0.0.0:%d", port);
		std::cerr << "listening for commands on port " << port << "\n";
		int retries = 3;
		while (retries>0) {
			try {
                socket.bind (buf);
                return;
			}
			catch (zmq::error_t err) {
				if (--retries>0) {
					std::cerr << "error binding to port.." << zmq_strerror(zmq_errno()) << "..trying again\n";
					usleep(100000);
					continue;
				}
				else
					std::cerr << "error binding to port.. giving up. Command port not available\n";
			}
		}
}

struct CommandRefresh : public Command {
	bool run(std::vector<Value> &params);
};

bool CommandRefresh::run(std::vector<Value> &params) {
	error_str = "refresh command not implemented";
	return false;
}


struct CommandUnknown : public Command {
    bool run(std::vector<Value> &params);
};

bool CommandUnknown::run(std::vector<Value> &params) {
    error_str = "Unknown command";
    return false;
}


struct CommandInfo : public Command {
	bool run(std::vector<Value> &params);
};

bool CommandInfo::run(std::vector<Value> &params) {
	char buf[100];
	snprintf(buf, 100, "scope version %d.%d", Scope_VERSION_MAJOR, Scope_VERSION_MAJOR);
	result_str = buf;
	return true;
}

struct CommandMonitor : public Command {
	bool run(std::vector<Value> &params);
};

bool CommandMonitor::run(std::vector<Value> &params) {
	char buf[1000];
	if (params.size()<2) {
		error_str = "usage: MONITOR machine_name | MONITOR PATTERN pattern | MONITOR PROPERTY property value";
		return false;
	}
	if (params.size() == 3 && params[1] == "PATTERN") {
		snprintf(buf, 1000, "CHANNEL %s ADD MONITOR PATTERN %s", 
			current_channel.c_str(), params[2].asString().c_str());
	}
	else if (params.size() == 4 && params[1] == "PROPERTY") {
		snprintf(buf, 1000, "CHANNEL %s ADD MONITOR PROPERTY %s \"%s\"",
                 current_channel.c_str(), params[2].asString().c_str(), params[3].asString().c_str());
	}
	else if (params.size() == 2) {
		snprintf(buf, 1000, "CHANNEL %s ADD MONITOR %s", 
			current_channel.c_str(), params[1].asString().c_str());		
	}
	else {
		error_str = "usage: MONITOR machine_name | MONITOR PATTERN pattern";
		return false;
	}
	std::cerr <<buf << "\n";
	zmq::socket_t sock(*MessagingInterface::getContext(), ZMQ_REQ);
	sock.connect("inproc://remote_commands");
	sock.send(buf, strlen(buf));
    cerr << "sent internal command: " << buf << "\n";
	size_t len = sock.recv(buf, 1000);
	if (len) {
		buf[len] = 0;
		result_str = buf;
        cerr << "got internal response: " << buf << "\n";
		return true;
	}
	else {
		error_str = "failed to issue command";
		return false;
	}
}

struct CommandStopMonitor : public Command {
	bool run(std::vector<Value> &params);
};

bool CommandStopMonitor::run(std::vector<Value> &params) {
	char buf[100];
	if (params.size()<2) {
		error_str = "usage: UNMONITOR machine_name | UNMONITOR PATTERN pattern";
		return false;
	}
	if (params.size() == 3 && params[1] == "PATTERN") {
		snprintf(buf, 1000, "CHANNEL %s REMOVE MONITOR PATTERN %s", 
			current_channel.c_str(), params[2].asString().c_str());
	}
	else if (params.size() == 2) {
		snprintf(buf, 100, "CHANNEL %s REMOVE MONITOR %s", 
			current_channel.c_str(), params[1].asString().c_str());
	}
	else {
		error_str = "usage: UNMONITOR machine_name | UNMONITOR PATTERN pattern | UNMONITOR PROPERTY property value";
		return false;
	}
	std::cerr <<buf << "\n";
	zmq::socket_t sock(*MessagingInterface::getContext(), ZMQ_REQ);
	sock.connect("inproc://remote_commands");
	sock.send(buf, strlen(buf));
	size_t len = sock.recv(buf, 100);
	if (len) {
		buf[len] = 0;
		result_str = buf;
		return true;
	}
	else {
		error_str = "failed to issue command";
		return false;
	}
}

void sendMessage(zmq::socket_t &socket, const char *message) {
    const char *msg = (message) ? message : "";
    size_t len = strlen(msg);
    zmq::message_t reply (len);
    memcpy ((void *) reply.data (), msg, len);
    socket.send (reply);
}

void CommandThread::operator()() {
    cerr << "------------------ Command Thread Started -----------------\n";
    
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
            std::cerr << "Command thread received " << data << std::endl;
            std::istringstream iss(data);

            std::list<Value> *parts = 0;

			// read the command, attempt to read a structured form first, 
			// and try a simple form if that fails
			std::string cmd;
			if (!MessageEncoding::getCommand(data, cmd, &parts)) {
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
			if (parts)
				std::copy(parts->begin(), parts->end(), std::back_inserter(params));
			delete parts;
            if (params.empty()) {
                sendMessage(socket, "Empty message received\n");
                goto cleanup;
            }
			{
				Command *command = 0;
				std::string ds(params[0].asString());
				if (ds == "info") {
					command = new CommandInfo();
				}
				else if (ds == "monitor" || ds == "MONITOR") {
					command = new CommandMonitor();
				}
				else if (ds == "unmonitor" || ds == "UNMONITOR") {
					command = new CommandStopMonitor();
				}
				else if (ds == "refresh" || ds == "REFRESH") {
					command = new CommandRefresh();
				}
                else command = new CommandUnknown;
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
			if (errno) std::cerr << "error during client communication: " << strerror(errno) << "\n" << std::flush;
            if (zmq_errno())
                std::cerr << zmq_strerror(zmq_errno()) << "\n" << std::flush;
            else
                std::cerr << " Exception: " << e.what() << "\n" << std::flush;
			if (zmq_errno() == EFSM)
				exit(1);
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

std::string escapeNonprintables(const char *buf) {
    const char *hex = "0123456789ABCDEF";
	std::string res;
	while (*buf) {
		if (isprint(*buf)) res += *buf;
		else if (*buf == '\015') res += "\\r";
		else if (*buf == '\012') res += "\\n";
		else if (*buf == '\010') res += "\\t";
		else {
			const char tmp[3] = { hex[ (*buf & 0xf0) >> 4], hex[ (*buf & 0x0f)], 0 };
			res = res + "#{" + tmp + "}";
		}
		++buf;
	}
	return res;
}

static bool need_refresh = false;

class SetupConnectMonitor : public EventResponder {
public:
    void operator()(const zmq_event_t &event_, const char* addr_) {
        need_refresh = true;
    }
};


int main(int argc, const char * argv[])
{
	zmq::context_t context;
	MessagingInterface::setContext(&context);
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
    std::cerr << "-------- Starting Command Interface ---------\n";
    CommandThread cmdline;
    boost::thread cmd_interface(boost::ref(cmdline));
    
    zmq::socket_t cmd(*MessagingInterface::getContext(), ZMQ_REP);
    cmd.bind("inproc://remote_commands");
    
    SubscriptionManager subscription_manager(options.channel().c_str(), eCLOCKWORK, 
			options.subscriberHost().c_str(), options.subscriberPort());
    
    struct timeval start;
    gettimeofday(&start, 0);
    stringstream output;
	unsigned int retry_count = 3;
    for (;;) {
        zmq::pollitem_t items[] = {
                { subscription_manager.setup(), 0, ZMQ_POLLERR | ZMQ_POLLIN, 0 },
                { subscription_manager.subscriber(), 0, ZMQ_POLLERR | ZMQ_POLLIN, 0 },
                { cmd, 0, ZMQ_POLLERR | ZMQ_POLLIN, 0 }
            };
        try {
				if (!subscription_manager.checkConnections(items, 3, cmd)) {
					current_channel = "";
					usleep(100000);
					continue;
				}
				if (current_channel.length() == 0)
					current_channel = subscription_manager.current_channel;
				retry_count = 3;
			}
			catch(zmq::error_t err) {
				std::cerr << zmq_strerror(errno) << "\n";
				if (zmq_errno() == EFSM) retry_count--;
				if (retry_count == 0) exit(1);
				continue;
			}
			if ( !(items[1].revents & ZMQ_POLLIN) || (items[1].revents & ZMQ_POLLERR) )
					continue;
            
			try {
			zmq::message_t update;
			subscription_manager.subscriber().recv(&update, ZMQ_NOBLOCK);

			struct timeval now;
			gettimeofday(&now, 0);

			long scale = 1;
			if (options.reportMillis()) scale = 1000;

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
				if (MessageEncoding::getCommand(data, op, &message)) {
					if (op == "STATE"&& message->size() == 2) {
						machine = message->front().asString();
						message->pop_front();
						state = message->front().asString();
						int state_num = lookupState(state);
						map<string, int>::iterator idx = device_map.find(machine);
						if (idx == device_map.end()) //new machine
							device_map[machine] = next_device_num++;
						output << get_diff_in_microsecs(&now, &start)/scale 
							<< "\t" << machine << "\t" << state << "\t" << state_num;
					}
					else if (op == "UPDATE") {

						//output << get_diff_in_microsecs(&now, &start)/scale << data << "\n";
						output << get_diff_in_microsecs(&now, &start)/scale;
						std::list<Value>::iterator iter = message->begin();
						while (iter!= message->end()) {
							const Value &v =  *iter++;
							output << v; if (iter != message->end()) output << "\t";
						}
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
						
						output << get_diff_in_microsecs(&now, &start)/ scale
							<< "\t" << property << "\tvalue\t";
						if (val.kind == Value::t_string)
						 	output << "\"" << escapeNonprintables(val.asString().c_str()) << "\"";
						else
							output << escapeNonprintables(val.asString().c_str());
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
				
						output << get_diff_in_microsecs(&now, &start)/scale
							<< "\t" << machine << "\t" << state << "\t" << state_num;
		            }
					else if (op == "VALUE" && !options.ignoreValues()) {
						property = machine;
						map<string, int>::iterator idx = device_map.find(property);
						if (idx == device_map.end()) //new machine
							device_map[property] = next_device_num++;

	
						if (options.onlyNumericValues()) {
							long val;
							if (outputNumeric(output, iss, val)) 
								output << get_diff_in_microsecs(&now, &start) /scale
								<< "\t" << property << "\tvalue\t" << val;
						}
						else {
							std::string val(outputRemaining(output, iss));
							output << get_diff_in_microsecs(&now, &start) /scale
								<< "\t" << property << "\tvalue\t" << escapeNonprintables(val.c_str());
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
        catch(exception& e) {
            cerr << "error: " << e.what() << "\n";
        }
        catch(...) {
            cerr << "Exception of unknown type!\n";
        }
    }
    cmdline.stop();
    cmd_interface.join();

    return 0;
}

