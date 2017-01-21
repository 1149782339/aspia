//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/server_tcp.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__SERVER_TCP_H
#define _ASPIA_NETWORK__SERVER_TCP_H

#include <functional>
#include <list>

#include "base/exception.h"
#include "base/macros.h"
#include "base/thread.h"

namespace aspia {

template <class T>
class ServerTCP : private Thread
{
public:
    ServerTCP(int port, typename T::OnEventCallback on_event_callback) :
        on_event_callback_(on_event_callback),
        sock_(new SocketTCP())
    {
        sock_->Bind(port);
        sock_->Listen();

        Start();
    }

    ~ServerTCP()
    {
        // ��������� ����������.
        sock_->Shutdown();

        if (IsActiveThread())
        {
            Stop();
            WaitForEnd();
        }
    }

private:
    void RemoveDeadClients()
    {
        // ��������� ������ ������������ ��������.
        LockGuard<Lock> guard(&client_list_lock_);

        auto iter = client_list_.begin();

        // �������� �� ����� ������ ������������ ��������.
        while (iter != client_list_.end())
        {
            // ���� ����� ������� ��������� � ������ ����������.
            if (iter->get()->IsDead())
            {
                // ������� ������� �� ������ � �������� ��������� ������� ������.
                iter = client_list_.erase(iter);
            }
            else
            {
                // ��������� � ���������� ��������.
                ++iter;
            }
        }
    }

    void OnClientEvent(typename T::Event type)
    {
        // ���� ����� ��������� ������, �� ���������� �������.
        if (IsThreadTerminating())
            return;

        if (type == typename T::Event::Disconnected)
            RemoveDeadClients();

        on_event_callback_(type);
    }

    void OnStop() override
    {
        // Nothing
    }

    void Worker() override
    {
        while (!IsThreadTerminating())
        {
            try
            {
                std::unique_ptr<Socket> sock(sock_->Accept());

                T::OnEventCallback on_event_callback =
                    std::bind(&ServerTCP::OnClientEvent, this, std::placeholders::_1);

                std::unique_ptr<T> client(new T(std::move(sock), on_event_callback));

                // ��������� ������ ������������ ��������.
                LockGuard<Lock> guard(&client_list_lock_);

                // ��������� ������� � ������ ������������ ��������.
                client_list_.push_back(std::move(client));
            }
            catch (const Exception &err)
            {
                DLOG(WARNING) << "Exception in tcp server: " << err.What();
                Stop();
            }
        }
    }

private:
    std::unique_ptr<Socket> sock_;

    std::list<std::unique_ptr<T>> client_list_;
    Lock client_list_lock_;

    typename T::OnEventCallback on_event_callback_;

    DISALLOW_COPY_AND_ASSIGN(ServerTCP);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SERVER_TCP_H
