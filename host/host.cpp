/*
* PROJECT:         Aspia Remote Desktop
* FILE:            host/host.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "host/host.h"

#include "base/exception.h"
#include "base/logging.h"

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
    if (!IsThreadTerminated())
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
                write_buffer_.reset(new ScopedAlignedBuffer(size));
            }

            if (!message->SerializeToArray(write_buffer_->get(), size))
            {
                LOG(ERROR) << "SerializeToArray() failed";
                throw Exception("Unable to serialize the message.");
            }

            uint8_t *encrypted_buffer = nullptr;

            encryptor_->Encrypt(write_buffer_->get(), size, &encrypted_buffer, &size);

            sock_->WriteMessage(encrypted_buffer, size);
        }
    }
    catch (const Exception&)
    {
        Stop();
    }
}

void Host::ReadMessage(std::unique_ptr<proto::ClientToHost> *message)
{
    uint32_t size = sock_->ReadMessageSize();

    if (!size)
    {
        LOG(ERROR) << "ReadMessageSize() returns zero";
        throw Exception("Serialized message size is equal to zero.");
    }

    if (read_buffer_size_ < size)
    {
        read_buffer_size_ = size;
        read_buffer_.reset(new ScopedAlignedBuffer(size));
    }

    sock_->ReadMessage(read_buffer_->get(), size);

    uint8_t *buffer = nullptr;

    decryptor_->Decrypt(read_buffer_->get(), size, &buffer, &size);

    if (!message->get()->ParseFromArray(buffer, size))
    {
        LOG(ERROR) << "ParseFromArray() failed";
        throw Exception("Unable to parse the message.");
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
    {
        LOG(ERROR) << "Wrong public key size recieved";
        throw Exception("Wrong public key size recieved.");
    }

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
    {
        LOG(ERROR) << "Wrong session key size recieved";
        throw Exception("Wrong session key size recieved.");
    }

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
    else if (message->has_video_control())
    {
        ReadVideoControl(message->video_control());
    }
    else if (message->has_cursor_shape_control())
    {
        DLOG(ERROR) << "CursorShapeControl unimplemented yet";
    }
    else if (message->has_clipboard())
    {
        DLOG(ERROR) << "Clipboard unimplemented yet";
    }
    else if (message->has_clipboard_request())
    {
        DLOG(ERROR) << "ClipboardRequest unimplemented yet";
    }
    else if (message->has_clipboard_control())
    {
        DLOG(ERROR) << "ClipboardControl unimplemented yet";
    }
    else if (message->has_power_control())
    {
        DLOG(ERROR) << "PowerControl unimplemented yet";
    }
    else if (message->has_bell())
    {
        DLOG(ERROR) << "Bell unimplemented yet";
    }
    else if (message->has_text_chat())
    {
        DLOG(ERROR) << "TextChat unimplemented yet";
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

        feature_mask_ = proto::FEATURE_DESKTOP_MANAGE;

        // ���� ������ �� ����� ����������� ������������.
        if (feature_mask_ == proto::FEATURE_NONE)
        {
            // �������� ����������.
            throw Exception("Client session has no features.");
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

    // ������������� � ���������� ����� �������� ��������� ������.
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
    {
        throw Exception("Session has no desktop manage feature.");
    }

    if (!input_injector_)
    {
        input_injector_.reset(new InputInjector());
    }

    // ��������� ������� ����������� ������� �/��� ������� ������ ����.
    input_injector_->InjectPointer(msg);
}

void Host::ReadKeyEvent(const proto::KeyEvent &msg)
{
    if (!(feature_mask_ & proto::FEATURE_DESKTOP_MANAGE))
    {
        throw Exception("Session has no desktop manage feature.");
    }

    if (!input_injector_)
    {
        input_injector_.reset(new InputInjector());
    }

    // ��������� ������� ������� �������.
    input_injector_->InjectKeyboard(msg);
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
        throw Exception("Session has no desktop manage or view features.");
    }

    // ���� �������� ������� ��������� �������� �����-�������.
    if (!msg.enable())
    {
        // ���������� ��������� ������.
        screen_sender_.reset();

        // �������, ������ �������� �� ���������.
        return;
    }

    // �� ��������� �������������� ������ �������� ������� � RGB565.
    PixelFormat format = PixelFormat::MakeRGB565();

    // ���� ��� ������� ������ �������� �� �������.
    if (msg.has_pixel_format())
    {
        // �������� ��� � ��������� � ��������� ������.
        const proto::VideoPixelFormat &pf = msg.pixel_format();

        format.SetBitsPerPixel(pf.bits_per_pixel());

        format.SetRedMax(pf.red_max());
        format.SetGreenMax(pf.green_max());
        format.SetBlueMax(pf.blue_max());

        format.SetRedShift(pf.red_shift());
        format.SetGreenShift(pf.green_shift());
        format.SetBlueShift(pf.blue_shift());
    }

    // ���� �������� �����-������� ��� ����������������.
    if (screen_sender_)
    {
        // ��������������� �� � ������������ � ����������� �� ������� �����������.
        screen_sender_->Configure(msg.encoding(), format);
    }
    else
    {
        ScreenSender::OnMessageCallback on_message =
            std::bind(&Host::WriteMessage, this, std::placeholders::_1);

        // �������������� �������� �����-������� � ����������� �� ������� �����������.
        screen_sender_.reset(new ScreenSender(msg.encoding(), format, on_message));
    }
}

} // namespace aspia
