#include <thread>
#include "dumper.h"


void ThreadMain() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    RunDumper();
}

__attribute__((constructor))
void LibMain() {
    auto t = std::thread(ThreadMain);
    t.detach();
}