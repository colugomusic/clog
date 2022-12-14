# clog

Header-only libraries.

## Requirements
- c++17

## Libraries
1. [vectors.hpp](include/clog/vectors.hpp) - operations for manipulating sorted vectors, documentation in header
2. [rcv.hpp](#rcvhpp) - reusable cell vector
3. [signal.hpp](#signalhpp) - single-threaded signal/slot library
4. [property.hpp](#propertyhpp) - set/get property library
5. [expire.hpp](#expirehpp) - dying object notifications
6. [small_function.hpp](include/clog/small_function.hpp) - like `std::function` except it can never allocate heap memory. not documented
7. [item_processor.hpp](include/clog/item_processor.hpp) - push items to be processed later, from the main thread or a worker thread or a realtime processing thread. not documented
8. [cache.hpp](#cachehpp) - a single cached value
9. [tree.hpp](#treehpp) - an acyclic, unbalanced, ordered tree

## rcv.hpp
[include/clog/rcv.hpp](include/clog/rcv.hpp)

Requires: [vectors.hpp](include/clog/vectors.hpp)

Reusable Cell Vector

It's a vector of T which can only grow. Elements are constructed in-place using `acquire(/*constructor args*/)` and you get back a handle which you can use to access the element. The handle is just an index into the array, but it will never become invalidated until you call `release(handle)`. Even if you erase elements from the middle of the vector, the handle will still be valid, because the logical positions of the existing elements doesn't change.

The public interface is:
```c++
template <typename T, typename ResizeStrategy = rcv_default_resize_strategy>
class rcv
{
public:
	using handle_t = rcv_handle;
	rcv();
	rcv(const rcv& rhs);
	rcv(rcv&& rhs);
	auto active_handles() const -> std::vector<handle_t>;
	auto capacity() const -> size_t;
	auto reserve(size_t size) -> void;
	auto size() const -> size_t;
	template <typename... ConstructorArgs>
	auto acquire(ConstructorArgs... constructor_args) -> handle_t;
	auto release(handle_t index) -> void;
	auto get(handle_t index) -> T*;
};
```

- T must be copy or move constructible.
- You can't choose where in the vector new elements are inserted.
- Elements may become fragmented, but only within a single contiguous block of memory.
- A cell is empty until a call to `acquire()` constructs an element there.

When iterating over the elements their order is not guaranteed, e.g.

```c++
a = v.acquire();
b = v.acquire();

for (auto handle : v.active_handles())
{
    // b might be visited before a
}
```
Calling `acquire()` while iterating like this is ok (only the cells returned by `active_handles()` will be visited.) But calling `release()` while visiting is not ok so you should defer releasing until after you finish visiting somehow ([signal.hpp](include/clog/signal.hpp) does this.)

Adding or removing elements from the vector doesn't invalidate indices. Everything "logically" stays where it is in the vector, e.g. an element at index 3 will always be at index 3 even if more storage needs to be allocated. If the vector has to grow then the objects may be copied. If `is_nothrow_move_constructible<T>` then they will be moved instead.

Erasing an element from the middle also doesn't invalidate the handles of the other elements. A slot just opens up at the released position.

`acquire(/*constructor arguments*/)` constructs an element in-place and returns a handle to access it. Retrieve it using `get(handle)`.

```c++
rsv<thing> items;
rsv_handle item = items.acquire(/*constructor arguments*/);

// get() just returns a pointer to the object. do
// whatever you want with it
items.get(item)->bar();
*items.get(item) = thing{};
thing* ptr = items.get(item);
ptr->bar();
```

`release()` destroys the element at the given index (handle) and opens up the cell it was occupying. Calling `get()` with the handle that was just released is invalid.

Note that the memory of the released cell is not freed, but the destructor will be run so there will be no object there anymore. If `acquire()` is called later, the new element might be constructed at that newly opened cell, and the handle pointing to that index would become valid again.

### `clg::unsafe_rcv<T>`

There is another class in `clg::` named `unsafe_rcv`. The only difference between `rcv` and `unsafe_rcv` is that `rcv::get()` will check that the given handle is valid and return `nullptr` if not. The expense of doing this is a binary search over a sorted vector of known valid handles. In cases where you know your handles are always valid then you might as well use `unsafe_rcv`.

## signal.hpp
[include/clog/signal.hpp](include/clog/signal.hpp)

Requires: [rcv.hpp](#rcvhpp), [vectors.hpp](include/clog/vectors.hpp)

It's a single-threaded signal/slot libary. I wrote this because `boost::signals2` proved from profiling to be a bottleneck in my project and I couldn't find any other library that I liked.

Advantages of this library over `boost::signals2`:
- It's faster. I haven't benchmarked anything but I improved a bunch of visible lag in my program simply by switching everything over to this library. (Yes i know you can use a dummy mutex with boost if you don't need multithreading support. It's still much slower than this library even if you do that.)
- The code isn't a stupid mess so it's much easier to follow the control flow through connections in a debugger.
- It compiles faster

Disadvantages:
- Not thread safe
- Written by a moron

Some quirks of this library:
- Signals and connections are moveable but not copyable
- Connections are always scoped, i.e. you get a connection object which automatically disconnects the slot when it goes out of scope. You can't simply set and forget a connection. Connection methods are marked with `[nodiscard]]` so you have to do something with the result. This may be annoying in cases where you know the signal won't outlive the slot but in my opinion it is worth it to be less error prone.
- There's no `disconnect` method. Disconnects happen automatically when the `clg::cn` goes out of scope. If you want to explicitly disconnect you can just do `connection = {};`
- Connecting more slots while the signal is emitting is supported.
- Disconnecting a slot while the signal is emitting is supported, but is not optimal. (The signal will take a temporary copy of all its current connections to work around a corner case where deleting the function object would cause the signal itself to be deleted.)
- Slots will not necessarily be visited in the same order that they were connected.

```c++
struct emitter
{
  clg::signal<std::string> hello;
  clg::signal<std::string, int> goodbye;
  clg::signal<> trigger;
  
  auto do_things() -> void
  {
    hello("world");
    goodbye("planet", 5);
    trigger();
  }
};

struct receiver
{
  // If these connections are active when receiver is
  // destructed then the slots will be automatically
  // disconnected. It's ok if the connected signals
  // are destroyed first.
  clg::cn hello_connection;
  clg::cn goodbye_connection;
  clg::cn trigger_connection;
  
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
// using clg::store and the += operator
struct receiver2
{
  clg::store cns;
  
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

Requires: [signal.hpp](#signalhpp), [rcv.hpp](#rcvhpp), [vectors.hpp](include/clog/vectors.hpp)

It's a library on top of [signal.hpp](#signalhpp) which lets you do this kind of thing:

```c++
struct animal
{
  // a getter and setter. Any time the value changes a signal is emitted.
  clg::property<string> name{"unnamed");
 
  // this one has no interface for setting the value, only for getting it.
  // to set the value you need to create a clg::setter for it.
  clg::readonly_property<int> age;
  
  // instead of storing a value, this one stores a function for retrieving
  // the value.
  clg::proxy_property<string> description;
  
private:
  
  // a private setter for setting the value of the age property.
  clg::setter<int> age_setter_ { &age };

  clg::store cns_;
  
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

...

animal a;
clg::store cns;

const auto on_desc_changed = [](function<string()> get)
{
  cout << "animal description: " << get() << "\n";
};

const auto on_name_changed = [](string new_name)
{
  cout << "animal name: " << new_name << "\n";
};

cns += a.description >> on_description_changed;
cns += a.name >> on_name_changed;

a.name = "Harold"; // both those lambdas will be called
```

## expire.hpp
[include/clog/expire.hpp](include/clog/expire.hpp)

Requires: [signal.hpp](#signalhpp), [rcv.hpp](#rcvhpp), [vectors.hpp](include/clog/vectors.hpp)

If you inherit from `clg::expirable` you can call `expire()` on it to emit a one-shot "expiry" signal. Repeated calls to `expire()` won't do anything. The expiry signal is automatically emitted when the object is destructed, if `expire()` was not already called explicitly.

A type inheriting from `clg::attacher` can have expirable objects "attached" to it. When the object expires, it is automatically "detached". Objects can also be detached manually.

When an object is attached, `update(clg::attach<T>)` is called. When an object is detached, `update(clg::detach<T>)` is called.

T doesn't need to be a pointer type, but `std::hash<T>` needs to be defined.

```c++
struct animal : public clg::expirable
{
  ...
};

struct house : public clg::attacher<house>
{
  unordered_set<animal*> animals;
  unordered_map<ID, clg::store> animal_connections;
  
  // Called when an animal is attached
  auto update(clg::attach<animal*> a) -> void
  {
    animals.insert(a);
    
    const auto on_age_changed = [a](int new_age) { /* do something */ };
    
    auto& cns = animal_connections[a->id];
    
    cns += a->age >> on_age_changed;
    
    on_age_changed(a->age);
  }
  
  // Will be called when the animal expires
  auto update(clg::detach<animal*> a) -> void
  {
    animals.erase(a);
    animal_connections.erase(a->id);
  }
};

...

house h;
{
  animal a;
  animal b;
  animal c;

  // Attach objects. Calls house::update(clg::attach<animal*>)
  h << &a;
  h << &b;
  h << &c;
  
  // Manually detach an object
  h >> &c;
  
  // house::update(clg::detach<animal*>) will be called twice at the end of this scope (for a and b)
} 
```

## cache.hpp
[include/clog/cache.hpp](include/clog/cache.hpp)

A value, a dirty flag and a function object to update the value. I write a version of this in all my projects so here it is.

```c++
clg::cached<string> text;

text = []()
{
	return "generated text";
};

// The dirty flag is initially set

cout << *text << "\n"; // Lambda is called and the value is cached. Dirty flag is cleared
cout << *text << "\n"; // Lambda is not called
cout << *text << "\n"; // Lambda is not called

text.set_dirty();

cout << *text << "\n"; // Lambda is called and the value is cached. Dirty flag is cleared
cout << *text << "\n"; // Lambda is not called

text = "you can directly set the value too";

cout << *text << "\n"; // Lambda is not called

text.set_dirty();
text = "directly setting the value clears the dirty flag";

cout << *text << "\n"; // Lambda is not called

```

## tree.hpp
[include/clog/tree.hpp](include/clog/tree.hpp)

Requires: [vectors.hpp](include/clog/vectors.hpp)

It's an acyclic, unbalanced, ordered tree.

- The tree always has one root node.
- Siblings are always sorted (`std::less<T>` by default).
- Siblings are stored as contiguous blocks of memory, so it's good if your tree tends to be wider than it is deep.
- Duplicate siblings are not allowed, but duplicate elements are allowed if they don't share a parent.
- Each node makes 1 additional allocation for a control block which allows node handles to remain valid even if the internal vectors need to be resized.
- `T` must be moveable or copyable.

![tree](https://github.com/colugomusic/clog/blob/master/doc/images/tree.png)

The above tree could be produced like this:

```c++
struct Item
{
	std::string text;
	int value; // Used for ordering
	
	auto operator<(const Item& rhs) const -> bool
	{
		return value < rhs.value;
	}
};

clg::tree<Item> tree{Item{"one", 1}};

// Calling add() on the tree object always adds to the root node
tree.add(Item{"two", 2}, Item{"five", 5}, Item{"nine", 9});

// Intermediate nodes are only added if they don't already exist.
// If the leaf node already exists (as determined by the comparison
// function), it is overwritten.
tree.add(Item{"two", 2}, Item{"five", 5}, Item{"ten", 10});
tree.add(Item{"two", 2}, Item{"five", 5}, Item{"TEN", 10}); // Overwrites the leaf node
tree.add(Item{"two", 2}, Item{"six", 6});
tree.add(Item{"three", 3})

// add() returns a handle to the deepest added node
auto four = tree.add(Item{"four", 4});

four->add(Item{"seven", 7}, Item{"eleven", 11});
four->add(Item{"seven", 7}, Item{"twelve", 12});
four->add(Item{"eight", 8});

// To visit every node in the tree, you can pass
// a function that always returns false to the
// search methods.
const auto print = [](const Item& item)
{
	std::cout << item.text << "\n";
	return false;
};

tree.search_breadth_first(print);
std::cout << "\n";

tree.search_depth_first(print);
std::cout << "\n";

// Search for a node.
const auto is_eleven = [](const Item& item)
{
	return item.value == 11;
};

// This returns a handle to the node.
auto eleven = tree.search_depth_first(is_eleven);

// false if the node was not found.
assert (eleven);

// Modifying the value directly. Note that the tree expects
// siblings to be in an ordered state at all times, so
// modifying the "text" member here is ok but modifying the
// "value" member could potentially break the tree.
eleven->get_value().text = "onze";

// set_value() can be used to overwrite the entire value of
// the node. Unlike the above, this is safe because the node
// will always be re-inserted in the correct place.
eleven->set_value(Item{"thirteen", 13});

// The "eleven" handle will remain valid even if more
// siblings are added to the tree.
eleven->get_parent()->add(Item{"fourteen", 14});

// The only thing that invalidates a node handle is removing
// the associated node from the tree.
eleven->get_parent()->remove(eleven);

// The "eleven" handle is now invalid. You can't use it at
// all, not even operator bool.
```
