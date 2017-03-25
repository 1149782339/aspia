//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session_client.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/desktop_session_client.h"
#include "host/power_injector.h"
#include "base/process.h"

namespace aspia {

static const uint32_t kSupportedVideoEncodings =
    proto::VideoEncoding::VIDEO_ENCODING_ZLIB | proto::VideoEncoding::VIDEO_ENCODING_VP8 |
    proto::VideoEncoding::VIDEO_ENCODING_VP9;

static const uint32_t kSupportedAudioEncodings = 0;

static const uint32_t kSupportedFeatures =
    proto::DesktopFeatures::FEATURE_CURSOR_SHAPE | proto::DesktopFeatures::FEATURE_CLIPBOARD;

DesktopSessionClient::DesktopSessionClient()
{
    // Nothing
}

void DesktopSessionClient::Execute(const std::wstring& input_channel_name,
                                   const std::wstring& output_channel_name)
{
    //
    // ��� ���������� ������ ������� ��� ���������� ������ ������������ ������� ������
    // ���������� �������� ������������ ���������� �������.
    //
    SetProcessShutdownParameters(0, // System reserved last shutdown range.
                                 SHUTDOWN_NORETRY);

    PipeChannel::IncommingMessageCallback incomming_message_callback =
        std::bind(&DesktopSessionClient::OnIncommingMessage,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2);

    // ������������ � ������� ������.
    if (ipc_channel_.Connect(input_channel_name,
                             output_channel_name,
                             incomming_message_callback))
    {
        // ���������� ������ ������������.
        if (!SendConfigRequest())
        {
            // ���� �� ������� ��������� ������ ������������, �� ��������� �����.
            ipc_channel_.Disconnect();
        }

        // ���� ���� ���������� �� ����������.
        ipc_channel_.WaitForDisconnect();

        // ������������� ����� ������� ������.
        clipboard_.StopExchange();

        // ������������� �������� ����������� ������.
        screen_sender_.StopSending();
    }
}

bool DesktopSessionClient::OnIncommingMessage(const uint8_t* buffer, uint32_t size)
{
    proto::desktop::ClientToHost message;

    if (!message.ParseFromArray(buffer, size))
    {
        LOG(ERROR) << "ParseFromArray() failed";
        return false;
    }

    if (message.has_pointer_event())
    {
        return ReadPointerEvent(message.pointer_event());
    }
    else if (message.has_key_event())
    {
        return ReadKeyEvent(message.key_event());
    }
    else if (message.has_power_event())
    {
        return ReadPowerEvent(message.power_event());
    }
    else if (message.has_clipboard_event())
    {
        return ReadClipboardEvent(message.clipboard_event());
    }
    else if (message.has_config())
    {
        return ReadConfig(message.config());
    }

    DLOG(WARNING) << "Unhandled message from client";

    // ����������� ��������� ������������.
    return true;
}

bool DesktopSessionClient::WriteMessage(const proto::desktop::HostToClient& message)
{
    uint32_t message_size = message.ByteSizeLong();

    if (!message_size)
    {
        DLOG(ERROR) << "Wrong message size";
    }
    else
    {
        AutoLock lock(outgoing_lock_);

        if (outgoing_buffer_.Size() < message_size)
            outgoing_buffer_.Resize(message_size);

        message.SerializeWithCachedSizesToArray(outgoing_buffer_);

        if (ipc_channel_.WriteMessage(outgoing_buffer_, message_size))
            return true;
    }

    ipc_channel_.Disconnect();

    return false;
}

bool DesktopSessionClient::ReadPointerEvent(const proto::PointerEvent& event)
{
    if (!input_injector_)
        input_injector_.reset(new InputInjector());

    // ��������� ������� ����������� ������� �/��� ������� ������ ����.
    input_injector_->InjectPointerEvent(event);

    return true;
}

bool DesktopSessionClient::ReadKeyEvent(const proto::KeyEvent& event)
{
    //
    // ����� InputInjector ������ ������������������ �������,
    // � ������� ����� ����������� ��������� �����.
    //
    if (!input_injector_)
        input_injector_.reset(new InputInjector());

    // ��������� ������� ������� �������.
    input_injector_->InjectKeyEvent(event);

    return true;
}

bool DesktopSessionClient::ReadClipboardEvent(const proto::ClipboardEvent& event)
{
    clipboard_.InjectClipboardEvent(event);
    return true;
}

bool DesktopSessionClient::ReadPowerEvent(const proto::PowerEvent& event)
{
    InjectPowerEvent(event);
    return true;
}

bool DesktopSessionClient::SendClipboardEvent(const proto::ClipboardEvent& event)
{
    proto::desktop::HostToClient message;
    message.mutable_clipboard_event()->CopyFrom(event);
    return WriteMessage(message);
}

bool DesktopSessionClient::SendConfigRequest()
{
    proto::desktop::HostToClient message;

    proto::DesktopConfigRequest* request = message.mutable_config_request();

    request->set_video_encodings(kSupportedVideoEncodings);
    request->set_audio_encodings(kSupportedAudioEncodings);
    request->set_features(kSupportedFeatures);

    return WriteMessage(message);
}

bool DesktopSessionClient::ReadConfig(const proto::DesktopConfig& config)
{
    ScreenSender::MessageCallback message_callback =
        std::bind(&DesktopSessionClient::WriteMessage, this, std::placeholders::_1);

    // ��������������� �� � ������������ � ����������� �� ������� �����������.
    if (!screen_sender_.StartSending(config, message_callback))
        return false;

    // ���� �������� ������� �������� ����� ������� ������.
    if (config.flags() & proto::DesktopConfig::ENABLE_CLIPBOARD)
    {
        Clipboard::ClipboardEventCallback clipboard_event_callback =
            std::bind(&DesktopSessionClient::SendClipboardEvent, this, std::placeholders::_1);

        // ��������� ����� ������� ������.
        clipboard_.StartExchange(clipboard_event_callback);
    }
    else
    {
        // ������������� ����� ������� ������.
        clipboard_.StopExchange();
    }

    return true;
}

} // namespace aspia
