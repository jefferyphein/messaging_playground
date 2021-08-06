#pragma once

#include <condition_variable>
#include <queue>
#include <mutex>

template<typename T>
class SafeQueue {
public:
    SafeQueue() : q(), m(), c() {}

    void enqueue(T& t) {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
    }

    void enqueue_n(const std::vector<T>& Ts) {
        std::lock_guard<std::mutex> lock(m);
        for (const auto& t : Ts) {
            q.push(t);
        }
    }

    bool try_dequeue(T& t, int timeout_ms=0) {
        std::unique_lock<std::mutex> lck(m);
        if (!c.wait_for(lck, std::chrono::milliseconds(timeout_ms), [this]{ return !q.empty(); })) {
            return false;
        }

        t = q.front();
        q.pop();

        return true;
    }

    std::vector<T> dequeue_n(size_t count, int timeout_ms=0) {
        std::vector<T> vec;
        std::unique_lock<std::mutex> lck(m);

        if (!c.wait_for(lck, std::chrono::milliseconds(timeout_ms), [this]{ return !q.empty(); })) {
            return vec;
        }

        int total = std::min(q.size(), count);
        for (size_t index=0; index<total; index++) {
            vec.push_back(q.front());
            q.pop();
        }
        return vec;
    }

    size_t size() const {
        return q.size();
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};
