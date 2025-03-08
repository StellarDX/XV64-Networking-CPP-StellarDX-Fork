// Simple array list

#ifndef UARRAYLIST_HH
#define UARRAYLIST_HH

#include "UDef.hh"

template<typename Tp, size_t MaxNm>
struct ArrayList
{
    using value_type      = Tp;
    using pointer         = value_type*;
    using const_pointer   = const pointer;
    using reference       = value_type&;
    using const_reference = const value_type&;
    using iterator        = pointer;
    using const_iterator  = const_pointer;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;

private:
    Tp Elems[MaxNm];
    size_t CurrentSize = 0;

public:
    [[__nodiscard__, __gnu__::__const__, __gnu__::__always_inline__]]
    constexpr pointer data() noexcept
    {
        return static_cast<pointer>(Elems);
    }

    [[__nodiscard__]]
    constexpr const_pointer data() const noexcept
    {
        return static_cast<const_pointer>(Elems);
    }

    constexpr void fill(const value_type& u)
    {
        for (int i = 0; i < CurrentSize; ++i)
        {
            data()[i] = u;
        }
    }

    // Iterators
    [[__gnu__::__const__, __nodiscard__]]
    constexpr iterator begin() noexcept
    {
        return iterator(data());
    }

    [[__nodiscard__]]
    constexpr const_iterator begin() const noexcept
    {
        return const_iterator(data());
    }

    [[__gnu__::__const__, __nodiscard__]]
    constexpr iterator end() noexcept
    {
        return iterator(data() + CurrentSize);
    }

    [[__nodiscard__]]
    constexpr const_iterator end() const noexcept
    {
        return const_iterator(data() + CurrentSize);
    }

    [[__nodiscard__]]
    constexpr const_iterator cbegin() const noexcept
    {
        return const_iterator(data());
    }

    [[__nodiscard__]]
    constexpr const_iterator cend() const noexcept
    {
        return const_iterator(data() + CurrentSize);
    }

    // Capacity.
    [[__nodiscard__, __gnu__::__const__, __gnu__::__always_inline__]]
    constexpr size_type size() const noexcept { return CurrentSize; }

    [[__nodiscard__, __gnu__::__const__, __gnu__::__always_inline__]]
    constexpr size_type max_size() const noexcept { return MaxNm; }

    constexpr void resize(size_type NewSize)
    {
        if (CurrentSize > NewSize)
        {
            for (size_type i = NewSize; i < CurrentSize; ++i)
            {
                Elems[i] = value_type();
            }
        }
        CurrentSize = NewSize;
    }

    [[__nodiscard__, __gnu__::__const__, __gnu__::__always_inline__]]
    constexpr bool empty() const noexcept { return size() == 0; }

    // Element access.
    [[__nodiscard__]]
    constexpr reference operator[](size_type n) noexcept
    {
        return Elems[n];
    }

    [[__nodiscard__]]
    constexpr const_reference operator[](size_type n) const noexcept
    {
        return Elems[n];
    }

    constexpr reference at(size_type n)
    {
        return Elems[n];
    }

    constexpr const_reference at(size_type n) const
    {
        return Elems[n];
    }

    [[__nodiscard__]]
    constexpr reference front() noexcept
    {
        return Elems[(size_type)0];
    }

    [[__nodiscard__]]
    constexpr const_reference front() const noexcept
    {
        return Elems[(size_type)0];
    }

    [[__nodiscard__]]
    constexpr reference back() noexcept
    {
        return Elems[CurrentSize - 1];
    }

    [[__nodiscard__]]
    constexpr const_reference back() const noexcept
    {
        return Elems[CurrentSize - 1];
    }

    constexpr void push_back(const value_type& x)
    {
        Elems[CurrentSize] = x;
        ++CurrentSize;
    }

    constexpr void pop_back() noexcept
    {
        --CurrentSize;
        Elems[CurrentSize] = value_type();
    }

    constexpr iterator insert(iterator position, const value_type& x)
    {
        for (auto it = end() - 1; it != position - 1; --it)
        {
            *(it + 1) = *(it);
        }
        *position = x;
        ++CurrentSize;
    }

    constexpr iterator erase(iterator position)
    {
        --CurrentSize;
        for (; position != end(); ++position)
        {
            *position = *(position + 1);
        }
        Elems[CurrentSize] = value_type();
        return position;
    }

    constexpr iterator erase(const_iterator first, const_iterator last)
    {
        size_type gap = last - first;
        for (int i = first; i < CurrentSize; ++i)
        {
            Elems[i] = Elems[i + gap];
        }
        size_type NewSize = CurrentSize - gap;
        for (int i = NewSize; i < CurrentSize; ++i)
        {
            Elems[i] = value_type();
        }
        CurrentSize = NewSize;
    }

    constexpr void clear() noexcept { *this = ArrayList(); }
};

template<typename Iter, typename Pred>
Iter find_if(Iter First, Iter Last, Pred Pr)
{
    for (; First != Last; ++First)
    {
        if (Pr(*First)) {break;}
    }
    return First;
}

template<typename Iter, typename Pred>
Iter find_if_not(Iter First, Iter Last, Pred Pr)
{
    for (; First != Last; ++First)
    {
        if (!Pr(*First)) {break;}
    }
    return First;
}

template<typename Iter, typename Pred>
Iter partition(Iter First, Iter Last, Pred Pr)
{
    First = find_if_not(First, Last, Pr);
    if (First == Last) {return First;}
    for (auto i = ++First; i != Last; ++i)
    {
        if (Pr(*i))
        {
            auto Tmp = *i;
            *i = *First;
            *First = Tmp;
            ++First;
        }
    }
    return First;
}

template<typename Iter, typename Pred>
void qsort(Iter First, Iter Last, Pred Pr)
{
    if (First == Last) {return;}
    auto Pivot = *(First + (Last - First) / 2);
    Iter Mid1 = partition(First, Last, [&Pr, Pivot](const auto& Em){return Pr(Em, Pivot);});
    Iter Mid2 = partition(Mid1, Last, [&Pr, Pivot](const auto& Em){return !Pr(Pivot, Em);});
    qsort(First, Mid1, Pr);
    qsort(Mid2, Last, Pr);
}

#endif // UARRAYLIST_HH
