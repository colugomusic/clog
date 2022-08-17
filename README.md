# clog

- [expire.hpp](#expirehpp)
- [idle.hpp](#idlehpp)

## expire.hpp
[include/clog/expire.hpp](include/clog/expire.hpp)
```c++
struct thing : public clog::expirable { ... };
thing my_thing;
...
//
// A one-shot "expiry" signal. Expected usage is to call this when
// the object has not been deleted yet but probably will be soon.
//
// Any references to this object should consider it to be in an
// expired state from now on, i.e. it should be considered to be
// dead or "as good as deleted" by the program logic.
//
// A `clog::expiry_pointer` is a safe reference to a `clog::expirable`.
//
// The other mechanisms below provide ways to register tasks to
// be run at the point of expiry. The motivation behind that is to
// allow objects to safely disconnect themselves from other objects
// when they are deleted.
//
// This way it is not necessary to, for example, continuously check
// if a referenced object has been deleted by looking it up by ID or
// some other method. Instead, the code path which references the
// object can just be turned off completely when the object is expired.
//
// Expire is called automatically by the destructor, if it hasn't
// already been called at that point.
//
my_thing.expire();
...
//
// Example of another object which might want to hold a reference
// to `my_thing`
//
struct observer
{
  auto observe_thing(thing* my_thing) -> void
  {
    //
    // Task to run when `my_thing` is expired (method 1, unsafe)
    //
    const auto on_expired_1 = [this]()
    {
      // Note that we are capturing `this` so we might do something
      // with this observer object when `my_thing` is expired.
      
      // This is very bad if the lifetime of `my_thing` outlasts this
      // object, because `my_thing` will still run this task when it
      // is expired. The connection is not removed automatically when
      // the observer object is deleted.
      
      // Therefore this method should only be used if you know that
      // this observer object will stay alive for the lifetime of
      // `my_thing`.
      
      // Otherwise, use `make_expiry_observer` instead (see below)
    };
    
    // Note that there is no way to remove this connection once it
    // has been added. It stays alive for the lifetime of `my_thing`.
    my_thing->observe_expiry(on_expired_1);
    
    ...
    
    //
    // Task to run when `my_thing` is expired (method 2, safe)
    //
    const auto on_expired_2 = [this]()
    {
      // The connection stays alive until `thing_expiry_` is
      // destructed, so when this observer object is deleted
      // this task won't ever be run anymore.
    };
    
    // This connection stays alive until `thing_expiry_` goes out
    // of scope, or `my_thing` is deleted.
    thing_expiry_ = my_thing->make_expiry_observer(on_expired_2);
    
    ...
    
    //
    // Creating a smart pointer to `my_thing` which knows when it
    // has been expired or deleted.
    //
    auto expiry_pointer = my_thing->make_expiry_pointer<thing>();
    
    expiry_pointer.is_expired(); // Returns true if `my_thing` is expired
    expiry_pointer.get(); // Returns null if `my_thing` is expired
    
    // You can optionally add a single task to run when `my_thing`
    // is expired. The connection is removed when `expiry_pointer``
    // goes out of scope.
    expiry_pointer.on_expired([](){ ... });
  }
  
  clog::expiry_observer thing_expiry_;
};
```

## idle.hpp
[include/clog/idle.hpp](include/clog/idle.hpp)

Safely push tasks onto a queue to be executed efficiently "during idle time".

The concept of "idle time" is whatever you want it to be but the motivating usage is a system based on a typical event loop where `idle_task_processor::process_all()` can be called at regular intervals.

An ID can be passed along with any task to prevent it from being run more than once per idle frame.

```c++
clog::idle_task_processor processor;

auto idle_time() -> void
{
  processor.process_all();
}

struct object
{
  clog::idle_task_pusher pusher { processor.make_idle_task_pusher() };
  
  auto foobar() -> void
  {
    pusher.push([this]()
    {
      // This task will be run the next time the task queue is processed.
      
      // Note that `this` is being captured which suggests that this object
      // will be accessed. This is safe even if this object is deleted before
      // the task queue is processed, because when `pusher` goes out of scope
      // the task will be removed from the queue automatically, and won't
      // ever be run.
    });
    
    // IDs for tasks that should only be run at most once each time the
    // task queue is processed.
    //
    // These IDs are local to the pusher
    enum class task_id
    {
      some_task,
      another_task,
    };
    
    // This task won't be pushed if a task with the same ID was already
    // pushed since the last time the task queue was processed.
    pusher.push(task_id::some_task, []()
    {
      // This task will only be run, at most, once every time the task queue
      // is processed.
    });
    
    // Note that for performance reasons the task IDs are used as indices
    // into an array, so they should start at zero and increment by 1 (so
    // an enum works fine.)
    //
    // So don't just use an ID like '500' or something because when the
    // pusher sees '500' it will allocate memory for that many indexed tasks.
  }
};

```
