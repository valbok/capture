/**
 * Copyright (C) 2020, Val Doroshchuk <valbok@gmail.com>
 */

#include "Capture/socket.h"
#include "Capture/socket_thread.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>
#include <iostream>

namespace Capture {

struct socket_thread_private
{
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    std::queue<socket> queue;
    std::atomic_bool stop{ false };
    std::function<bool(socket &)> callback;
    std::chrono::steady_clock::time_point time;

    ~socket_thread_private();
    void wait();
    void terminate();
    void push(socket &&s);
    void run();
};

socket_thread_private::~socket_thread_private()
{
    wait();
}

void socket_thread_private::wait()
{
    terminate();
    if (thread.joinable())
        thread.join();
}

void socket_thread_private::terminate()
{
    stop = true;
    cv.notify_one();
}

void socket_thread_private::push(socket &&s)
{
    mutex.lock();
    queue.push(std::move(s));
    mutex.unlock();
    cv.notify_one();
}

socket_thread::socket_thread()
    : m(new socket_thread_private)
{
}

socket_thread::socket_thread(socket_thread &&other)
    : socket_thread()
{
    auto tmp = m;
    m = other.m;
    other.m = tmp;
}

socket_thread::~socket_thread()
{
    delete m;
}

void socket_thread::push(socket &&s)
{
    m->push(std::move(s));
}

void socket_thread::start(const std::function<bool(socket &)> &f)
{
    m->stop = false;
    m->callback = f;
    m->thread = std::thread(&socket_thread_private::run, m);
}

void socket_thread_private::run()
{
    while (!stop) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return stop || !queue.empty(); });
        if (stop)
            break;

        auto socket = std::move(queue.front());
        queue.pop();
        lock.unlock();

        if (callback(socket))
            push(std::move(socket));
    }
}

void socket_thread::stop()
{
    m->terminate();
}

size_t socket_thread::size() const
{
    return m->queue.size();
}

} // Capture