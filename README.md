# clog

spaghetti code spider web object oriented hell scape. all software is garbage and so are you

## Requirements
- c++17

## Libraries
- [cache.hpp](include/clog/cache.hpp) - a single cached value
- [expire.hpp](include/clog/expire.hpp) - dying object notifications, not documented
- [idle.hpp](#idlehpp) - object task queues
- [property.hpp](#propertyhpp) - set/get property library
- [rcv.hpp](include/clog/rcv.hpp) - reusable cell vector, some documentation in header
- [signal.hpp](#signalhpp) - single-threaded signal/slot library
- [vectors.hpp](include/clog/vectors.hpp) - operations for manipulating sorted vectors, documentation in header

## signal.hpp
[include/clog/signal.hpp](include/clog/signal.hpp)

It's a single-threaded signal/slot libary. I wrote this because `boost::signals2` proved from profiling to be a bottleneck in my project and I couldn't find any other library that I liked.

Advantages of this library over `boost::signals2`:
- It's faster. I haven't benchmarked anything but I improved a bunch of visible lag in my program simply by switching everything over to this library. (Yes i know you can use a dummy mutex with boost if you don't need multithreading support. It's still much slower than this library even if you do that.)
- The code isn't a stupid mess so it's much easier to follow the control flow through connections in a debugger.

Disadvantages:
- Not thread safe
- Written by a moron

Some quirks of this library:
- Signals and connections are moveable but not copyable
- Connections are always scoped, i.e. you get a connection object which automatically disconnects the slot when it goes out of scope. You can't simply set and forget a connection. Connection methods are marked with `[nodiscard]]` so you have to do something with the result. This may be annoying in cases where you know the signal won't outlive the slot but in my opinion it is worth it to be less error prone.
- There's no `disconnect` method. Disconnects happen automatically when the `clog::cn` goes out of scope. If you want to explicitly disconnect you can just do `connection = {};`

```c++
struct emitter
{
  clog::signal<std::string> hello;
  clog::signal<std::string, int> goodbye;
  clog::signal<> trigger;
  
  auto emit_hello() -> void
  {
    hello("world");
  }
  
  auto emit_goodbye() -> void
  {
    goodbye("planet", 5);
  }
  
  auto something_else() -> void
  {
    trigger();
  }
};

struct receiver
{
  // If these connections are active when receiver is
  // destructed then the slots will be automatically
  // disconnected. It's ok if the connected signal is
  // destroyed first.
  clog::cn hello_connection;
  clog::cn goodbye_connection;
  clog::cn trigger_connection;
  
  auto attach(emitter* e) -> void
  {
    const auto on_hello = [](std::string message)
    {
      std::cout << "hello " << message << "\n";
    };
    
    const auto on_goodbye = [](std::string message, int n)
    {
      for (int i = 0; i < n; i++)
      {
        std::cout << "goodbye " << message << "\n";
      }
    };
    
    hello_connection = e->hello >> on_hello;
    goodbye_connection = e->goodbye >> on_goodbye;
    
    // You can just write the lambda inline if you want
    trigger_connection = e->trigger >> []()
    {
      std::cout << "trigger\n";
    };
  }
};

// An alternative way of storing the connections,
// using clog::store and the += operator
struct receiver2
{
  clog::store cns;
  
  auto attach(emitter* e) -> void
  {
    const auto on_hello = [](std::string message) { ... };
    const auto on_goodbye = [](std::string message, int n) { ... };
    const auto on_trigger = []() { ... };
    
    cns = {}; // clear any existing connections
    cns += e->hello >> on_hello;
    cns += e->goodbye >> on_goodbye;
    cns += e->trigger >> on_trigger;
  }
};

```

## property.hpp
[include/clog/property.hpp](include/clog/property.hpp)

It's a library on top of [signal.hpp](#signalhpp) which lets you do this kind of thing:

```c++
struct animal
{
  // a getter and setter. Any time the value changes a signal is emitted.
  clog::property<string> name{"unnamed");
 
  // this one has no interface for setting the value, only for getting it.
  // to set the value you need to create a clog::setter for it.
  clog::readonly_property<int> age;
  
  // instead of storing a value, this one stores a function for retrieving
  // the value.
  clog::proxy_property<string> description;
  
private:
  
  // a private setter for setting the value of the age property.
  clog::setter<int> age_setter_ { &age };

  clog::store cns_;
  
public:

  animal()
    : age{inital_age}
  {
    // You can pass a function into the proxy_property constructor to
    // initialize it if you want but you can also set it using the =
    // operator like this:
    description = []()
    {
      stringstream ss;
      ss << "An animal named " << *name << ". It is " << to_string(*age) << " years old";
      return ss.str();
    };
    
    const auto on_age_changed = [=](int new_age)
    {
      cout << "age was updated to " << new_age << "\n";
      
      // This simply emits a signal<> which will be received by
      // anything connecting to the 'description' property.
      //
      // The description function defined above won't be called
      // until someone reads the property
      description.notify();
    };
    
    const auto on_name_changed = [=](auto&&...)
    {
      // We don't actually use the new name value here but the
      // signal requires slots to receive the new value as a
      // function argument.
      
      // Sometimes it is nice to be able to call this lambda
      // directly without having to pass in the argument so
      // to support that you can use this auto&&... shit
      
      description.notify();
    };
    
    cns_ += age >> on_age_changed;
    cns_ += name >> on_name_changed;
    
    on_age_changed(age);
    on_name_changed();
    
    cout << "constructed an animal. Here is a description of it:\n";
    cout << *description << "\n";
  }
  
  auto grow() -> void
  {
    // Signal will be emitted from the 'age' property
    age_setter_ = *age + 1;
  }
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
```c++
// There is some syntax sugar that lets you write things like this instead
struct alternative
{
  enum class task
  {
    some_task,
    another_task,
  };
  
  clog::idle_task_pusher tasks { processor.make_pusher() };
  
  alternative()
  {
    // Pre-define the tasks for each id
    tasks[task::some_task] = [=]()
    {
      // Do something
    }
    
    tasks[task::another_task] = [=]()
    {
      // Do something else
    }
  }
  
  auto foobar() -> void
  {
    // Push the tasks
    tasks << task::some_task;
    tasks << task::another_task;
  }
};

```
