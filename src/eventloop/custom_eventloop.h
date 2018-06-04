#ifndef CUSTOM_EVENTLOOP_H
#define CUSTOM_EVENTLOOP_H

#if defined (__cplusplus)
extern "C" {
#endif

  #include "duktape.h"
  #include <signal.h>
  #include "../globals.h"

#if defined (__cplusplus)
}
#endif

#include <iostream>
#include <vector>
#include <map>

using namespace std;

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
    void setRequestExit(bool request_exit) { exit_requested_ = request_exit; }

    static int eventloop_run(duk_context *ctx, void *udata);
    static int expire_timers(duk_context *ctx);
    static int create_timer(duk_context *ctx);
    static int delete_timer(duk_context *ctx);
    static int request_exit(duk_context *ctx);

    static duk_function_list_entry eventloop_funcs[];

  private:
    duk_context *ctx_;
    map<int,TimerStruct> *timers_;
    bool exit_requested_ = 0;

    static double get_now(void);

    static int next_id_;
    static map<duk_context*,map<int, TimerStruct>*> all_timers_;
    static map<duk_context*,EventLoop*> all_eventloops_;


};

#endif
