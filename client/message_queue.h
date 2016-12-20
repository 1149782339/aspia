/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/message_queue.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CLIENT__MESSAGE_QUEUE_H
#define _ASPIA_CLIENT__MESSAGE_QUEUE_H

#include <functional>
#include <queue>
#include <memory>

#include "base/macros.h"
#include "base/thread.h"
#include "base/lock.h"
#include "base/waitable_event.h"

namespace aspia {

template <class T>
class MessageQueue : private Thread
{
public:
    typedef std::function<void(const T*)> ProcessMessageCallback;

    MessageQueue(ProcessMessageCallback process_message) :
        process_message_(process_message)
    {
        SetThreadPriority(Thread::Priority::Highest);
        Start();
    }

    ~MessageQueue()
    {
        if (!IsThreadTerminated())
        {
            Stop();
            WaitForEnd();
        }
    }

    void MessageQueue::Add(std::unique_ptr<T> &message)
    {
        {
            // ��������� ������� ���������.
            LockGuard<Lock> guard(&queue_lock_);

            // ��������� ��������� � �������.
            queue_.push(std::move(message));
        }

        // ���������� ����� � ���, ��� � ������� ���� ����� ���������.
        message_event_.Notify();
    }

private:
    void Dispatch()
    {
        //
        // ���������� ��������� ������� ���� �� ����� ���������� ��� ��������� ���
        // ����� �� ������� ������� ������������.
        //
        while (!queue_.empty() && !IsThreadTerminating())
        {
            std::unique_ptr<T> message;

            {
                // ��������� ������� ���������.
                LockGuard<Lock> guard(&queue_lock_);

                // ��������� ������ ��������� �� �������.
                message = std::move(queue_.front());

                // ������� ������ ��������� �� �������.
                queue_.pop();
            }

            // �������� callback ��� ��������� ���������.
            process_message_(message.get());
        }
    }

    void Worker() override
    {
        while (!IsThreadTerminating())
        {
            // ������� ����������� � ����� ���������.
            message_event_.WaitForEvent();

            Dispatch();
        }
    }

    void OnStop() override
    {
        //
        // ���� ������ ���� ������� ������������, �� �������� ����������� �
        // ���������, ����� �������� ���� ������.
        //
        message_event_.Notify();
    }

private:
    // Callback ��� ��������� ���������.
    ProcessMessageCallback process_message_;

    // ������� ���������.
    std::queue <std::unique_ptr<T>> queue_;

    // Mutex ��� ������������ ������� ���������.
    Lock queue_lock_;

    // ����� ��� ����������� � ������� ����� ���������.
    WaitableEvent message_event_;

    DISALLOW_COPY_AND_ASSIGN(MessageQueue);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__MESSAGE_QUEUE_H
