# clog

## expire.hpp

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
    
    thing_expiry_ = my_thing->make_expiry_observer(on_expired_2);
    
    ...
    
    //
    // Creating a pointer to `my_thing` which knows when it has
    // been expired or deleted.
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
