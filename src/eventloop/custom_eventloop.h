#ifndef CUSTOM_EVENTLOOP_H
#define CUSTOM_EVENTLOOP_H

#if defined (__cplusplus)
extern "C" {
#endif

  #include "duktape.h"

#if defined (__cplusplus)
}
#endif

#include <map>
#include <mutex>
using std::map;
using std::mutex;

class EventLoop {
  public:
    typedef struct {
    	int id;       /* numeric ID (returned from e.g. setTimeout); zero if unused */
    	double target;    /* next target time */
    	double delay;     /* delay/interval */
    	int oneshot;      /* oneshot=1 (setTimeout), repeated=0 (setInterval) */
    	int removed;      /* timer has been requested for removal */
      int expiring;     /* if timer has been set to be expired */
    } TimerStruct;

    // context for isolated operations
    EventLoop(duk_context *ctx);

    void evloopRegister(duk_context *ctx);
    inline bool exitRequested() { return exit_requested_; }
    void setRequestExit(bool request_exit) {
      {
        std::lock_guard<mutex> evloop_lock(getMutex());
        exit_requested_ = request_exit;
      }
    }
    void setCurrentTimeout(int timeout) { current_timeout_ = timeout; }
    int getCurrentTimeout() { return current_timeout_; }

    static inline mutex& getMutex() { return mtx_; }

    static int eventloop_run(duk_context *ctx, void *udata);
    static int expire_timers(duk_context *ctx);
    static int create_timer(duk_context *ctx);
    static int delete_timer(duk_context *ctx);
    static int request_exit(duk_context *ctx);

    static duk_function_list_entry eventloop_funcs[];

    /* ******************* */
    /* Setters and Getters */
    /* ******************* */

    static inline const map<duk_context*,map<int, TimerStruct>*>& getAllTimers() { return all_timers_; }
    static map<int, TimerStruct>* getTimers(duk_context *ctx);
    static void createNewTimers(duk_context *ctx);

    static inline const map<duk_context*,EventLoop*>& getAllEventLoops() { return all_eventloops_; }
    static EventLoop* getEventLoop(duk_context *ctx);
    static void createNewEventLoop(duk_context *ctx, EventLoop *event_loop);

  private:
    bool exit_requested_ = 0;
    int current_timeout_ = 0;
    static mutex mtx_;

    static double get_now(void);

    static int next_id_;
    static map<duk_context*,map<int, TimerStruct>*> all_timers_;
    static map<duk_context*,EventLoop*> all_eventloops_;


};

#endif
