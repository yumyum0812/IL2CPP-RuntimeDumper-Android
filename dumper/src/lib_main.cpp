#include <thread>
#include "dumper.h"


void ThreadMain() {
    RunDumper();
}

__attribute__((constructor))
void LibMain() {
    auto t = std::thread(ThreadMain);
    t.detach();
}