#include <iostream>
#include <unordered_map>
#include <ranges>
#include <vector>
#include <chrono>
#include "opaque_view.h"

class ValueProducer
{
public:
    ValueProducer()
    {
        for (int i = 0; i < 1024; ++i)
        {
            m_map.emplace(i, i);
        }
    }
    // Implementation/storage details are part of API/ABI because return type is:
    //   std::ranges::filter_view<std::ranges::elements_view<std::ranges::ref_view<
    //     std::unordered_map<int, int>>, 1>, <lambda bool(int n)>>
    auto GetValuesSTL()
    {
        return m_map | std::views::values | std::views::filter([] (int n) { return n % 2 == 0; });
    }
    // Implementation/storage details are hidden from API/ABI, and the
    // function definition can be in another compilation unit.
    opaque_view<int> GetValuesOpaque()
    {
        return m_map | std::views::values | std::views::filter([] (int n) { return n % 2 == 0; });
    }
private:
    std::unordered_map<int, int> m_map;
};

inline void do_not_optimize(int& value)
{
    asm volatile("" : "+r"(value) : : "memory");
}

int main()
{
    ValueProducer prod;

    auto t0 = std::chrono::steady_clock::now();
    for (auto& val : prod.GetValuesSTL())
    {
        do_not_optimize(val);
    }
    auto t1 = std::chrono::steady_clock::now();
    for (auto& val : prod.GetValuesOpaque())
    {
        do_not_optimize(val);
    }
    auto t2 = std::chrono::steady_clock::now();

    std::cout << "\nstl view time: " << std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0)
              << "\nopaque_view time: " << std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1)
              << "\nopaque time multiplier: " << static_cast<float>((t2 - t1).count()) / static_cast<float>((t1 - t0).count());

    return 0;
}
