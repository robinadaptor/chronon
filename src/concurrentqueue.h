//
// Copyright (c) 2018-2019 Chronon developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CHN_CONCURRENTQUEUE_H
#define CHN_CONCURRENTQUEUE_H


#include <mutex>
#include <condition_variable>
#include <deque>

template <typename T>
class concurrentqueue
{
private:
    boost::mutex              mutex;
    boost::condition_variable condition;
    std::deque<T>           queue;

public:
    void push(T const& value) {
        {
            boost::unique_lock<boost::mutex> lock(this->mutex);
            queue.push_front(value);
        }
        this->condition.notify_one();
    }
    T pop() {
        boost::unique_lock<boost::mutex> lock(this->mutex);
        this->condition.wait(lock, [=]{ return !this->queue.empty(); });
        T rc(std::move(this->queue.back()));
        this->queue.pop_back();
        return rc;
    }

    T popNotWait(){
        boost::unique_lock<boost::mutex> lock(this->mutex);
        T rc(std::move(this->queue.back()));
        this->queue.pop_back();
        return rc;
    }

    bool hasElements(){
        boost::unique_lock<boost::mutex> lock(this->mutex);
        return !queue.empty();
    }
};

#endif //CHN_CONCURRENTQUEUE_H
