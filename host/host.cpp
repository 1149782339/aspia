//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host.h"

namespace aspia {

Host::Host(std::unique_ptr<Socket> sock, const EventCallback& event_callback) :
    sock_(std::move(sock)),
    event_callback_(event_callback)
{
    Start();
}

bool Host::IsDead()
{
    return IsTerminating();
}

bool Host::DoKeyExchange()
{
    // �������������� �������� � ����������.
    encryptor_.reset(new EncryptorAES());
    decryptor_.reset(new DecryptorAES());

    if (!decryptor_->Init() || !encryptor_->Init())
        return false;

    // �������� ������ ���������� ����� � �������� ������ ��� ����.
    uint32_t public_key_len = decryptor_->GetPublicKeySize();
    std::unique_ptr<uint8_t[]> public_key(new uint8_t[public_key_len]);

    if (sock_->ReadSize() != public_key_len)
        return false;

    // ������ ��������� ����.
    if (!sock_->Read(public_key.get(), public_key_len))
        return false;

    // ������������� ��������� ���� ��� ���������.
    if (!encryptor_->SetPublicKey(public_key.get(), public_key_len))
        return false;

    // �������� ��������� ���� �����������.
    if (!decryptor_->GetPublicKey(public_key.get(), public_key_len))
        return false;

    // ���������� ��������� ���� �����������.
    if (!sock_->Write(public_key.get(), public_key_len))
        return false;

    // �������� ������ ����������� ����� � �������� ������ ��� ����.
    uint32_t session_key_len = encryptor_->GetSessionKeySize();
    std::unique_ptr <uint8_t[]> session_key(new uint8_t[session_key_len]);

    // �������� ���������� ���� ���������.
    if (!encryptor_->GetSessionKey(session_key.get(), session_key_len))
        return false;

    // ���������� ���������� ����.
    if (!sock_->Write(session_key.get(), session_key_len))
        return false;

    if (sock_->ReadSize() != session_key_len)
        return false;

    // ������ ���������� ���� �����������.
    if (!sock_->Read(session_key.get(), session_key_len))
        return false;

    // ������������� ���������� ���� ��� �����������.
    if (!decryptor_->SetSessionKey(session_key.get(), session_key_len))
        return false;

    return true;
}

bool Host::WriteMessage(const uint8_t* buffer, uint32_t size)
{
    const uint8_t* encrypted_buffer = encryptor_->Encrypt(buffer, size, &size);

    if (!encrypted_buffer || !sock_->Write(encrypted_buffer, size))
    {
        Stop();
        return false;
    }

    return true;
}

void Host::Worker()
{
    // ��������� ����� ������� ����������.
    if (!DoKeyExchange())
    {
        Stop();
        return;
    }

    DesktopSessionServer::OutgoingMessageCallback outgoing_message_callback =
        std::bind(&Host::WriteMessage, this, std::placeholders::_1, std::placeholders::_2);

    DesktopSessionServer::ErrorCallback error_callback =
        std::bind(&Host::Stop, this);

    session_.StartSession(outgoing_message_callback, error_callback);

    while (!IsTerminating())
    {
        uint32_t size = sock_->ReadSize();

        if (!size)
            break;

        if (read_buffer_.Size() < size)
            read_buffer_.Resize(size);

        if (!sock_->Read(read_buffer_, size))
            break;

        const uint8_t* buffer = decryptor_->Decrypt(read_buffer_, size, &size);
        if (!buffer)
            break;

        session_.OnMessageFromClient(buffer, size);
    }

    Stop();
}

void Host::OnStop()
{
    sock_->Disconnect();
    session_.StopSession();
}

} // namespace aspia
