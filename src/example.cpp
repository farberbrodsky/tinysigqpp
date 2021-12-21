#include "tinysigqpp.hpp"
#include <ctime>
#include <iostream>
#include <unistd.h>
using namespace tinysigqpp;

int main() {
    basic_tinysigqpp sig_queue;

    kill_me k1 { sig_queue, [](siginfo_t sig)  {
        return sig.si_signo == SIGINT;
    }, [](siginfo_t) {
        std::cout << "You sent SIGINT, tinysigqpp works!" << std::endl;
    } };

    std::cout << "Example" << std::endl;
    std::cout << getpid() << std::endl;
    struct timespec req { 3, 0 };
    struct timespec rem;
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) req = rem;

    k1.cancel();

    siginfo_t sig;
    while (sig_queue.get_signal(&sig)) {  // only gets events that don't match any kill_me in any thread
        std::cout << "Unhandled: " << sig.si_signo << std::endl;
    }

    return 0;
}
