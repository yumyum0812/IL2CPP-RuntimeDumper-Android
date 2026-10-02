#pragma once

#include "functions.h"
#include "domain.h"


namespace Api {
    struct Thread {
        Thread() = delete;
        
        static Thread* Attach(Domain* domain) {
            return Functions::il2cpp_thread_attach(domain);
        }

        void Detach() {
            return Functions::il2cpp_thread_detach(this);
        }
    };
}