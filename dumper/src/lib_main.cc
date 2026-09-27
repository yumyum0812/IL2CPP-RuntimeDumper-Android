#include <thread>
#include "dumper/dumper.h"


void ThreadMain() {
    auto dumper = Dumper::RuntimeDumper();
    dumper.Run();
}

__attribute__((constructor))
void LibMain() {
    std::thread(ThreadMain).detach();
}