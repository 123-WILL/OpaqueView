# opaque_view

## Overview

Ranges and views were introduced in C++20 to represent iterable collections of items that have ``.begin()`` and ``.end()`` defined on them. I don't know all the nuances in the terminology around ranges and views (there's a lot) so I may misuse a few terms. Also, this is a proof-of-concept implementation that will almost certainly misbehave under some situations that I didn't think about.

The purpose of ``opaque_view<T>`` is to wrap the result of applying range adaptor objects (such as ``std::views::filter`` or ``std::views::transform``) to ranges (such as ``std::vector`` or ``std::map``) in a way that hides the implementation and storage details of the range, so the only information stored in the type itself is the type of element in the range.

If you have a ``std::unordered_map<int, int>`` and want to print all the values in it, ranges and range adaptors provide an elegant solution with the following code:
```cpp
std::unordered_map<int, int> map;
for (int value : map | std::views::values)
{
    std::cout << value << ' ';
}
```
This works great! It feels a bit like magic, as long as we don't have to think about or handle the type that ``map | std::views::values`` returns. Incidentally, this type is: ``std::ranges::elements_view<std::ranges::ref_view<std::unordered_map<int, int>>, 1>``, which removes some of the magic from the situation.

I was recently writing a class with a member function that should return a view onto elements that satisfy some predicate from a range of objects of type ``ValueT`` owned by the object. If we assume we are storing the objects in a ``std::vector<ValueT>``, we could do something like this:

```cpp
class Example  
{  
public:  
    // deduced return type: 
    // std::ranges::filter_view<
    //     std::ranges::ref_view<std::vector<ValueT>>, 
    //     <lambda auto(const auto & value)>>
    auto GetMatchingValues()  
    {  
        return m_values | std::views::filter([](const auto& value){...});  
    }  
    
private:  
    std::vector<ValueT> m_values;  
};
```

This works fine, but has a few limitations. For one, ``GetMatchingValues`` must have its definition at the point of declaration since we can't explicitly specify the type of the lambda (which is part of the function return type). This could slow down compilation times because it prevents separating the function declaration in a header file from the definition in a source file.

The next issue is that I want the freedom to change the implementation details of the internal storage of the values (e.g. from ``std::vector<ValueT>`` to the mapped type in ``std::map<std::size_t, ValueT>``) without changing the class's API or ABI. However, all the details of the underlying range and all range adaptors are part of the return value's static type, so any changes to the class implementation would require all clients to be recompiled (with possible code changes) to keep up with the change, even without any visible behavior difference in the function.

``opaque_view<T>`` erases all information of the underlying range and range adaptors from the static type of the object. With this as the return type, the only contract in your API is that you will provide an unowning view onto some objects of type ``T``. This also frees you to implement the function in a separate compilation unit and change the implementation at will. The modified code is a very simple change:

```cpp
#include "opaque_view.h"

class Example  
{  
public:
    opaque_view<ValueT> GetMatchingValues()  
    {  
        return m_values | std::views::filter([] (const auto& value) { ... });  
    }  
    
private:  
    std::vector<ValueT> m_values;  
};
```

## Implementation Details

This section in progress. The implementation involves a combination of type erasure and runtime polymorphism, and a small object optimization that avoids heap allocations as long as the size of the wrapped view/view iterator is less than some limit. By default I set this to 64-bytes (a common cache line size) which fit most of the reasonable range adaptor combinations that I tried.

It's also not much code if you just want to read ``opaque_view.h``. 