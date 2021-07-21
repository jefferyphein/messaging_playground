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

    T *dequeue(int timeout=0) {
        std::unique_lock<std::mutex> lck(m);
        if (!c.wait_for(lck, std::chrono::milliseconds(timeout), [this]{ return !q.empty(); })) {
            return NULL;
        }

        T *value = q.front();
        q.pop();

        return value;
    }

    size_t size() const {
        return q.size();
    }

private:
    std::queue<T*> q;
    mutable std::mutex m;
    std::condition_variable c;
};
