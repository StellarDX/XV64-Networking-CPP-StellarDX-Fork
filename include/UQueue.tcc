#ifndef UQUEUE_TCC
#define UQUEUE_TCC

#include "UDef.hh"

_EXTERN_C
_ADD_KALLOC
_ADD_KFREE
_END_EXTERN_C

template<typename Tp>
class LinkedQueue
{
public:
    using value_type      = Tp;
    using pointer         = Tp*;
    using const_pointer   = const Tp*;
    using reference       = Tp&;
    using const_reference = const Tp&;
    using size_type       = size_t;

    struct container
    {
        value_type Data;
        container* Next = nullptr;
    };

private:
    container* MyFirst = nullptr;
    container* MyLast = nullptr;

public:
    LinkedQueue() {Init();}
    ~LinkedQueue()
    {
        while(!empty()) {pop();}
    }

    void Init()
    {
        MyFirst = nullptr;
        MyLast = MyFirst;
    }

    [[__nodiscard__]] bool empty() const
    {
        return !MyFirst && !MyLast;
    }

    [[__nodiscard__]] size_type size() const
    {
        size_type Size = 0;
        container* First = MyFirst;
        while (First)
        {
            ++Size;
            First = First->Next;
        }
        return Size;
    }

    [[__nodiscard__]] reference front()
    {
        return MyFirst->Data;
    }

    [[__nodiscard__]] const_reference front() const
    {
        return MyFirst->Data;
    }

    [[__nodiscard__]] reference back()
    {
        return MyLast->Data;
    }

    [[__nodiscard__]] const_reference back() const
    {
        return MyLast->Data;
    }

    void push(const value_type& x)
    {
        container* NewContainer = new container();
        NewContainer->Data = x;
        if (!MyLast)
        {
            MyFirst = NewContainer;
            MyLast = MyFirst;
        }
        else
        {
            MyLast->Next = NewContainer;
            MyLast = MyLast->Next;
        }
    }

    void pop()
    {
        container* First = MyFirst->Next;
        delete MyFirst;
        MyFirst = First;
        MyLast = First ? MyLast : nullptr;
    }

    void clear() {*this = LinkedQueue();}
};

#endif // UQUEUE_TCC
