#include "custom_eventloop.h"

#include <sys/time.h>
#include <cmath>

#include "application.h"
#include "app_log.h"
#include "globals.h"

#define  TIMERS_SLOT_NAME       "eventTimers"
#define  MIN_DELAY              1.0
#define  MIN_WAIT               1.0
#define  MAX_WAIT               60000.0
#define  MAX_EXPIRIES           10

#define  MAX_TIMERS             4096     /* this is quite excessive for embedded use, but good for testing */

int EventLoop::next_id_ = 0;
mutex EventLoop::mtx_;
map<duk_context*,map<int, EventLoop::TimerStruct> > EventLoop::all_timers_;
map<duk_context*,EventLoop*> EventLoop::all_eventloops_;
duk_function_list_entry EventLoop::eventloop_funcs[] = {
 { "createTimer", create_timer, 3 },
 { "deleteTimer", delete_timer, 1 },
 { "requestExit", request_exit, 0 },
 { NULL, NULL, 0 }
};

EventLoop::EventLoop(duk_context *ctx):
  exit_requested_(0),current_timeout_(0) {
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
  map<int,TimerStruct> *timers = getTimers(ctx);
  JSApplication *app = JSApplication::getApplications().at(ctx);
  double now;
  double diff;
  int timeout;

  (void) udata;

  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "EventLoop");
  }
  DBOUT( "eventloop_run(): Got EventLoop property");
  DBOUT(__func__ << "(): Timers size " << timers->size());

  eventloop->exit_requested_ = 0;

  for(;;) {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());

    {
      if(interrupted || eventloop->exitRequested()) {
        DBOUT( "eventloop_run(): Asked for termination");
        break;
      }

      if(app->getAppState() == JSApplication::APP_STATES::PAUSED) {
        app->getDuktapeMutex().unlock();
        std::unique_lock<mutex> cv_lock(app->getCVMutex());
        app->getEventLoopCV().wait_for(cv_lock, std::chrono::milliseconds(int(MAX_WAIT)), [eventloop,app]{
            return interrupted || !eventloop || eventloop->exitRequested() || !app || app->getAppState() == JSApplication::APP_STATES::RUNNING;
        });
        continue;
      }

      expire_timers(ctx);

      if(timers->size() == 0) {
        timeout = (int) MAX_WAIT;
      }

      now = get_now();

      timeout = (int)MAX_WAIT;
      diff = MAX_WAIT;

      for (const auto &timer : *timers) {
        diff = timer.second.target - now;
        diff = floor(diff);
        if (diff < MIN_WAIT) {
          diff = MIN_WAIT;
        } else if (diff > MAX_WAIT) {
            diff = MAX_WAIT;
        }

        timeout = diff < timeout ? diff : timeout;  /* clamping ensures that fits */
      }

      // timeout = floor(diff / 2) < MIN_WAIT ? MIN_WAIT : floor(diff / 2);
      timeout = diff;
      // updating current timeout
      eventloop->setCurrentTimeout(timeout);
    }
    // unlocking app mutexes if for some reason they are not released by lock guard
    app->getDuktapeMutex().unlock();
    JSApplication::getStaticMutex().unlock();

    // handling sigint event
    signal(SIGINT, sigint_handler);

    try {
      std::unique_lock<mutex> cv_lock(app->getCVMutex());
      app->getEventLoopCV().wait_for(cv_lock, std::chrono::milliseconds(timeout), [eventloop]{
          return interrupted || !eventloop || eventloop->exitRequested();
      });

      if(interrupted || eventloop->exitRequested()) {
        DBOUT( "eventloop_run(): Asked for termination");
        break;
      }
    } catch(const std::system_error& e) {
      ERROUT("Some random error: " << e.what());
      duk_pop(app->getContext());
      return 0;
    }

  }

  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    duk_pop(app->getContext());
  }

  DBOUT( "eventloop_run(): Run was executed successfully");

  return 0;
}

int EventLoop::expire_timers(duk_context *ctx) {
  if(!ctx) return 0;

  JSApplication *app = JSApplication::getApplications().at(ctx);

  int sanity = MAX_EXPIRIES;
  double now = get_now();
  int rc;
  EventLoop *eventloop = getAllEventLoops().at(ctx);
  map<int,TimerStruct> *timers = getTimers(ctx);

  {
    std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
    duk_push_global_stash(ctx);
    duk_get_prop_string(ctx, -1, TIMERS_SLOT_NAME);
    /* [ ... stash eventTimers ] */

    for(;sanity > 0 && (MAX_EXPIRIES-sanity) < (int)timers->size(); --sanity) {
        // exit has been requested...
        if (eventloop->exitRequested()) {
          break;
        }

        // if there is nothing to expire
        if(timers->size() == 0) {
          break;
        }

        // find expired timers and mark them
        for (auto &timer : *timers) {
          if(timer.second.removed) {
            break;
          }

          if(timer.second.target < now) {
            timer.second.expiring = 1;
            timer.second.target = now + timer.second.delay;
          } else if(timer.second.target > now && timer.second.expiring == 1) {
            {
              std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());
              // executing callback
              duk_push_number(ctx, (double) timer.second.id);
              duk_get_prop(ctx, -2);
              rc = duk_pcall(ctx, 0 /*nargs*/);  /* -> [ ... stash eventTimers retval ] */
              if(rc != 0) {
                if (duk_is_error(ctx, -1)) {
                  /* Accessing .stack might cause an error to be thrown, so wrap this
                   * access in a duk_safe_call() if it matters.
                   */
                   ERROUT(__func__ << "(): Call error");
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
              timer.second.expiring = 0;

              // checking oneshot
              if(timer.second.oneshot) {
                timer.second.removed = 1;
              }
            }
            // breaking to avoid too long callback executions on one run
            break;
          } else {
            timer.second.expiring = 0;
          }
        }

        duk_pop_2(ctx);  /* -> [ ... ] */
    }
  }

  return 0;
}

int EventLoop::create_timer(duk_context *ctx) {
  DBOUT( "Creating new timer" );
  double now = get_now();
  double delay;
  bool oneshot;
  map<int, TimerStruct> *timers = getTimers(ctx);
  TimerStruct timer;
  JSApplication *app = JSApplication::getApplications().at(ctx);
  std::lock_guard<recursive_mutex> duktape_lock(app->getDuktapeMutex());

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

   DBOUT(__func__ << "(): Delay " << delay << " loaded");
   DBOUT(__func__ << "(): Oneshot type is: " << duk_get_type(ctx,2));

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
   timer.id = EventLoop::next_id_;
   ++EventLoop::next_id_;
   timer.target = now + delay;
   timer.removed = 0;
   timer.expiring = 0;

   DBOUT(__func__ << "(): Timer id: " << (double)timer.id);

   // inserting new timer (or old)
   addTimer(ctx,timer);

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
  removeTimer(ctx, (int)timer_id);

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
  JSApplication *app = JSApplication::getApplications().at(ctx);
  {
    std::unique_lock<mutex> cv_lock(app->getCVMutex());
    eventloop->setRequestExit(1);
  }
  app->getCVMutex().unlock();
  app->getEventLoopCV().notify_one();
  return 0;
}

map<int, EventLoop::TimerStruct>* EventLoop::getTimers(duk_context *ctx) {
  if(getAllTimers().find(ctx) != getAllTimers().end()) {
    return &all_timers_.at(ctx);
  }

  return 0;
}

void EventLoop::addTimer(duk_context *ctx, const TimerStruct& timer) {
  if(getAllTimers().find(ctx) != getAllTimers().end()) {
    all_timers_.at(ctx).insert(pair<int,TimerStruct>(timer.id,timer));
  } else {
    createNewTimers(ctx);
    all_timers_.at(ctx).insert(pair<int,TimerStruct>(timer.id,timer));
  }
  return;
}

void EventLoop::removeTimer(duk_context *ctx, int id) {
  if(getAllTimers().find(ctx) != getAllTimers().end()) {
    all_timers_.at(ctx).erase(id);
  }
  return;
}

void EventLoop::createNewTimers(duk_context *ctx) {
  if(getAllTimers().find(ctx) != getAllTimers().end()) {
    return;
  }

  all_timers_.insert(pair<duk_context*,map<int, TimerStruct> >(ctx,map<int,TimerStruct>()));
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

void EventLoop::deleteEventLoop(duk_context *ctx) {
  if(getAllEventLoops().find(ctx) != getAllEventLoops().end()) {
    delete EventLoop::all_eventloops_.at(ctx);
    EventLoop::all_eventloops_.erase(ctx);
  }

  if(getAllTimers().find(ctx) != getAllTimers().end()) {
    EventLoop::all_timers_.erase(ctx);
  }


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
