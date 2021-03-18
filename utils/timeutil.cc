#include "timeutil.h"
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace std::chrono;

uint64_t gettickcount() {
    time_point <std::chrono::system_clock, milliseconds> tp =
            time_point_cast<milliseconds>(system_clock::now());
    
    return tp.time_since_epoch().count();
}

void printcurrtime() {
    auto t = system_clock::to_time_t(system_clock::now());
    // TODO deprecate put_time
    std::cout << std::put_time(std::localtime(&t), "%m-%d %H:%M:%S");
}

