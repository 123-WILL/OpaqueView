#include <iostream>
#include <unordered_map>
#include <ranges>
#include "opaque_view.h"

class ValueProducer
{
public:
    ValueProducer()
    {
        for (int i = 0; i < 16; ++i)
        {
            m_map.emplace(i, i);
        }
    }
    // Implementation/storage details hidden from API/ABI
    // and definition can be in another compilation unit.
    opaque_view<int> GetValues()
    {
        return m_map | std::views::values;
    }
private:
    std::unordered_map<int, int> m_map;
};

int main()
{
    ValueProducer prod;
    for (auto& val : prod.GetValues())
    {
        std::cout << val << ' ';
    }
    return 0;
}
