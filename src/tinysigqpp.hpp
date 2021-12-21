#ifndef _TINYSIGQPP_HPP
#define _TINYSIGQPP_HPP
#include <queue>
#include <csignal>
#include <pthread.h>
#include <functional>
#include <forward_list>

// NOTE: You should only have one tinysigqpp instance

namespace tinysigqpp {
    class kill_me;

    class basic_tinysigqpp {
        friend kill_me;
    public:
        // Sets a signal mask for the whole process, creates the signal catching thread
        basic_tinysigqpp();
        // Should only be called when the process is done
        virtual ~basic_tinysigqpp();

        basic_tinysigqpp(const basic_tinysigqpp &) = delete;              // no copy
        basic_tinysigqpp &operator=(const basic_tinysigqpp &) = delete;   // no copy
        basic_tinysigqpp(basic_tinysigqpp &&) = delete;                   // no move
        basic_tinysigqpp &operator=(const basic_tinysigqpp &&) = delete;  // no move

        // Returns whether a signal was available
        bool get_signal(siginfo_t *sig);

    protected:
        // Different implementations of this can exist - e.g. one that updates an eventfd
        virtual bool add_to_queue(siginfo_t sig);

    private:
        static void *signal_receiver_thread(void *);

        pthread_t t;  // I'm using pthread here because only pthread supports sigmask

        std::queue<siginfo_t> signals;
        pthread_mutex_t signals_lock = PTHREAD_MUTEX_INITIALIZER;  // used to lock the signal queue

        struct kill_me_struct {
            std::function<bool(siginfo_t)> check;
            std::function<void(siginfo_t)> action;
            pthread_t who;
            int cancel_handle;

            // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
            kill_me_struct(std::function<bool(siginfo_t)> check, std::function<void(siginfo_t)> action, pthread_t who, int cancel_handle)
                : check { std::move(check) }, action { std::move(action) }, who { who }, cancel_handle { cancel_handle } {}
        };

        std::deque<kill_me_struct> kill_me_list;
        int max_cancel_handle = 0;
        pthread_mutex_t kill_me_list_lock = PTHREAD_MUTEX_INITIALIZER;  // for both max_cancel_handle and kill_me_list

        unsigned int signal_count[64];  // counter of each signal in the queue, used for e.g. checking whether SIGKILL was set at all
    };

    class tinysigqpp_eventfd : public basic_tinysigqpp {
    public:
        explicit tinysigqpp_eventfd(int eventfd);
        ~tinysigqpp_eventfd() override = default;

        tinysigqpp_eventfd(const basic_tinysigqpp &) = delete;              // no copy
        tinysigqpp_eventfd &operator=(const basic_tinysigqpp &) = delete;   // no copy
        tinysigqpp_eventfd(basic_tinysigqpp &&) = delete;                   // no move
        tinysigqpp_eventfd &operator=(const basic_tinysigqpp &&) = delete;  // no move

    protected:
        bool add_to_queue(siginfo_t sig) override;
    private:
        int eventfd;
    };

    // Used to set/unset a "kill me" which is a signal you want to be interrupted for, multiple threads can get one signal this way
    class kill_me {
    public:
        kill_me(basic_tinysigqpp &sigq, std::function<bool(siginfo_t)> check, std::function<void(siginfo_t)> handler);
        void cancel();  // Stop this kill me
        // Normal copy and move because this is only a wrapper for holding a signal queue and cancel handle

    private:
        basic_tinysigqpp &parent;
        int cancel_handle;
    };
}

#endif
