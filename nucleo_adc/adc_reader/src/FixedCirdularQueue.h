#include <array>
#include <functional>
#include <mutex>

template <typename TValue, size_t VSize>
struct FixedCircularQueue 
{
    constexpr static size_t MAX_SIZE = VSize;

    size_t sizeUnlocked() const
    {
        if (empty)
            return 0;

        if (end >= start)
            return end - start + 1;

        return (VSize - start) + (end + 1);
    }

    bool isFullUnlocked() const
    {
        return sizeUnlocked() >= VSize - 1;
    }

    [[nodiscard]]
    std::unique_lock<std::mutex> waitSize(size_t targetSize, std::atomic<bool>& cancellationToken)
    {   
        std::unique_lock dataLock{dataMutex};
        if(sizeUnlocked() < targetSize)
        {
            newDataCv.wait(dataLock, [this, &cancellationToken, targetSize](){return sizeUnlocked() >= targetSize || !cancellationToken.load();});
        }
        return std::move(dataLock);
    }

    [[nodiscard]]
    std::unique_lock<std::mutex> waitNext(std::atomic<bool>& cancellationToken)
    {   
        std::unique_lock dataLock{dataMutex};
        const size_t oldItemsAdded = itemCounter;
        newDataCv.wait(dataLock, [this, &cancellationToken, oldItemsAdded](){return oldItemsAdded != itemCounter || !cancellationToken.load();});
        return std::move(dataLock);
    }

    [[nodiscard]]
    std::unique_lock<std::mutex> lock()
    {   
        std::unique_lock dataLock{dataMutex};
        return std::move(dataLock);
    }

    void popOldestUnlocked()
    {
        if (sizeUnlocked() == 0)
            return;

        if(start == end)
        {
            empty = true;
        }
        else 
        {
            start = (start + 1) % VSize;
        }
        
        removalDataCv.notify_all();
    }

    void clearUnlocked()
    {
        start = end = 0;
        empty = true;
        removalDataCv.notify_all();
    }

    void pushNext(const std::function<void(TValue*)>& writerLambda)
    {
        {
            std::unique_lock dataLock{dataMutex};
            moveIndices();
            writerLambda(&values[end]);
        }

        newDataCv.notify_all();
    }

    void pushNextWaitSpace(const std::function<void(TValue*)>& writerLambda, std::atomic<bool>& cancellationToken)
    {
        {
            std::unique_lock dataLock{dataMutex};
            if(isFullUnlocked())
            {
                removalDataCv.wait(dataLock, [this, &cancellationToken](){
                    return !isFullUnlocked() || !cancellationToken.load();
                });
                if(!cancellationToken.load())
                {
                    return;
                }
            }

            moveIndices();
            writerLambda(&values[end]);
        }

        newDataCv.notify_all();
    }

    void clear()
    {
        {
            std::unique_lock dataLock{dataMutex};
            start = end = 0;
        }

        newDataCv.notify_all();
    }

    const TValue& headUnlocked() const
    {
        return values[end];
    }

    TValue& headUnlocked()
    {
        return values[end];
    }

    const TValue& atUnlocked(size_t idx) const
    {
        return values[(start + idx)%VSize];
    }

    TValue& atUnlocked(size_t idx)
    {
        return values[(start + idx)%VSize];
    }

    void notifyWaiters()
    {
        newDataCv.notify_all();
        removalDataCv.notify_all();
    }
    size_t start = 0;
    size_t end = 0;
    bool empty = true;
    // monotonic counter for detecting that an item was added
    size_t itemCounter = 0;

    std::array<TValue, VSize> values = {0};

protected:
    std::mutex dataMutex;
    std::condition_variable newDataCv;
    std::condition_variable removalDataCv;

    void moveIndices()
    {
        if(empty)
        {
            empty = false;
            ++itemCounter;
            return;
        }
        end += 1;
        if(end >= VSize)
        {
            end = 0;
        }
        if(start == end)
        {
            start += 1;
            if(start >= VSize) {
                start = 0;
            }
        }
        ++itemCounter;
    }
};