#pragma once

#include <memory>
#include <ranges>

namespace detail
{
    template<typename BaseType, std::size_t InplaceDerivedSizeLimit = 48>
    class small_polymorphic_object
    {
    public:
        using base_type = BaseType;
        constexpr static std::size_t size_limit = InplaceDerivedSizeLimit;

        static_assert(sizeof(BaseType) <= size_limit);

        small_polymorphic_object() = default;
        small_polymorphic_object(const small_polymorphic_object&) = delete;
        small_polymorphic_object& operator=(const small_polymorphic_object&) = delete;

        small_polymorphic_object(small_polymorphic_object&& other) noexcept
        {
            if (std::holds_alternative<ptr_type>(other.m_obj))
            {
                m_obj = std::move(std::get<ptr_type>(m_obj));
            }
            if (std::holds_alternative<buf_type>(other.m_obj))
            {
                m_obj = buf_type{};
                other.m_move_ctor(std::get<buf_type>(m_obj).data(), std::get<buf_type>(other.m_obj).data());
                m_move_ctor = other.m_move_ctor;
            }
        }

        small_polymorphic_object& operator=(small_polymorphic_object&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }
            if (std::holds_alternative<ptr_type>(other.m_obj))
            {
                m_obj = std::move(std::get<ptr_type>(m_obj));
            }
            if (std::holds_alternative<buf_type>(other.m_obj))
            {
                m_obj = buf_type{};
                other.m_move_ctor(std::get<buf_type>(m_obj).data(), std::get<buf_type>(other.m_obj).data());
                m_move_ctor = other.m_move_ctor;
            }
            return *this;
        }

        ~small_polymorphic_object()
        {
            if (std::holds_alternative<buf_type>(m_obj))
            {
                std::destroy_at(std::bit_cast<base_type*>(std::get<buf_type>(m_obj).data()));
            }
        }

        template<typename Derived, typename... DerivedArgTs>
        void set(DerivedArgTs&&... args)
        {
            static_assert(std::is_move_assignable_v<Derived>);

            if constexpr (sizeof(Derived) <= size_limit)
            {
                m_obj = buf_type{};
                std::construct_at(std::bit_cast<Derived*>(std::get<buf_type>(m_obj).data()),
                                  std::forward<DerivedArgTs>(args)...);
                m_move_ctor = [] (void* dst_void, void* src_void)
                {
                    std::construct_at(static_cast<Derived*>(dst_void), std::move(*static_cast<Derived*>(src_void)));
                };
            }
            else
            {
                m_obj = std::make_unique<Derived>(std::forward<DerivedArgTs>(args)...);
            }
        }

        [[nodiscard]] base_type& get()
        {
            return const_cast<base_type&>(static_cast<const small_polymorphic_object*>(this)->get());
        }

        [[nodiscard]] const base_type& get() const
        {
            if (std::holds_alternative<ptr_type>(m_obj))
            {
                return *std::get<ptr_type>(m_obj);
            }
            if (std::holds_alternative<buf_type>(m_obj))
            {
                return *std::bit_cast<BaseType*>(std::get<buf_type>(m_obj).data());
            }
            throw std::runtime_error("small_polymorphic_object::get() called without holding value");
        }

        [[nodiscard]] base_type& operator*() { return get(); }
        [[nodiscard]] const base_type& operator*() const { return get(); }
        [[nodiscard]] base_type* operator->() { return &get(); }
        [[nodiscard]] const base_type* operator->() const { return &get(); }

    private:
        using ptr_type = std::unique_ptr<base_type>;
        using buf_type = std::array<std::byte, size_limit>;

        std::variant<std::monostate, ptr_type, buf_type> m_obj;
        void (*m_move_ctor)(void* dst, void* src) = nullptr;
    };

    class base_polymorphic_view
    {
    public:
        base_polymorphic_view() = default;
        virtual ~base_polymorphic_view() = default;

        class base_iterator
        {
        public:
            base_iterator() = default;
            virtual ~base_iterator() = default;

            [[nodiscard]] virtual small_polymorphic_object<base_iterator> duplicate() = 0;

            virtual base_iterator& operator++() = 0;
            virtual base_iterator& operator--() = 0;

            [[nodiscard]] virtual void* get_value_pointer() = 0;

            [[nodiscard]] virtual bool operator==(const base_iterator& other) const = 0;
        };

        [[nodiscard]] virtual small_polymorphic_object<base_iterator> init_begin_iterator() = 0;
        [[nodiscard]] virtual small_polymorphic_object<base_iterator> init_end_iterator() = 0;
    };

    template<typename ViewT>
    class derived_polymorphic_view final : public base_polymorphic_view
    {
    public:
        using view_type = ViewT;
        using view_iterator_type = std::ranges::iterator_t<view_type>;

        explicit derived_polymorphic_view(const ViewT& view) : m_view(view) {}

        derived_polymorphic_view() = delete;
        derived_polymorphic_view(const derived_polymorphic_view&) = default;
        derived_polymorphic_view& operator=(const derived_polymorphic_view&) = default;
        derived_polymorphic_view(derived_polymorphic_view&&) noexcept = default;
        derived_polymorphic_view& operator=(derived_polymorphic_view&&) noexcept = default;
        ~derived_polymorphic_view() override = default;

        class derived_iterator final : public base_iterator
        {
        public:
            explicit derived_iterator(const view_iterator_type& iter)
                : m_iterator(iter)
            {}

            ~derived_iterator() override = default;

            [[nodiscard]] small_polymorphic_object<base_iterator> duplicate() override
            {
                small_polymorphic_object<base_iterator> result;
                result.set<derived_iterator>(m_iterator);
                return result;
            }

            base_iterator& operator++() override
            {
                ++m_iterator;
                return *this;
            }

            base_iterator& operator--() override
            {
                if constexpr (requires { --m_iterator; })
                {
                    --m_iterator;
                }
                else
                {
                    throw std::runtime_error("operator-- not supported on view");
                }
                return *this;
            }

            [[nodiscard]] void* get_value_pointer() override
            {
                return const_cast<void*>(static_cast<const void*>(&(*m_iterator)));
            }

            [[nodiscard]] bool operator==(const base_iterator& other) const override
            {
                return m_iterator == static_cast<const derived_iterator&>(other).m_iterator;
            }

        private:
            view_iterator_type m_iterator;
        };

        [[nodiscard]] small_polymorphic_object<base_iterator> init_begin_iterator() override
        {
            small_polymorphic_object<base_iterator> result;
            result.set<derived_iterator>(std::begin(m_view));
            return result;
        }

        [[nodiscard]] small_polymorphic_object<base_iterator> init_end_iterator() override
        {
            small_polymorphic_object<base_iterator> result;
            result.set<derived_iterator>(std::end(m_view));
            return result;
        }

    private:
        view_type m_view;
    };
}

template<typename T>
class opaque_view
{
public:
    template<typename ViewT>
    opaque_view(const ViewT& view)
    {
        m_polymorphic_view.set<detail::derived_polymorphic_view<ViewT>>(view);
    }

    opaque_view() = delete;
    opaque_view(const opaque_view&) = delete;
    opaque_view& operator=(const opaque_view&) = delete;
    opaque_view(opaque_view&&) noexcept = default;
    opaque_view& operator=(opaque_view&&) noexcept = default;
    ~opaque_view() = default;

    template<bool Const>
    class iterator
    {
    public:
        using value_type = std::conditional_t<Const, const T, T>;
        using reference = value_type&;
        using pointer = value_type*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;

        iterator() = delete;

        explicit iterator(opaque_view& view)
            : m_polymorphic_iterator(view.m_polymorphic_view->init_begin_iterator())
        {}

        explicit iterator(detail::small_polymorphic_object<detail::base_polymorphic_view::base_iterator> polymorphic_iter)
            : m_polymorphic_iterator(std::move(polymorphic_iter))
        {}

        iterator(const iterator& other)
            : m_polymorphic_iterator(other->m_polymorphic_iterator->duplicate())
        {}

        iterator& operator=(const iterator& other)
        {
            if (this == &other)
            {
                return *this;
            }
            m_polymorphic_iterator = other->m_polymorphic_iterator->duplicate();
            return *this;
        }

        iterator(iterator&& other) noexcept
            : m_polymorphic_iterator(std::move(other->m_polymorphic_iterator))
        {}

        iterator& operator=(iterator&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }
            m_polymorphic_iterator = std::move(other->m_polymorphic_iterator);
            return *this;
        }

        ~iterator() = default;

        reference operator*() noexcept
        {
            return *static_cast<pointer>(m_polymorphic_iterator->get_value_pointer());
        }

        pointer operator->() noexcept
        {
            return static_cast<pointer>(m_polymorphic_iterator->get_value_pointer());
        }

        iterator& operator++() noexcept
        {
            m_polymorphic_iterator->operator++();
            return *this;
        }

        iterator operator++(int) noexcept
        {
            iterator tmp = *this;
            m_polymorphic_iterator->operator++();
            return tmp;
        }

        iterator& operator--() noexcept
        {
            m_polymorphic_iterator->operator--();
            return *this;
        }

        iterator operator--(int) noexcept
        {
            iterator tmp = *this;
            m_polymorphic_iterator->operator--();
            return tmp;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept
        {
            return *lhs.m_polymorphic_iterator == *rhs.m_polymorphic_iterator;
        }

    private:
        detail::small_polymorphic_object<detail::base_polymorphic_view::base_iterator> m_polymorphic_iterator;
    };

    iterator<false> begin()
    {
        return iterator<false>(m_polymorphic_view->init_begin_iterator());
    }

    iterator<true> begin() const
    {
        return iterator<true>(const_cast<opaque_view*>(this)->m_polymorphic_view->init_begin_iterator());
    }

    iterator<false> end()
    {
        return iterator<false>(m_polymorphic_view->init_end_iterator());
    }

    iterator<true> end() const
    {
        return iterator<true>(const_cast<opaque_view*>(this)->m_polymorphic_view->init_end_iterator());
    }

private:
    detail::small_polymorphic_object<detail::base_polymorphic_view> m_polymorphic_view;
};