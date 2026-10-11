#include <thread>
#include "dumper.h"

__attribute__((constructor))
void LibMain() {
    auto t = std::thread(RunDumper);
    t.detach();
}