#pragma once

#include "functions.h"
#include "domain.h"


namespace Dumper::Api {
    struct Thread {
        static Thread* Attach(Domain* domain) {
            return (Thread*) Functions::il2cpp_thread_attach(domain);
        }

        void Detach() {
            return Functions::il2cpp_thread_detach(this);
        }
    };
}