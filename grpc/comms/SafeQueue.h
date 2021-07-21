#pragma once

#include <iostream>
#include <queue>
#include <mutex>

template<typename T>
class SafeQueue {
public:
    SafeQueue() : q(), m() {}

    void enqueue(T t) {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
    }

    bool try_pop(T& value) {
        std::unique_lock<std::mutex> lock(m);
        if (q.empty()) {
            return false;
        }

        value = q.front();
        q.pop();
        return true;
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
};
