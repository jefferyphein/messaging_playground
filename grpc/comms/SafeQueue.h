#pragma once

#include <condition_variable>
#include <queue>
#include <mutex>

template<typename T>
class SafeQueue {
public:
    SafeQueue() : q(), m(), c() {}

    void enqueue(T *t) {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
    }

    void enqueue_n(T **Ts, size_t count) {
        std::lock_guard<std::mutex> lock(m);
        for (size_t index=0; index<count; index++) {
            q.push(Ts[index]);
        }
    }

    T *dequeue(int timeout=0) {
        std::unique_lock<std::mutex> lck(m);
        if (!c.wait_for(lck, std::chrono::milliseconds(timeout), [this]{ return !q.empty(); })) {
            return NULL;
        }

        T *value = q.front();
        q.pop();

        return value;
    }

    size_t dequeue_n(T **Ts, size_t count, int timeout=0) {
        std::unique_lock<std::mutex> lck(m);

        int total = std::min(q.size(), count);
        for (size_t index=0; index<total; index++) {
            Ts[index] = q.front();
            q.pop();
        }
        return total;
    }

    size_t size() const {
        return q.size();
    }

private:
    std::queue<T*> q;
    mutable std::mutex m;
    std::condition_variable c;
};
