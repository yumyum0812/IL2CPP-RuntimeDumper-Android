#include <thread>
#include "dumper/dumper.h"


void ThreadMain() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto dumper = Dumper::RuntimeDumper();
    dumper.Run();
}

__attribute__((constructor))
void LibMain() {
    std::thread(ThreadMain).detach();
}