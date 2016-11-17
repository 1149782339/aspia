/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/message_queue.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "client/message_queue.h"

MessageQueue::MessageQueue(ProcessMessageCallback &process_message) :
    process_message_(process_message),
    message_event_("message_event")
{
    // Nothing
}

MessageQueue::~MessageQueue()
{
    // Nothing
}

void MessageQueue::Add(std::unique_ptr<proto::ServerToClient> message)
{
    // ��������� ������� ���������.
    LockGuard<Mutex> guard(&queue_lock_);

    // ��������� ��������� � �������.
    queue_.push(std::move(message));

    // ���������� ����� � ���, ��� � ������� ���� ����� ���������.
    message_event_.Notify();
}

void MessageQueue::Worker()
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
            std::unique_ptr<proto::ServerToClient> message;

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

void MessageQueue::OnStart()
{
    // Nothing
}

void MessageQueue::OnStop()
{
    //
    // ���� ������ ���� ������� ������������, �� �������� ����������� �
    // ���������, ����� �������� ���� ������.
    //
    message_event_.Notify();
}
