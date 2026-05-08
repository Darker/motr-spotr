#pragma once
#include <stdint.h>

#include <stddef.h>

template <typename TValue, size_t VSize>
struct SimpleQueue
{
    size_t start = 0;
    size_t end   = 0;

    TValue values[VSize];

    size_t size() const
    {
        return (end + VSize - start) % VSize;
    }

    bool isFull() const
    {
        return size() >= VSize - 1;
    }

    bool isEmpty() const
    {
        return start == end;
    }

    void pushNextFn(void (*writer)(TValue*))
    {
        moveIndices();
        writer(&values[end]);
    }

    TValue* pushNext()
    {
        moveIndices();
        return &values[end];
    }

    bool pushNextNoOverwrite(void (*writer)(TValue*))
    {
        if (isFull())
        {
            return false;
        }
        moveIndices();
        writer(&values[end]);
        return true;
    }

    void popOldest()
    {
        if (!isEmpty())
        {
            start = (start + 1) % VSize;
        }
    }

    void clear()
    {
        start = 0;
        end   = 0;
    }

    TValue& head()
    {
        return values[end];
    }

    const TValue& head() const
    {
        return values[end];
    }

    TValue& at(size_t idx)
    {
        return values[(start + idx) % VSize];
    }

    const TValue& at(size_t idx) const
    {
        return values[(start + idx) % VSize];
    }

private:
    void moveIndices()
    {
        end = (end + 1) % VSize;

        if (end == start)
        {
            start = (start + 1) % VSize;
        }
    }
};
