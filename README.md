# clog

- [expire.hpp](#expirehpp)
- [idle.hpp](#idlehpp)
- [sometimes_sorted_vector.hpp](#sometimes_sorted_vectorhpp)

## expire.hpp
[include/clog/expire.hpp](include/clog/expire.hpp)

Allow interconnected objects to let each other know when they have been deleted, or will be deleted soon. Register tasks to run when objects expire.

```c++
struct thing : public clog::expirable { ... };
thing my_thing;
...
//
// A one-shot "expiry" signal. Expected usage is to call this when
// the object is going to be deleted soon. If the signal is not
// emitted explicitly then it will always be emitted automatically
// in the destructor.
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
// `expire` is called automatically by the destructor, if it hasn't
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

Tasks can be pushed onto the queue by an object which might potentially be deleted before the task is run. In a naive system this would be a problem if the pushed task captures `this`, because by the time the task queue is processed, `this` could now refer to the deleted object.

In this system this is not a problem because in order to push tasks onto the queue we have to create a `clog::idle_task_pusher` object which automatically removes all associated tasks from the queue when it goes out of scope.

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
  clog::idle_task_pusher pusher { processor.make_pusher() };
  
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

## sometimes_sorted_vector.hpp
[include/clog/sometimes_sorted_vector.hpp](include/clog/sometimes_sorted_vector.hpp)

Extends `std::vector` to add the following methods:

### `contains(T item) const -> bool`

Precondition: The vector must be sorted.

Performs a binary search to check if the value is in the vector.

### `insert(T item) -> std::pair<iterator, bool>`

Precondition: The vector must be sorted.

Inserts the value into the sorted vector.

Returns: `std::pair<(iterator to inserted item), (true if insertion was successful)>`

### `erase(T item) -> size_type`

Precondition: The vector must be sorted.

Removes the value from the sorted vector.

Returns the number of items removed (0 or 1).

### Notes

- The vector is allowed to become unsorted (e.g. if you manipulate it using the normal `std::vector` methods.)
- Calling the above functions on an unsorted vector is invalid.
- It is the client's responsibility to keep track of whether or not the vector is currently sorted.
