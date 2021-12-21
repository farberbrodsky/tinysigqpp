#include "tinysigqpp.hpp"
#include <iostream>  // TODO: remove this, debug-by-printing only
#include <functional>
using namespace tinysigqpp;

void *basic_tinysigqpp::signal_receiver_thread(void *arg) {
    auto *self = reinterpret_cast<basic_tinysigqpp *>(arg);  // like this but in argument

    sigset_t set;
    sigfillset(&set);

    siginfo_t sig;

    while (true) {
        sigwaitinfo(&set, &sig);
        self->add_to_queue(sig);
    }

    return nullptr;
}

void basic_tinysigqpp::add_to_queue(siginfo_t sig) {
    // go over all kill_mes for a match, anything that matches gets the signal
    bool matches_any_kill_me = false;
    if (pthread_mutex_lock(&this->kill_me_list_lock)) throw std::runtime_error { "Can't lock mutex" };
    try {
        for (auto &s : this->kill_me_list) {
            if (s.check(sig)) {
                matches_any_kill_me = true;
                union sigval v;
                auto *arg = new std::tuple<std::function<void(siginfo_t)>, siginfo_t>(s.action, sig);
                v.sival_ptr = arg;
                if (pthread_sigqueue(s.who, SIGIO, v)) {
                    // idk why this failed but free memory anyway
                    delete arg;
                }
            }
        }
    } catch (...) {
        pthread_mutex_unlock(&this->kill_me_list_lock);
        throw;  // rethrow exception
    }

    pthread_mutex_unlock(&this->kill_me_list_lock);

    if (matches_any_kill_me) return;

    // otherwise, add it to the signal list
    if (pthread_mutex_lock(&this->signals_lock)) throw std::runtime_error { "Can't lock mutex" };
    try {
        ++(this->signal_count[sig.si_signo]);
        this->signals.push(sig);
    } catch (...) {
        pthread_mutex_unlock(&this->signals_lock);
        throw;  // rethrow exception
    }

    pthread_mutex_unlock(&this->signals_lock);
}

bool basic_tinysigqpp::get_signal(siginfo_t *sig) {
    // TODO maybe add an atomic value with the number of signals so if there aren't any it's much faster
    if (pthread_mutex_lock(&this->signals_lock)) throw std::runtime_error { "Can't lock mutex" };
    try {
        if (this->signals.empty()) {
            pthread_mutex_unlock(&this->signals_lock);
            return false;
        }

        *sig = this->signals.front();
        this->signals.pop();

        pthread_mutex_unlock(&this->signals_lock);
        return true;
    } catch (...) {
        pthread_mutex_unlock(&this->signals_lock);
        throw;  // rethrow exception
    }
}

static void signal_handler(int, siginfo_t *sig, void *) {
    auto *data = reinterpret_cast<std::tuple<std::function<void(siginfo_t)>, siginfo_t> *>(sig->si_value.sival_ptr);
    std::get<0>(*data)(std::get<1>(*data));
    delete data;
}

basic_tinysigqpp::basic_tinysigqpp() {
    // block all signals except sigio in this process
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGIO);
    sigprocmask(SIG_BLOCK, &set, nullptr);
    struct sigaction siga;
    siga.sa_flags = SA_SIGINFO;
    sigemptyset(&siga.sa_mask);
    siga.sa_sigaction = signal_handler;
    sigaction(SIGIO, &siga, nullptr);

    // start the signal receiver
    pthread_create(&t, nullptr, basic_tinysigqpp::signal_receiver_thread, this);
}

basic_tinysigqpp::~basic_tinysigqpp() {
    pthread_cancel(t);
    pthread_join(t, nullptr);
}

kill_me::kill_me(basic_tinysigqpp &sigq, std::function<bool(siginfo_t)> check, std::function<void(siginfo_t)> handler) : parent { sigq } {
    if (pthread_mutex_lock(&sigq.kill_me_list_lock)) throw std::runtime_error { "Can't lock mutex" };
    try {
        this->cancel_handle = (sigq.max_cancel_handle)++;
        sigq.kill_me_list.emplace_back(std::move(check), std::move(handler), pthread_self(), this->cancel_handle);
    } catch (...) {
        pthread_mutex_unlock(&sigq.kill_me_list_lock);
        throw;  // rethrow exception
    }

    pthread_mutex_unlock(&sigq.kill_me_list_lock);
}

void kill_me::cancel() {
    if (pthread_mutex_lock(&parent.kill_me_list_lock)) throw std::runtime_error { "Can't lock mutex" };
    try {
        for (auto it = parent.kill_me_list.begin(), end = parent.kill_me_list.end(); it != end; ++it) {
            if (it->cancel_handle == this->cancel_handle) {
                // Found!
                parent.kill_me_list.erase(it);
                break;
            }
        }
    } catch (...) {
        pthread_mutex_unlock(&parent.kill_me_list_lock);
        throw;  // rethrow exception
    }

    pthread_mutex_unlock(&parent.kill_me_list_lock);
}
