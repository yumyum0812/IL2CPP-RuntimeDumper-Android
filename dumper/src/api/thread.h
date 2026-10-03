#pragma once

#include "functions.h"
#include "domain.h"


namespace Api {
    struct Thread {
        Thread() = delete;
        
        static Thread* Attach(Domain* domain) {
            return Functions::thread_attach(domain);
        }

        void Detach() {
            return Functions::thread_detach(this);
        }
    };
}