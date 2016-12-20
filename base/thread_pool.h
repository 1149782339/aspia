/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/thread_pool.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__THREAD_POOL_H
#define _ASPIA_BASE__THREAD_POOL_H

#include <list>
#include <memory>

#include "base/thread.h"

namespace aspia {

class ThreadPool
{
public:
    ThreadPool() {}
    ~ThreadPool() {}

    void Insert(std::unique_ptr<Thread> thread)
    {
        // ��������� ��� �������.
        LockGuard<Lock> guard(&list_lock_);

        // ������� ����������� ������.
        RemoveDeadThreads();

        // ��������� ����� �����.
        list_.push_back(std::move(thread));
    }

private:
    void RemoveDeadThreads()
    {
        auto iter = list_.begin();

        // �������� �� ����� �������.
        while (iter != list_.end())
        {
            Thread *thread = iter->get();

            // ���� ����� ��������.
            if (thread->IsThreadTerminated())
            {
                // ������� ����� � �������� ��������� ������� ������.
                iter = list_.erase(iter);
            }
            else
            {
                // ��������� � ���������� ��������.
                ++iter;
            }
        }
    }

private:
    std::list<std::unique_ptr<Thread>> list_;
    Lock list_lock_;
};

} // namespace aspia

#endif // _ASPIA_BASE__THREAD_POOL_H
