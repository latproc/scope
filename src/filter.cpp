#include <iostream>
#include <regular_expressions.h>
#include <list>
#include <string.h>
#include <time.h>
#include "convert_date.h"

int main(int argc, char *argv[])
{
    bool fix_time = false; // don't rewrite the timestamp
    std::list<rexp_info *>patterns;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--fix-time") == 0) {
            fix_time = true;
            continue;
        }
        rexp_info *info = create_pattern(argv[i]);
        if (info->compilation_result == 0) {
            patterns.push_back(info);
        }
        else {
            std::cerr << "failed to compile regexp: " << argv[i] << "\n";
            release_pattern(info);
        }
    }

    while (!std::cin.eof()) {
        std::string buf;
        while (std::getline(std::cin, buf)) {
            if (fix_time) {
                DateTime dt;
                auto error = parse_8601_datetime(buf, dt);
                if (error == none) {
                    size_t buflen = buf.length() + 40;
                    std::vector<char> updated(buflen);
                    time_t local_time = timegm(&dt.datetime);
                    localtime_r(&local_time, &dt.datetime);
                    const auto &t = dt.datetime;
                    snprintf(updated.data(), buflen, "%04d-%02d-%02d %02d:%02d:%02d.%06d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, dt.frac_sec);
                    int len = strlen(updated.data());
                    const char *data_start = strchr(buf.c_str(), ' ');
                    if (data_start == nullptr) { data_start = strchr(buf.c_str(), '\t'); }
                    if (data_start == nullptr) {
                        data_start = buf.c_str();
                    }
                    snprintf(updated.data() + len, buflen - len, "%s", data_start);
                    buf = updated.data();
                }
            }

            if (!patterns.empty()) {
                std::list<rexp_info *>::iterator iter = patterns.begin();
                while (iter != patterns.end()) {
                    rexp_info *info = *iter++;
                    if (execute_pattern(info, buf.c_str()) == 0) {
                        std::cout << buf << "\n" << std::flush;
                        break;
                    }
                }
            }
            else {
                std::cout << buf << "\n" << std::flush;
            }
        }
    }
    return 0;
}
