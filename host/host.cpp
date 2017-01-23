//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host.h"

#include "base/exception.h"
#include "base/logging.h"
#include "base/power.h"
#include "base/clipboard.h"

namespace aspia {

Host::Host(std::unique_ptr<Socket> sock, OnEventCallback on_event_callback) :
    sock_(std::move(sock)),
    on_event_callback_(on_event_callback),
    feature_mask_(proto::FEATURE_NONE),
    read_buffer_size_(0),
    write_buffer_size_(0)
{
    SetThreadPriority(Thread::Priority::Highest);
    Start();
}

Host::~Host()
{
    if (IsActiveThread())
    {
        Stop();
        WaitForEnd();
    }
}

bool Host::IsDead()
{
    return IsThreadTerminating();
}

void Host::AsyncNotify(Host::Event type)
{
    std::async(std::launch::async, on_event_callback_, type);
}

void Host::WriteMessage(const proto::HostToClient *message)
{
    try
    {
        uint32_t size = message->ByteSize();

        if (size)
        {
            LockGuard<Lock> guard(&write_lock_);

            if (write_buffer_size_ < size)
            {
                write_buffer_size_ = size;
                write_buffer_.resize(size);
            }

            if (!message->SerializeToArray(write_buffer_.get(), size))
                throw Exception("Unable to serialize the message");

            uint8_t *encrypted_buffer = nullptr;

            encryptor_->Encrypt(write_buffer_.get(), size, &encrypted_buffer, &size);

            sock_->WriteMessage(encrypted_buffer, size);
        }
    }
    catch (const Exception &err)
    {
        DLOG(ERROR) << "An exception occurred: " << err.What();
        Stop();
    }
}

void Host::ReadMessage(std::unique_ptr<proto::ClientToHost> *message)
{
    uint32_t size = sock_->ReadMessageSize();

    if (size)
    {
        if (read_buffer_size_ < size)
        {
            read_buffer_size_ = size;
            read_buffer_.resize(size);
        }

        sock_->ReadMessage(read_buffer_.get(), size);

        uint8_t *buffer = nullptr;

        decryptor_->Decrypt(read_buffer_.get(), size, &buffer, &size);

        if (!message->get()->ParseFromArray(buffer, size))
            throw Exception("Unable to parse the message");
    }
}

void Host::DoKeyExchange()
{
    // �������������� �������� � ����������.
    encryptor_.reset(new EncryptorAES());
    decryptor_.reset(new DecryptorAES());

    // �������� ������ ���������� ����� � �������� ������ ��� ����.
    uint32_t public_key_len = decryptor_->GetPublicKeySize();
    std::unique_ptr<uint8_t[]> public_key(new uint8_t[public_key_len]);

    if (sock_->ReadMessageSize() != public_key_len)
        throw Exception("Wrong public key size recieved");

    // ������ ��������� ����.
    sock_->ReadMessage(public_key.get(), public_key_len);

    // ������������� ��������� ���� ��� ���������.
    encryptor_->SetPublicKey(public_key.get(), public_key_len);

    // �������� ��������� ���� �����������.
    decryptor_->GetPublicKey(public_key.get(), public_key_len);

    // ���������� ��������� ���� �����������.
    sock_->WriteMessage(public_key.get(), public_key_len);

    // �������� ������ ����������� ����� � �������� ������ ��� ����.
    uint32_t session_key_len = encryptor_->GetSessionKeySize();
    std::unique_ptr <uint8_t[]> session_key(new uint8_t[session_key_len]);

    // �������� ���������� ���� ���������.
    encryptor_->GetSessionKey(session_key.get(), session_key_len);

    // ���������� ���������� ����.
    sock_->WriteMessage(session_key.get(), session_key_len);

    if (sock_->ReadMessageSize() != session_key_len)
        throw Exception("Wrong session key size recieved");

    // ������ ���������� ���� �����������.
    sock_->ReadMessage(session_key.get(), session_key_len);

    // ������������� ���������� ���� ��� �����������.
    decryptor_->SetSessionKey(session_key.get(), session_key_len);
}

void Host::ProcessMessage(const proto::ClientToHost *message)
{
    if (message->has_pointer_event())
    {
        ReadPointerEvent(message->pointer_event());
    }
    else if (message->has_key_event())
    {
        ReadKeyEvent(message->key_event());
    }
    else if (message->has_bell_event())
    {
        ReadBellEvent(message->bell_event());
    }
    else if (message->has_control())
    {
        const proto::Control &control = message->control();

        if (control.has_video())
        {
            ReadVideoControl(control.video());
        }

        if (control.has_power())
        {
            ReadPowerControl(control.power());
        }

        if (control.has_clipboard())
        {
            ReadClipboardControl(control.clipboard());
        }
    }
    else if (message->has_clipboard())
    {
        ReadClipboard(message->clipboard());
    }
}

void Host::Worker()
{
    // �������� ����������� � ���, ��� ����������� ����� ������.
    AsyncNotify(Host::Event::Connected);

    try
    {
        // ��������� ����� ������� ����������.
        DoKeyExchange();

        //while (!IsThreadTerminating())
        //{

        //}

        feature_mask_ = proto::FEATURE_DESKTOP_MANAGE | proto::FEATURE_POWER_MANAGE |
            proto::FEATURE_BELL | proto::FEATURE_CLIPBOARD;

        // ���� ������ �� ����� ����������� ������������.
        if (feature_mask_ == proto::FEATURE_NONE)
        {
            // �������� ����������.
            throw Exception("Client session has no features");
        }

        std::unique_ptr<proto::ClientToHost> message(new proto::ClientToHost());

        // ���������� ���� ���� �� ����� ���� ������� ���������� �����.
        while (!IsThreadTerminating())
        {
            message->Clear();

            // ������ ��������� �� �������.
            ReadMessage(&message);

            ProcessMessage(message.get());
        }
    }
    catch (const Exception &err)
    {
        DLOG(ERROR) << "An exception occurred: " << err.What();
    }

    // ������������� � ���������� ������ �������� ��������� ������ � �������.
    screen_sender_.reset();

    Stop();

    //
    // ���������� ���������� ������� ������� ��������� ������ ������������ ��������
    // (����� ������� �� ��������, ��� ������ ���������� � ���������� ������� ��
    // ������ ������������ �������� "�������").
    //
    AsyncNotify(Host::Event::Disconnected);
}

void Host::OnStop()
{
    // ��������� ����������.
    sock_->Shutdown();
}

void Host::ReadPointerEvent(const proto::PointerEvent &msg)
{
    if (!(feature_mask_ & proto::FEATURE_DESKTOP_MANAGE))
        throw Exception("Session has no desktop manage feature");

    if (!input_injector_)
        input_injector_.reset(new InputInjector());

    // ��������� ������� ����������� ������� �/��� ������� ������ ����.
    input_injector_->InjectPointer(msg);
}

void Host::ReadKeyEvent(const proto::KeyEvent &msg)
{
    if (!(feature_mask_ & proto::FEATURE_DESKTOP_MANAGE))
        throw Exception("Session has no desktop manage feature");

    if (!input_injector_)
        input_injector_.reset(new InputInjector());

    // ��������� ������� ������� �������.
    input_injector_->InjectKeyboard(msg);
}

void Host::ReadBellEvent(const proto::BellEvent &msg)
{
    if (!(feature_mask_ & proto::FEATURE_BELL))
        throw Exception("Session has no bell feature");

    if (!input_injector_)
        input_injector_.reset(new InputInjector());

    input_injector_->InjectBell(msg);
}

void Host::ReadVideoControl(const proto::VideoControl &msg)
{
    //
    // ���� ������ ����� ����������� ���������� ������� ������ ���
    // ��������� �������� �����.
    //
    if (!(feature_mask_ & proto::FEATURE_DESKTOP_MANAGE) &&
        !(feature_mask_ & proto::FEATURE_DESKTOP_VIEW))
    {
        throw Exception("Session has no desktop manage or view features");
    }

    // ���� �������� ������� ��������� �������� �����-�������.
    if (!(msg.flags() & proto::VideoControl::ENABLE_VIDEO))
    {
        // ���������� ��������� ������ � �������.
        screen_sender_.reset();
        return;
    }

    // ���� �������� �����-������� �� ����������������.
    if (!screen_sender_)
    {
        ScreenSender::OnMessageCallback on_message_callback =
            std::bind(&Host::WriteMessage, this, std::placeholders::_1);

        // �������������� �������� �����-�������.
        screen_sender_.reset(new ScreenSender(on_message_callback));
    }

    // ��������������� �� � ������������ � ����������� �� ������� �����������.
    screen_sender_->Configure(msg);
}

void Host::ReadPowerControl(const proto::PowerControl &msg)
{
    if (!(feature_mask_ & proto::FEATURE_POWER_MANAGE))
    {
        throw Exception("Session has no power manage feature");
    }

    switch (msg.action())
    {
        case proto::PowerControl::SHUTDOWN:
            PowerControl::Shutdown();
            break;

        case proto::PowerControl::REBOOT:
            PowerControl::Reboot();
            break;

        case proto::PowerControl::HIBERNATE:
            PowerControl::Hibernate();
            break;

        case proto::PowerControl::SUSPEND:
            PowerControl::Suspend();
            break;

        case proto::PowerControl::LOGOFF:
            PowerControl::Logoff();
            break;

        default:
            LOG(ERROR) << "Unknown power control action requested: " << msg.action();
            break;
    }
}

void Host::ReadClipboardControl(const proto::ClipboardControl &msg)
{
    if (!(feature_mask_ & proto::FEATURE_CLIPBOARD))
    {
        throw Exception("Session has no clipboard feature");
    }

    if (msg.flags() & proto::ClipboardControl::REQUESTED)
    {
        proto::HostToClient msg;
        proto::Clipboard *clipboard = msg.mutable_clipboard();

        clipboard->set_data(Clipboard::Get());

        WriteMessage(&msg);
    }
}

void Host::ReadClipboard(const proto::Clipboard &msg)
{
    if (!(feature_mask_ & proto::FEATURE_CLIPBOARD))
    {
        throw Exception("Session has no clipboard feature");
    }

    Clipboard::Set(msg.data());
}

} // namespace aspia
