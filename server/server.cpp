/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/server.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "server/server.h"

#include "base/logging.h"

Server::Server(const char *hostname, int port)
{
    // ������� ��������� ������.
    socket_.reset(new SocketTCP());

    // ������� ������ ��� �������� �����������.
    socket_->Bind(hostname, port);
    socket_->Listen();
}

Server::~Server()
{
    // Nothing
}

void Server::RegisterCallbacks(OnClientConnectedCallback OnClientConnected,
                               OnClientRejectedCallback OnClientRejected,
                               OnClientDisconnectedCallback OnClientDisconnected)
{
    OnClientConnected_    = OnClientConnected;
    OnClientRejected_     = OnClientRejected;
    OnClientDisconnected_ = OnClientDisconnected;
}

void Server::DisconnectAll()
{
    {
        // ��������� ������ ������������ ��������.
        LockGuard<Mutex> guard(&client_list_lock_);

        // �������� �� ����� ������ ������������ ��������.
        for (auto iter = client_list_.begin(); iter != client_list_.end(); ++iter)
        {
            // ���� ������� ������ ������� ������������.
            iter->get()->Stop();
        }
    }

    // ������� ������������� �������� �� ������.
    RemoteDeadClients();
}

void Server::Disconnect(uint32_t client_id)
{
    {
        // ��������� ������ ������������ ��������.
        LockGuard<Mutex> guard(&client_list_lock_);

        // �������� �� ����� ������ ������������ ��������.
        for (auto iter = client_list_.begin(); iter != client_list_.end(); ++iter)
        {
            Client *client = iter->get();

            // ���� ID ������� ��������� � �����������.
            if (client->GetID() == client_id)
            {
                // ���� ������� ���������� ����� �������.
                client->Stop();
                break;
            }
        }
    }

    // ������� �������������� ������� �� ������.
    RemoteDeadClients();
}

void Server::RemoteDeadClients()
{
    // ��������� ������ ������������ ��������.
    LockGuard<Mutex> guard(&client_list_lock_);

    auto iter = client_list_.begin();

    // �������� �� ����� ������ ������������ ��������.
    while (iter != client_list_.end())
    {
        Client *client = iter->get();

        // ���� ����� ������� ��������� � ������ ����������.
        if (client->IsEndOfThread())
        {
            // �������� ���������� ID �������.
            uint32_t id = client->GetID();

            // ���������� ���������� ������.
            client->WaitForEnd();

            // ������� ������� �� ������ � �������� ��������� ������� ������.
            iter = client_list_.erase(iter);

            // ���� callback ��� ���������������.
            if (!OnClientDisconnected_._Empty())
            {
                //
                // ���������� �������� callback ��� ���������� � ���,
                // ��� ������ ����������.
                //
                std::async(std::launch::async, OnClientDisconnected_, id);
            }
        }
        else
        {
            // ��������� � ���������� ��������.
            ++iter;
        }
    }
}

void Server::Worker()
{
    DLOG(INFO) << "Server thread started";

    try
    {
        // ���������� ���� ���� �� ����� ���� ������� ���������� ������� �����.
        while (!IsEndOfThread())
        {
            // ��������� �������� ����������� �� �������.
            std::unique_ptr<Socket> client_socket = socket_->Accept();

            // �������� IP ����� ������������� �������.
            std::string ip(client_socket->GetIpAddress());

            try
            {
                // ���������� ���������� ������������� �������.
                uint32_t id = id_generator_.Generate();

                Client::OnDisconnectedCallback on_disconnected =
                    std::bind(&Server::Disconnect, this, std::placeholders::_1);

                //
                // �������������� ������ ������������� �������. ���� �� ����� �������������
                // ��������� ����������, �� ������ ������ �������� ��-�� ������ �������������
                // ��� ��-�� ������������ �����������.
                //
                std::unique_ptr<Client> client(new Client(std::move(client_socket),
                                                          id,
                                                          on_disconnected));

                // ��������� ����� �������.
                client->Start();

                // ��������� ������ ������������ ��������.
                LockGuard<Mutex> guard(&client_list_lock_);

                // ��������� ������� � ������ ������������ ��������.
                client_list_.push_back(std::move(client));

                // ���� callback ��� ���������������.
                if (!OnClientConnected_._Empty())
                {
                    // ���������� �������� callback ��� ����������� � ���, ��� ����������� ����� ������.
                    std::async(std::launch::async, OnClientConnected_, id, ip);
                }
            }
            catch (const Exception &client_err)
            {
                LOG(ERROR) << "Exception when a new client connection: " << client_err.What();

                // ���� callback ��� ���������������.
                if (!OnClientRejected_._Empty())
                {
                    // ���������� �������� callback ��� ����������� � ���, ��� ������ ��������.
                    std::async(std::launch::async, OnClientRejected_, ip);
                }
            }
        }
    }
    catch (const Exception &server_err)
    {
        //
        // ���� �������� ���������� ��� ���������� �������, �� ���������� ������� �
        // ��� � ������� �� ������.
        //
        LOG(ERROR) << "Exception in server thread: " << server_err.What();
    }

    DLOG(INFO) << "Server thread stopped";
}

void Server::OnStop()
{
    // ��������� ���� ������������ ��������.
    DisconnectAll();
}
