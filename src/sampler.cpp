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
#include <Logger.h>
#include <DebugExtra.h>
#include <stdlib.h>
#include <fstream>
#include <signal.h>
#include <value.h>
#include <MessageEncoding.h>
#include <MessagingInterface.h>
#include <SocketMonitor.h>
#include <ConnectionManager.h>
#include <ScopeConfig.h>
#include <MessageHeader.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <map>
#include <time.h>

using namespace std;

namespace po = boost::program_options;

uint64_t get_diff_in_microsecs(struct timeval *now, struct timeval *then)
{
    uint64_t t = (now->tv_sec - then->tv_sec);
    t = t * 1000000 + (now->tv_usec - then->tv_usec);
    return t;
}

void interrupt_handler(int sig)
{
    exit(0);
}

map<string, int> state_map;
map<string, int> device_map;
std::string current_channel;

void save_devices()
{
    ofstream device_file("devices.dat");
    map<string, int>::iterator iter = device_map.begin();
    while (iter != device_map.end()) {
        device_file << (*iter).first << "\t" << (*iter).second << "\n";
        iter++;
    }
}

void save_state_names()
{
    ofstream states_file("states.dat");
    map<string, int>::iterator iter = state_map.begin();
    while (iter != state_map.end()) {
        states_file << (*iter).first << "\t" << (*iter).second << "\n";
        iter++;
    }
}

class SamplerOptions {
        static SamplerOptions *_instance;
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
        int cw_port;
        bool debug_flag;
        uint64_t user_start_time; // user provided base time
        bool timestamp;
        string output_format;
        string date_format;

        SamplerOptions() : subscribe_to_port(5556), subscribe_to_host("localhost"),
            publish_to_port(5560), publish_to_interface("*"),
            republish(false), quiet(false), raw(false), ignore_values(false), only_numeric_values(false),
            use_millis(true), channel_name("SAMPLER_CHANNEL"), cw_port(5555), debug_flag(false),
            user_start_time(0), timestamp(false), output_format("std"), date_format("iso8601")
        {}
    public:
        static SamplerOptions *instance() { if (!_instance) _instance = new SamplerOptions(); return _instance; }
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
        int clockworkPort() { return cw_port; }
        static bool debug() { return instance()->debug_flag; }
        void parseStartTime(uint64_t st) {

        }
        uint64_t userStartTime() { return user_start_time; }
        bool emitTimestamp() { return timestamp; }
        const std::string &format() { return output_format; }
        const std::string &dateFormat() { return date_format; }
};

bool SamplerOptions::parseCommandLine(int argc, const char *argv[])
{
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
        ("help", "produce help message")
        ("republish", "publish received messages")
        ("subscribe", po::value<string>(), "host to subscribe to [localhost]")
        ("subscribe-port", po::value<int>(), "port to subscribe to [5556]")
        ("interface", po::value<string>(),  "local interface to publish to [*]")
        ("publish-port", po::value<int>(), "port to subscribe to [5560]")
        ("quiet", "quiet mode, no output on stdout")
        ("raw", "process all received messages, not just state and value changes")
        ("ignore-values", "ignore value changes, only process state changes")
        ("only-numeric-values", "ignore value changes for non-numeric values")
        ("millisec", "report time offset in milliseconds (default)")
        ("microsec", "report time offset in microseconds")
        ("timestamp", "timestamp each result rather than displaying offset")
        ("channel", po::value<string>(), "name of channel to use")
        ("cw-port", po::value<int>(), "clockwork command port (5555)")
        ("debug", "debug info")
        ("start", po::value<string>(), "start time for time deltas")
        ("format", po::value<string>(), "select output format (std, kv, kvq)")
        ("date-format", po::value<string>(), "timestamp format (posix, iso8601) (implies --timestamp)")
        ;
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        if (vm.count("help")) {
            cerr << desc << "\n";
            return false;
        }
        if (vm.count("republish")) {
            republish = true;
        }
        if (vm.count("subscribe")) {
            subscribe_to_host = vm["subscribe"].as<string>();
        }
        if (vm.count("interface")) {
            publish_to_interface = vm["interface"].as<string>();
        }
        if (vm.count("subscribe-port")) {
            subscribe_to_port = vm["subscribe-port"].as<int>();
        }
        if (vm.count("publish-port")) {
            publish_to_port = vm["publish-port"].as<int>();
        }
        if (vm.count("quiet")) {
            quiet = true;
        }
        if (vm.count("raw")) {
            raw = true;
        }
        if (vm.count("ignore-values")) {
            ignore_values = true;
        }
        if (vm.count("only-numeric-values")) {
            only_numeric_values = true;
        }
        if (vm.count("millisec")) {
            use_millis = true;
        }
        if (vm.count("microsec")) {
            use_millis = false;
        }
        if (vm.count("channel")) {
            channel_name = vm["channel"].as<string>();
        }
        if (vm.count("cw-port")) {
            cw_port = vm["cw-port"].as<int>();
        }
        if (vm.count("debug")) {
            debug_flag = true;
        }
        if (vm.count("start")) {
            parseStartTime(vm["start"].as<uint64_t>());
        }
        if (vm.count("timestamp")) {
            timestamp = true;
        }
        if (vm.count("format")) {
            output_format = vm["format"].as<string>();
            const std::set<string> valid_formats = {"std", "kv", "kvq"};
            if (!valid_formats.count(output_format)) {
                cerr << "error: invalid output format '" << output_format << "'\n";
                cerr << "valid formats are: std, kv, kvq\n";
                return false;
            }
        }
        if (vm.count("date-format")) {
            date_format = vm["date-format"].as<string>();
            timestamp = true;
            const std::set<string> valid_date_formats = {"posix", "iso8601"};
            if (!valid_date_formats.count(date_format)) {
                cerr << "error: invalid date format '" << date_format << "'\n";
                cerr << "valid formats are: posix, iso8601\n";
                return false;
            }
        }
    }
    catch (const exception &e) {
        cerr << "error: " << e.what() << "\n";
        return false;
    }
    catch (...) {
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
        virtual ~Command() { }
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

CommandThread::CommandThread() : done(false), socket(*MessagingInterface::getContext(), ZMQ_REP)
{
    int port = MessagingInterface::uniquePort(10000, 10999);
    char buf[40];
    snprintf(buf, 40, "tcp://0.0.0.0:%d", port);
    std::cerr << "listening for commands on port " << port << "\n";
    int retries = 3;
    while (retries > 0) {
        try {
            socket.bind(buf);
            return;
        }
        catch (const zmq::error_t &err) {
            if (--retries > 0) {
                std::cerr << "error binding to port.." << zmq_strerror(zmq_errno()) << "..trying again\n";
                usleep(100000);
                continue;
            }
            else {
                std::cerr << "error binding to port.. giving up. Command port not available\n";
            }
        }
    }
}

struct CommandRefresh : public Command {
    bool run(std::vector<Value> &params);
};

bool CommandRefresh::run(std::vector<Value> &params)
{
    error_str = "refresh command not implemented";
    return false;
}


struct CommandUnknown : public Command {
    bool run(std::vector<Value> &params);
};

bool CommandUnknown::run(std::vector<Value> &params)
{
    error_str = "Unknown command";
    return false;
}


struct CommandInfo : public Command {
    bool run(std::vector<Value> &params);
};

bool CommandInfo::run(std::vector<Value> &params)
{
    char buf[100];
    snprintf(buf, 100, "scope version %d.%d", Scope_VERSION_MAJOR, Scope_VERSION_MAJOR);
    result_str = buf;
    return true;
}

struct CommandMonitor : public Command {
    bool run(std::vector<Value> &params);
};

bool CommandMonitor::run(std::vector<Value> &params)
{
    char buf[1000];
    if (params.size() < 2) {
        error_str = "usage: MONITOR machine_name | MONITOR PATTERN pattern | MONITOR PROPERTY property value";
        return false;
    }
    if (params.size() == 3 && params[1] == "PATTERN") {
        char *raw_pat = strdup(params[2].asString().c_str());
        const char *pat = raw_pat;
        if (*raw_pat == '"') {
            ++pat;
        }
        if (*pat && pat[strlen(pat) - 1] == '"') {
            raw_pat[strlen(raw_pat) - 1] = 0;
        }
        snprintf(buf, 1000, "CHANNEL %s ADD MONITOR PATTERN %s",
                current_channel.c_str(), pat);
        free(raw_pat);
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
    if (SamplerOptions::debug()) {
        std::cerr << buf << "\n";
    }
    zmq::socket_t sock(*MessagingInterface::getContext(), ZMQ_REQ);
    sock.connect("inproc://remote_commands");
    sock.send(buf, strlen(buf));
    if (SamplerOptions::debug()) {
        cerr << "sent internal command: " << buf << "\n";
    }
    size_t len = sock.recv(buf, 1000);
    if (len) {
        buf[len] = 0;
        result_str = buf;
        if (SamplerOptions::debug()) {
            cerr << "got internal response: " << buf << "\n";
        }
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

bool CommandStopMonitor::run(std::vector<Value> &params)
{
    char buf[100];
    if (params.size() < 2) {
        error_str = "usage: UNMONITOR machine_name | UNMONITOR PATTERN pattern";
        return false;
    }
    if (params.size() == 3 && params[1] == "PATTERN") {
        char *raw_pat = strdup(params[2].asString().c_str());
        const char *pat = raw_pat;
        if (*raw_pat == '"') {
            ++pat;
        }
        if (*pat && pat[strlen(pat) - 1] == '"') {
            raw_pat[strlen(raw_pat) - 1] = 0;
        }
        snprintf(buf, 100, "CHANNEL %s REMOVE MONITOR PATTERN %s",
                current_channel.c_str(), params[2].asString().c_str());
        free(raw_pat);
    }
    else if (params.size() == 2) {
        snprintf(buf, 100, "CHANNEL %s REMOVE MONITOR %s",
                current_channel.c_str(), params[1].asString().c_str());
    }
    else {
        error_str = "usage: UNMONITOR machine_name | UNMONITOR PATTERN pattern | UNMONITOR PROPERTY property value";
        return false;
    }
    if (SamplerOptions::debug()) {
        std::cerr << buf << "\n";
    }
    zmq::socket_t sock(*MessagingInterface::getContext(), ZMQ_REQ);
    sock.connect("inproc://remote_commands");
    try {
        sock.send(buf, strlen(buf));
        size_t len;
        int count = 0;

        while ((len = sock.recv(buf, 100)) == 0 && ++count < 3) ;
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
    catch (const zmq::error_t &err) {
        std::cerr << zmq_strerror(errno) << "\n";
        if (zmq_errno() == EFSM) {
            FileLogger fl(program_name);
            fl.f() << zmq_strerror(errno)
                    << "in CommandStopMonitor::run() \n" << std::flush;
        }
    }
    return false;
}

void sendMessage(zmq::socket_t &socket, const char *message)
{
    const char *msg = (message) ? message : "";
    size_t len = strlen(msg);
    zmq::message_t reply(len);
    memcpy((void *) reply.data(), msg, len);
    socket.send(reply);
}

enum RecoveryType { no_recovery, send_recovery, recv_recovery } recovery_type;

void CommandThread::operator()()
{
    if (SamplerOptions::debug()) {
        cerr << "------------------ Command Thread Started -----------------\n";
    }

    while (!done) {
        try {

            zmq::pollitem_t items[] = { { socket, 0, ZMQ_POLLERR | ZMQ_POLLIN, 0 } };
            zmq::poll(&items[0], 1, 500 * 1000);
            if (!(items[0].revents & ZMQ_POLLIN)) {
                continue;
            }

            zmq::message_t request;
            if (!socket.recv(&request)) {
                continue;  // interrupted system call
            }
            size_t size = request.size();
            char *data = (char *)malloc(size + 1);
            memcpy(data, request.data(), size);
            data[size] = 0;
            if (SamplerOptions::debug()) {
                std::cerr << "Command thread received " << data << std::endl;
            }
            std::istringstream iss(data);

            std::list<Value> *parts = 0;

            // read the command, attempt to read a structured form first,
            // and try a simple form if that fails
            std::string cmd;
            if (!MessageEncoding::getCommand(data, cmd, &parts)) {
                parts = new std::list<Value>;
                std::string s;
                while (iss >> s) {
                    parts->push_back(Value(s.c_str()));
                }
            }

            std::vector<Value> params(0);
            params.push_back(Value{cmd});
            if (parts) {
                std::copy(parts->begin(), parts->end(), std::back_inserter(params));
            }
            delete parts;
            if (params.empty()) {
                sendMessage(socket, "Empty message received\n");
                goto cleanup;
            }
            {
                Command *command = 0;
                std::string ds(params[0].asString());
                if (ds == "info" || ds == "INFO") {
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
                else {
                    command = new CommandUnknown;
                }
                if ((*command)(params)) {
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
        catch (const std::exception &e) {
            if (errno) {
                std::cerr << "error during client communication: " << strerror(errno) << "\n" << std::flush;
            }
            if (zmq_errno()) {
                std::cerr << zmq_strerror(zmq_errno()) << "\n" << std::flush;
            }
            else {
                std::cerr << " Exception: " << e.what() << "\n" << std::flush;
            }
            if (zmq_errno() == EFSM) {
                exit(1);
            }
        }
    }
    socket.close();
}

int outputNumeric(std::ostream &out, std::istream &in, long &val)
{
    // convert the input to an integer value and output the name and value if
    // the conversion is successful
    string s_value;
    in >> s_value;
    char *remainder;
    val = strtol(s_value.c_str(), &remainder, 0);
    return (*remainder == 0);
}

std::string outputRemaining(std::ostream &out, std::istream &in)
{
    // copy the remainder of the stream as raw data
    char buf[25];
    char *rbuf;
    int pos = in.tellg();
    in.seekg(0, in.end);
    int endpos = in.tellg();
    int size = endpos - pos;
    in.seekg(pos, in.beg);
    if (size >= 25) {
        rbuf = new char [size + 1];
    }
    else {
        rbuf = buf;
    }
    std::streambuf *pbuf = in.rdbuf();
    pbuf->sgetn(rbuf, size);
    rbuf[size] = 0;

    std::string res(rbuf);
    if (size >= 25) {
        delete rbuf;
    }
    return res;
}

static int next_device_num = 0;
static int next_state_num = 0;

int lookupState(std::string &state)
{
    int state_num = 0;
    map<string, int>::iterator idx = state_map.find(state);
    if (idx == state_map.end()) {// new state
        state_num = next_state_num;
        state_map[state] = next_state_num++;
        save_state_names();
    }
    else {
        state_num = (*idx).second;
    }
    return state_num;
}

std::string escapeNonprintables(const char *buf)
{
    const char *hex = "0123456789ABCDEF";
    std::string res;
    while (*buf) {
        if (isprint(*buf)) {
            res += *buf;
        }
        else if (*buf == '\015') {
            res += "\\r";
        }
        else if (*buf == '\012') {
            res += "\\n";
        }
        else if (*buf == '\010') {
            res += "\\t";
        }
        else {
            const char tmp[3] = { hex[(*buf & 0xf0) >> 4], hex[(*buf & 0x0f)], 0 };
            res = res + "#{" + tmp + "}";
        }
        ++buf;
    }
    return res;
}

static bool need_refresh = false;

class SetupConnectMonitor : public EventResponder {
    public:
        void operator()(const zmq_event_t &event_, const char *addr_) {
            need_refresh = true;
        }
};

SamplerOptions *SamplerOptions::_instance = 0;

std::ostream &timestamp(std::ostream &out, uint64_t offset, long scale, bool use_datetime, const string &dateformat)
{
    using namespace boost::posix_time;
    if (use_datetime) {
        if (dateformat == "posix") {
            time_t rawtime;
            struct tm timeinfo;

            time(&rawtime);
            localtime_r(&rawtime, &timeinfo);
            char buf[40];
            asctime_r(&timeinfo, buf);
            size_t n = strlen(buf);
            if (n > 1 && buf[n - 1] == '\n') {
                buf[n - 1] = 0;
            }
            out << buf << " " << timeinfo.tm_zone;
        }
        else if (dateformat == "iso8601") {
            ptime t = microsec_clock::universal_time();
            out << to_iso_string(t) << "Z";
        }
        else {
            out << "unknown date format: " << dateformat;
        }
    }
    else {
        return out << offset / scale;
    }
    return out;
}

int main(int argc, const char *argv[])
{
    char *pn = strdup(argv[0]);
    program_name = strdup(basename(pn));
    free(pn);

    zmq::context_t context;
    MessagingInterface::setContext(&context);
    SamplerOptions &options = *SamplerOptions::instance();
    if (!options.parseCommandLine(argc, argv)) {
        return 1;
    }

    if (options.quietMode() && !options.publish()) {
        cerr << "Warning: not writing to stdout or zmq\n";
    }
    //  if (options.debug())
    //    LogState::instance()->insert(DebugExtra::instance()->DEBUG_CHANNELS);

    MessagingInterface *mif = 0;
    if (options.publish()) {
        mif = MessagingInterface::create("*", options.publisherPort());
    }

    atexit(save_devices);
    atexit(save_state_names);
    signal(SIGINT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);

    // this should be a separate thread
    if (SamplerOptions::debug()) {
        std::cerr << "-------- Starting Command Interface ---------\n";
    }
    CommandThread cmdline;
    boost::thread cmd_interface(boost::ref(cmdline));

    zmq::socket_t cmd(*MessagingInterface::getContext(), ZMQ_REP);
    cmd.bind("inproc://remote_commands");

    SubscriptionManager subscription_manager(options.channel().c_str(), eCLOCKWORK,
            options.subscriberHost().c_str(), options.subscriberPort());
    subscription_manager.configureSetupConnection(
            options.subscriberHost().c_str(), options.clockworkPort());
    struct timeval start;
    gettimeofday(&start, 0);
    uint64_t first_message_time = options.userStartTime(); // can be initialised on the commandline
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
                gettimeofday(&start, 0);
                continue;
            }
            if (current_channel.length() == 0) {
                current_channel = subscription_manager.current_channel;
            }
            retry_count = 3;
        }
        catch (const zmq::error_t &err) {
            std::cerr << zmq_strerror(errno) << "\n";
            if (zmq_errno() == EFSM) {
                retry_count--;
                exit(0);
            }
            if (retry_count == 0) {
                exit(1);
            }
            continue;
        }
        if (!(items[1].revents & ZMQ_POLLIN) || (items[1].revents & ZMQ_POLLERR)) {
            continue;
        }

        try {
            #if 0
            zmq::message_t update;
            subscription_manager.subscriber().recv(&update, ZMQ_NOBLOCK);
            long len = update.size();
            char *data = (char *)malloc(len + 1);
            memcpy(data, update.data(), len);
            data[len] = 0;
            #else
            MessageHeader mh;
            char *data = 0;
            size_t len = 0;
            if (!safeRecv(subscription_manager.subscriber(), &data, &len, false, 1, mh)) {
                std::cout << "failed to receive message\n";
            }
            if (first_message_time == 0) {
                first_message_time = mh.start_time;
            }
            #endif

            long scale = 1;
            if (options.reportMillis()) {
                scale = 1000;
            }

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
                    if (message == nullptr) {
                        std::cerr << "unexpected empty parameter list for recieved message: " << op << "\n";
                    }
                    else if (op == "STATE" && message->size() == 2) {
                        machine = message->front().asString();
                        message->pop_front();
                        state = message->front().asString();
                        int state_num = lookupState(state);
                        map<string, int>::iterator idx = device_map.find(machine);
                        if (idx == device_map.end()) { //new machine
                            device_map[machine] = next_device_num++;
                        }
                        if (options.format() == "std") {
                            timestamp(output, mh.start_time - first_message_time, scale, options.emitTimestamp(), options.dateFormat());
                            output << "\t" << machine << "\t" << state << "\t" << state_num;
                        }
                        else if (options.format() == "kv") {
                            output << "machine: " << machine << ", state: " << state << ", timestamp: ";
                            timestamp(output, mh.start_time - first_message_time, scale, options.emitTimestamp(), options.dateFormat());
                        }
                        else if (options.format() == "kvq") {
                            output << "\"machine\": \""
                                    << machine << "\", \"state\": \""
                                    << state << "\", \"timestamp\": ";
                            if (options.emitTimestamp()) {
                                output << "\"";
                            }
                            timestamp(output, mh.start_time - first_message_time, scale, options.emitTimestamp(), options.dateFormat());
                            if (options.emitTimestamp()) {
                                output << "\"";
                            }
                        }
                    }
                    else if (op == "UPDATE") {
                        output << (mh.start_time - first_message_time) / scale;
                        std::list<Value>::iterator iter = message->begin();
                        while (iter != message->end()) {
                            const Value &v =  *iter++;
                            output << v;
                            if (iter != message->end()) {
                                output << "\t";
                            }
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
                        if (idx == device_map.end()) { //new machine
                            device_map[property] = next_device_num++;
                        }

                        std::string value_str;
                        if (val.kind == Value::t_string) {
                            value_str = "\"";
                            value_str += escapeNonprintables(val.asString().c_str());
                            value_str += "\"";
                        }
                        else {
                            value_str = escapeNonprintables(val.asString().c_str());
                        }

                        if (options.format() == "std") {
                            timestamp(output, mh.start_time - first_message_time, scale, options.emitTimestamp(), options.dateFormat());
                            output << "\t" << property << "\tvalue\t" << value_str;
                        }
                        else if (options.format() == "kv") {
                            output << "machine: " << machine << ", " << prop << ": " << value_str << ", timestamp: ";
                            timestamp(output, mh.start_time - first_message_time, scale, options.emitTimestamp(), options.dateFormat());
                        }
                        else if (options.format() == "kvq") {
                            output << "\"machine\": \""
                                    << machine << "\", \"" << prop << "\": "
                                    << value_str << ", \"timestamp\": ";
                            if (options.emitTimestamp()) {
                                output << "\"";
                            }
                            timestamp(output, mh.start_time - first_message_time, scale, options.emitTimestamp(), options.dateFormat());
                            if (options.emitTimestamp()) {
                                output << "\"";
                            }
                        }
                    }
                    else {
                        std::cerr << "unexpected message " << op << " with " << (message != nullptr ? message->size() : 0) << " paramters\n";
                    }
                    delete message;
                }
                else {
                    struct timeval now;
                    gettimeofday(&now, 0);
                    istringstream iss(data);
                    std::string machine;
                    iss >> machine >> op;
                    if (op == "STATE") {
                        iss >> state;
                        int state_num = lookupState(state);
                        map<string, int>::iterator idx = device_map.find(machine);
                        if (idx == device_map.end()) { //new machine
                            device_map[machine] = next_device_num++;
                        }

                        output << get_diff_in_microsecs(&now, &start) / scale
                                << "\t" << machine << "\t" << state << "\t" << state_num;
                    }
                    else if (op == "VALUE" && !options.ignoreValues()) {
                        property = machine;
                        map<string, int>::iterator idx = device_map.find(property);
                        if (idx == device_map.end()) { //new machine
                            device_map[property] = next_device_num++;
                        }

                        if (options.onlyNumericValues()) {
                            long val;
                            if (outputNumeric(output, iss, val))
                                output << get_diff_in_microsecs(&now, &start) / scale
                                        << "\t" << property << "\tvalue\t" << val;
                        }
                        else {
                            std::string val(outputRemaining(output, iss));
                            output << get_diff_in_microsecs(&now, &start) / scale
                                    << "\t" << property << "\tvalue\t" << escapeNonprintables(val.c_str());
                        }
                    }
                }
            }
            if (!output.str().empty()) {
                if (!options.quietMode()) {
                    cout << output.str() << "\n" << flush;
                }
                if (options.publish()) {
                    mif->send(output.str().c_str());
                }
            }
            delete[] data;
        }
        catch (const exception &e) {
            cerr << "error: " << e.what() << "\n";
        }
        catch (...) {
            cerr << "Exception of unknown type!\n";
        }
    }
    cmdline.stop();
    cmd_interface.join();

    return 0;
}
