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

#include "base/thread.h"
#include "base/mutex.h"
#include "base/event.h"
#include "proto/proto.pb.h"

class MessageQueue : public Thread
{
public:
    typedef std::function<void(const proto::ServerToClient*)> ProcessMessageCallback;

    MessageQueue(ProcessMessageCallback &process_message);
    ~MessageQueue();

    void Add(std::unique_ptr<proto::ServerToClient> message);

private:
    void Worker() override;
    void OnStart() override;
    void OnStop() override;

private:
    // Callback ��� ��������� ���������.
    ProcessMessageCallback process_message_;

    // ������� ���������.
    std::queue <std::unique_ptr<proto::ServerToClient>> queue_;

    // Mutex ��� ������������ ������� ���������.
    Mutex queue_lock_;

    // ����� ��� ����������� � ������� ����� ���������.
    Event message_event_;
};

#endif // _ASPIA_CLIENT__MESSAGE_QUEUE_H
