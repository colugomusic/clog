# clog

spaghetti code spider web object oriented hell scape. all software is garbage and so are you

- [expire.hpp](include/clog/expire.hpp) - dying object notifications, not documented
- [idle.hpp](#idlehpp) - object task queues
- [property.hpp](include/clog/property.hpp) - set/get property library, not documented
- [rcv.hpp](include/clog/rcv.hpp) - reusable cell vector, some documentation in header
- [signal.hpp](include/clog/signal.hpp) - single-threaded signal/slot library, not documented
- [vectors.hpp](#vectorshpp) - operations for manipulating sorted vectors

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

## vectors.hpp
[include/clog/vectors.hpp](include/clog/vectors.hpp)

Operations for manipulating sorted vectors.

---
`vectors::sorted::`<br/>
`contains(const std::vector<T>& vector, T value)` ` -> bool`

Precondition: The vector must be sorted.

Returns: true if the value is in the vector.

---
`vectors::sorted::`<br/>
`insert(std::vector<T>* vector, T value)` ` -> std::pair<typename std::vector<T>::iterator, bool>`

Precondition: The vector must be sorted.

Inserts the value into the sorted vector.

Returns: `std::pair<(iterator to inserted item), (true if insertion was successful)>`

---
`vectors::sorted::`<br/>
`erase_all(std::vector<T>* vector, T value)` ` -> typename std::vector<T>::size_type`

Precondition: The vector must be sorted.

Removes all instances of the value from the sorted vector.

Returns: the number of items removed.

---
`vectors::sorted::unique::`<br/>
`insert(std::vector<T>* vector, T value)` ` -> std::pair<typename std::vector<T>::iterator, bool>`

Precondition: The vector must be sorted.

Inserts the value into the sorted vector.

Fails if the value is already in the vector.

Returns: `std::pair<(iterator to inserted item), (true if insertion was successful)>`

---
`vectors::sorted::unique::checked::`<br/>
`insert(std::vector<T>* vector, T value)` ` -> std::pair<typename std::vector<T>::iterator, bool>`

Precondition: The vector must be sorted.

Asserts that the value does not already exist in the vector.

Inserts the value into the sorted vector.

---
`vectors::sorted::unique::checked::`<br/>
`erase(std::vector<T>* vector, T value)` ` -> void`

Precondition: The vector must be sorted.

Removes the value from the sorted vector.

Asserts that exactly one element was removed.
