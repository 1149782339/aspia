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
#include "base/mutex.h"
#include "base/event.h"

template <class T>
class MessageQueue : public Thread
{
public:
    typedef std::function<void(const T*)> ProcessMessageCallback;

    MessageQueue(ProcessMessageCallback process_message) :
        process_message_(process_message)
    {
        // Nothing
    }

    ~MessageQueue()
    {
        // Nothing
    }

    void MessageQueue::Add(std::unique_ptr<T> &message)
    {
        {
            // ��������� ������� ���������.
            LockGuard<Mutex> guard(&queue_lock_);

            // ��������� ��������� � �������.
            queue_.push(std::move(message));
        }

        // ���������� ����� � ���, ��� � ������� ���� ����� ���������.
        message_event_.Notify();
    }

private:
    void Worker() override
    {
        while (true)
        {
            // ������� ����������� � ����� ���������.
            message_event_.WaitForEvent();

            // ���� ������ ���� �������� ������������, ���������� ����.
            if (IsEndOfThread()) break;

            // ���������� ��������� ������� ���� �� ����� ���������� ��� ���������.
            while (queue_.size())
            {
                std::unique_ptr<T> message;

                {
                    // ��������� ������� ���������.
                    LockGuard<Mutex> guard(&queue_lock_);

                    // ��������� ������ ��������� �� �������.
                    message = std::move(queue_.front());

                    // ������� ������ ��������� �� �������.
                    queue_.pop();
                }

                // �������� callback ��� ��������� ���������.
                process_message_(message.get());
            }
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
    Mutex queue_lock_;

    // ����� ��� ����������� � ������� ����� ���������.
    Event message_event_;

    DISALLOW_COPY_AND_ASSIGN(MessageQueue);
};

#endif // _ASPIA_CLIENT__MESSAGE_QUEUE_H
