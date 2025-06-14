#include "convert_date.h"
#include <exception>
#include <iostream>
#include <string>
#include <time.h>
#include <iomanip>

std::ostream &operator<<(std::ostream &out, const Term &term)
{
    switch (term) {
        case none: out << "none"; break;
        case year: out << "year"; break;
        case monsep: out << "monsep"; break;
        case mon: out << "mon"; break;
        case daysep: out << "daysep"; break;
        case day: out << "day"; break;
        case timesep: out << "timesep"; break;
        case hour: out << "hour"; break;
        case minsep: out << "minsep"; break;
        case min: out << "min"; break;
        case secsep: out << "secsep"; break;
        case secs: out << "secs"; break;
        case fracsecs: out << "fracsecs"; break;
        case zonesep: out << "zonesep"; break;
        case zonehr: out << "zonehr"; break;
        case zhmsep: out << "zhmsep"; break;
        case zonemin: out << "zonemin"; break;
    }
    return out;
}

void display(tm &time, int frac_sec)
{
    time_t local_time = timegm(&time);
    localtime_r(&local_time, &time);

    std::cout << (time.tm_year + 1900) << "-"
        << std::setw(2) << std::setfill('0') << (time.tm_mon + 1)
        << "-"
        << std::setw(2) << std::setfill('0') << time.tm_mday
        << " "
        << std::setw(2) << std::setfill('0') << time.tm_hour
        << ":"
        << std::setw(2) << std::setfill('0') << time.tm_min
        << ":"
        << std::setw(2) << std::setfill('0') << time.tm_sec;
    if (frac_sec) {
        std::cout << '.' << frac_sec;
    }
    std::cout << std::endl;
}

void display(DateTime &datetime)
{
    display(datetime.datetime, datetime.frac_sec);
}

Term parse_8601_datetime(const std::string &input, DateTime &result)
{
    struct Error {
        Term term;
        char ch;
    };
        Error error = {none, '\0'};
    try {
        tm time;
        time.tm_year = 0;
        time.tm_mon = 0;
        time.tm_mday = 0;
        time.tm_hour = 0;
        time.tm_min = 0;
        time.tm_sec = 0;
        int frac_sec = 0;
        const char *p = input.c_str();
        const char *q = p;
        auto append_to = [](int &field, const char *p) {
            field = field * 10 + *p - '0';
        };
        char date_sep = '-';
        char time_sep = ':';
        auto skip = [&p]() { ++p; };
        Term state = year;
        while (*p) {
            switch (state) {
                case none:
                    break;
                case year:
                    if (!isdigit(*p)) {
                        error = {state, *p};
                        break;
                    }
                    append_to(time.tm_year, p);
                    if (p - q == 3) {
                        q = p;
                        state = monsep;
                    }
                    break;
                case monsep:
                    state = mon;
                    if (*p == date_sep) { skip(); q=p; continue; }
                    q=p;
                    break;
                case mon:
                    if (!isdigit(*p)) {
                        error = {state, *p};
                        break;
                    }
                    append_to(time.tm_mon, p);
                    if (p - q == 1) {
                        q = p;
                        state = daysep;
                    }
                    break;
                case daysep:
                    state = day;
                    if (*p == date_sep) { skip(); q=p; continue; }
                    q=p;
                    break;
                case day:
                    if (!isdigit(*p)) {
                        error= Error{state, *p};
                        break;
                    }
                    append_to(time.tm_mday, p);
                    if (p - q == 1) {
                        q = p;
                        state = timesep;
                    }
                    break;
                case timesep:
                    state = hour;
                    if (*p == ' ' || *p == 'T') { skip(); q=p; continue; }
                    q = p;
                    break;
                case hour:
                    if (!isdigit(*p)) {
                        error= Error{state, *p};
                        break;
                    }
                    append_to(time.tm_hour, p);
                    if (p - q == 1) {
                        q = p;
                        state = minsep;
                    }
                    break;
                case minsep:
                    state = min;
                    if (*p == time_sep) { skip(); q=p; continue; }
                    q=p;
                    break;
                case min:
                    if (!isdigit(*p)) {
                        error= Error{state, *p};
                        break;
                    }
                    append_to(time.tm_min, p);
                    if (p - q == 1) {
                        q = p;
                        state = secsep;
                    }
                    break;
                case secsep:
                    state = secs;
                    if (*p == time_sep) { skip(); q=p; break; }
                    q=p;
                    break;
                case secs:
                    if (!isdigit(*p)) {
                        if (*p == '.') {
                            state = fracsecs;
                            break;
                        }
                        else if (*p == 'Z') {
                            state = zonehr;
                            break;
                        }
                        else if (isdigit(*p) && p - q > 2) {
                        error= Error{state, *p};
                            break;
                        }
                        else {
                            state = none;
                            break;
                        }
                    }
                    append_to(time.tm_sec, p);
                    break;
                case fracsecs:
                    if (!isdigit(*p)) {
                        if (*p == 'Z') {
                            state = zonehr;
                            break;
                        }
                        else {
                            state = none;
                            break;
                        }
                    }
                    append_to(frac_sec, p);
                    break;
                case zonesep:
                    break;
                case zonehr:
                    state = none;
                    break; // TODO
                    break;
                case zhmsep:
                    break;
                case zonemin:
                    break;
            }
            if (state == none || error.term != none) {
                break;
            }
            ++p;
        };
        if (error.term != none) {
            throw std::runtime_error("Unexpected character '" + std::string(1, error.ch) + "' at term: " + std::to_string(error.term));
        }
        else if (!*p || state == none) {
            time.tm_year -= 1900;
            time.tm_mon -= 1;
            result.datetime = time;
            result.frac_sec = frac_sec;
        }
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return error.term;
    }
    return none;
}

#ifdef TESTING
int main(int argc, char *argv[])
{

    for (int i = 1; i < argc; ++i) {
        DateTime dt;
        Term error;
        if ((error = parse_8601_datetime(argv[i], dt)) == none) {
            display(dt);
        }
        else {
            std::cerr << "Unexpected datetime format at term: " << error << "\n";
        }
    }

    return EXIT_SUCCESS;
}
#endif
