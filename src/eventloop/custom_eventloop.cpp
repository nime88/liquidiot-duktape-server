#include "custom_eventloop.h"

#include <sys/time.h>
#include <poll.h>
#include <cmath>

#include "application.h"
#include "app_log.h"

#if defined (__cplusplus)
extern "C" {
#endif

  #include "globals.h"

#if defined (__cplusplus)
}
#endif

// #define NDEBUG

#ifdef NDEBUG
    #include <iostream>
    using std::cout;
    #define DBOUT( x ) cout << x  << "\n"
#else
    #define DBOUT( x )
#endif

#define  TIMERS_SLOT_NAME       "eventTimers"
#define  MIN_DELAY              1.0
#define  MIN_WAIT               1.0
#define  MAX_WAIT               1000.0
#define  MAX_EXPIRIES           10

#define  MAX_TIMERS             4096     /* this is quite excessive for embedded use, but good for testing */

int EventLoop::next_id_ = 0;
mutex EventLoop::mtx_;
map<duk_context*,map<int, EventLoop::TimerStruct>*> EventLoop::all_timers_;
map<duk_context*,EventLoop*> EventLoop::all_eventloops_;
duk_function_list_entry EventLoop::eventloop_funcs[] = {
 { "createTimer", create_timer, 3 },
 { "deleteTimer", delete_timer, 1 },
 { "requestExit", request_exit, 0 },
 { NULL, NULL, 0 }
};

EventLoop::EventLoop(duk_context *ctx) {
  evloopRegister(ctx);

  // adding timers to global timers
  createNewTimers(ctx);
}

void EventLoop::evloopRegister(duk_context *ctx) {
  JSApplication *app = JSApplication::getApplications().at(ctx);

  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    /* Set global 'EventLoop'. */
    duk_push_global_object(ctx);
    duk_push_object(ctx);
    duk_put_function_list(ctx, -1, eventloop_funcs);
    duk_put_prop_string(ctx, -2, "EventLoop");
    duk_pop(ctx);

    /* Initialize global stash 'eventTimers'. */
    duk_push_global_stash(ctx);
    duk_push_object(ctx);
    duk_put_prop_string(ctx, -2, TIMERS_SLOT_NAME);
    duk_pop(ctx);
  }
}

int EventLoop::eventloop_run(duk_context *ctx, void *udata) {
  DBOUT( "eventloop_run(): Starting");
  EventLoop *eventloop = getAllEventLoops().at(ctx);
  map<int,TimerStruct> *timers = getAllTimers().at(ctx);
  JSApplication *app = JSApplication::getApplications().at(ctx);
  double now;
  double diff;
  int timeout;

  (void) udata;

  // DBOUT( "eventloop_run(): Data initialized");
  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "EventLoop");
  }

  DBOUT( "eventloop_run(): Got EventLoop property");

  for(;;) {
    {
      std::lock_guard<recursive_mutex> app_lock(app->getAppMutex());
      if(interrupted || eventloop->exitRequested()) {
        DBOUT( "eventloop_run(): Asked for termination");
        break;
      }

      if(app->getAppState() == JSApplication::APP_STATES::PAUSED) {
        poll(NULL,0,MIN_WAIT);
        continue;
      }

      DBOUT( "eventloop_run(): Expiring timers");
      expire_timers(ctx);

      if(timers->size() == 0) {
        timeout = (int) MAX_WAIT;
      }

      now = get_now();

      timeout = (int)MAX_WAIT;
      diff = MAX_WAIT;

      for (map<int, TimerStruct>::iterator it = timers->begin(); it != timers->end(); ++it) {
        diff = it->second.target - now;
        diff = floor(diff);
        if (diff < MIN_WAIT) {
          diff = MIN_WAIT;
        } else if (diff > MAX_WAIT) {
            diff = MAX_WAIT;
        }

        timeout = diff < timeout ? diff : timeout;  /* clamping ensures that fits */
      }

      timeout = floor(diff / 2) < MIN_WAIT ? MIN_WAIT : floor(diff / 2);
      // updating current timeout
      eventloop->setCurrentTimeout(timeout);

      DBOUT( "eventloop_run(): Timers, diff " << diff);
    }
    // unlocking app mutexes if for some reason they are not released by lock guard
    app->getAppMutex().unlock();
    app->getDuktapeMutex().unlock();
    JSApplication::getStaticMutex().unlock();

    DBOUT( "eventloop_run(): Starting poll() wait for " << timeout << " millis");
    poll(NULL,0,timeout);
    // handling sigint event
    signal(SIGINT, sigint_handler);
  }

  DBOUT( "eventloop_run(): Run was executed successfully");

  return 0;
}

int EventLoop::expire_timers(duk_context *ctx) {
   DBOUT( "expire_timers()" );
  if(!ctx) return 0;

  JSApplication *app = JSApplication::getApplications().at(ctx);

  // DBOUT( "expire_timers(): Starting expire timers");
  int sanity = MAX_EXPIRIES;
  double now = get_now();
  int rc;
  EventLoop *eventloop = getAllEventLoops().at(ctx);
  map<int,TimerStruct> *timers = getAllTimers().at(ctx);

  // DBOUT( "expire_timers(): Successfully initialized variables");
  // DBOUT( "expire_timers(): " << ctx << " - " << timers->size());

  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, TIMERS_SLOT_NAME);
    /* [ ... stash eventTimers ] */

    // DBOUT( "expire_timers(): Reading timers form heap slot successful");

    for(;sanity > 0 && (MAX_EXPIRIES-sanity) < (int)timers->size(); --sanity) {
        // exit has been requested...
        if (eventloop->exitRequested()) {
          break;
        }

        // if there is nothing to expire
        if(timers->size() == 0) {
          break;
        }

        // DBOUT( "expire_timers(): Loop, there are " << timers->size() << " timers");
        // find expired timers and mark them
        for (map<int, TimerStruct>::iterator it = timers->begin(); it != timers->end(); ++it) {
          if(it->second.removed) {
            // duk_push_number(ctx, (double) it->second.id);
            // duk_del_prop(ctx, -2);
            // timers->erase(it);
            break;
          }

          if(it->second.target < now) {
            it->second.expiring = 1;
            it->second.target = now + it->second.delay;
          } else if(it->second.target > now && it->second.expiring == 1) {
            {
              std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
              // executing callback
              duk_push_number(ctx, (double) it->second.id);
              duk_get_prop(ctx, -2);
              rc = duk_pcall(ctx, 0 /*nargs*/);  /* -> [ ... stash eventTimers retval ] */
              if(rc != 0) {
                if (duk_is_error(ctx, -1)) {
                  /* Accessing .stack might cause an error to be thrown, so wrap this
                   * access in a duk_safe_call() if it matters.
                   */
                   duk_get_prop_string(ctx, -1, "stack");
                   JSApplication *app = JSApplication::getApplications().at(ctx);
                   AppLog(app->getAppPath().c_str()) <<
                     AppLog::getTimeStamp() << " [" << Constant::String::LOG_ERROR << "] " <<
                     duk_safe_to_string(ctx, -1) << "\n";

                   duk_pop(ctx);
                } else {
                  JSApplication *app = JSApplication::getApplications().at(ctx);
                  AppLog(app->getAppPath().c_str()) <<
                    AppLog::getTimeStamp() << " [" << Constant::String::LOG_ERROR << "] " <<
                    duk_safe_to_string(ctx, -1) << "\n";
                }
              }
              duk_pop(ctx);
              it->second.expiring = 0;

              // checking oneshot
              if(it->second.oneshot) {
                it->second.removed = 1;
              }
            }
            // breaking to avoid too long callback executions on one run
            break;
          } else {
            it->second.expiring = 0;
          }
        }

        // DBOUT( "expire_timers(): Loop, trying to pop last 2");
        duk_pop_2(ctx);  /* -> [ ... ] */
    }
  }

  // DBOUT( "expire_timers(): Expired timers successfully");
  return 0;
}

int EventLoop::create_timer(duk_context *ctx) {
  DBOUT( "Creating new timer" );
  double now = get_now();
  double delay;
  bool oneshot;
  map<int, TimerStruct> *timers = getAllTimers().at(ctx);
  TimerStruct timer;

  DBOUT( "Loading data successful" );

  /* indexes:
   *   0 = function (callback)
   *   1 = delay
   *   2 = boolean: oneshot
   */

   delay = duk_require_number(ctx, 1);
   if (delay < MIN_DELAY) {
     delay = MIN_DELAY;
   }

   DBOUT( "Delay " << delay << " loaded");
   DBOUT( "Oneshot type is: " << duk_get_type(ctx,2));

   if(duk_get_type(ctx,2) == DUK_TYPE_NONE) {
     // didn't find parameter, thus handling it properly
     oneshot = false;
   } else {
     oneshot = duk_require_boolean(ctx, 2);
   }

   DBOUT( "Oneshot " << oneshot << " loaded" );

   // restricting creation of timer if there is too much of them
   if (timers->size() >= MAX_TIMERS) {
     (void) duk_error(ctx, DUK_ERR_RANGE_ERROR, "out of timer slots");
   }

   //setting some values
   timer.delay = delay;
   timer.oneshot = oneshot;
   timer.id = EventLoop::next_id_++;
   timer.target = now + delay;
   timer.removed = 0;
   timer.expiring = 0;

   DBOUT( "Setting up timer successful" );

   // inserting new timer (or old)
   timers->insert(pair<int,TimerStruct>(timer.id,timer));

   DBOUT(  "Trying to register timer to global stash" );
   /* Finally, register the callback to the global stash 'eventTimers' object. */
   duk_push_global_stash(ctx);
   duk_get_prop_string(ctx, -1, TIMERS_SLOT_NAME);  /* -> [ func delay oneshot stash eventTimers ] */
   duk_push_number(ctx, (double) timer.id);
   duk_dup(ctx, 0);
   duk_put_prop(ctx, -3);  /* eventTimers[timer_id] = callback */

   DBOUT( "No errors on registering timer");

   /* Return timer id. */

   duk_push_number(ctx, (double) timer.id);

   return 1;
}

int EventLoop::delete_timer(duk_context *ctx) {
  double timer_id;
  bool found = 0;

  /* indexes:
   *   0 = timer id
   */

  timer_id = (double) duk_require_number(ctx, 0);

  // removing timer from existence
  found = getTimers(ctx)->erase((int)timer_id);

  /* The C++ state is now up-to-date, but we still need to delete
   * the timer callback state from the global 'stash'.
   */

  duk_push_global_stash(ctx);
  duk_get_prop_string(ctx, -1, TIMERS_SLOT_NAME);  /* -> [ timer_id stash eventTimers ] */
  duk_push_number(ctx, timer_id);
  duk_del_prop(ctx, -2);  /* delete eventTimers[timer_id] */

  duk_push_boolean(ctx, found);
	return 1;
}

int EventLoop::request_exit(duk_context *ctx) {
  EventLoop *eventloop = getAllEventLoops().at(ctx);
  eventloop->setRequestExit(1);
  return 0;
}

map<int, EventLoop::TimerStruct>* EventLoop::getTimers(duk_context *ctx) {
  if(getAllTimers().find(ctx) != getAllTimers().end()) {
    return getAllTimers().at(ctx);
  }

  return 0;
}

void EventLoop::createNewTimers(duk_context *ctx) {
  if(getAllTimers().find(ctx) != getAllTimers().end()) {
    return;
  }

  EventLoop::all_timers_.insert(pair<duk_context*,map<int, TimerStruct>*>(ctx,new map<int,TimerStruct>()));
}

EventLoop* EventLoop::getEventLoop(duk_context *ctx) {
  if(getAllEventLoops().find(ctx) != getAllEventLoops().end()) {
    return getAllEventLoops().at(ctx);
  }

  return 0;
}

void EventLoop::createNewEventLoop(duk_context *ctx, EventLoop *event_loop) {
  if(getAllEventLoops().find(ctx) != getAllEventLoops().end()) {
    return;
  }

  if(event_loop)
    EventLoop::all_eventloops_.insert(pair<duk_context*,EventLoop*>(ctx,event_loop));
  else
    EventLoop::all_eventloops_.insert(pair<duk_context*,EventLoop*>(ctx,new EventLoop(ctx)));
}

/* Get Javascript compatible 'now' timestamp (millisecs since 1970). */
double EventLoop::get_now(void) {
	struct timeval tv;
	int rc;

	rc = gettimeofday(&tv, NULL);
	if (rc != 0) {
		/* Should never happen, so return whatever. */
		return 0.0;
	}
	return ((double) tv.tv_sec) * 1000.0 + ((double) tv.tv_usec) / 1000.0;
}
