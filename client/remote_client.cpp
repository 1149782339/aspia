/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/remote_client.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "client/remote_client.h"

#include "base/logging.h"

RemoteClient::RemoteClient(OnEventCallback on_event) :
    on_event_(on_event),
    current_encoding_(proto::VIDEO_ENCODING_UNKNOWN),
    port_(0)
{
    // Nothing
}

RemoteClient::~RemoteClient()
{
    // ���������� ���������� ������.
    WaitForEnd();
}

void RemoteClient::ConnectTo(const char *hostname, int port)
{
    // ��������� ����� � ����.
    hostname_ = hostname;
    port_ = port;

    // ��������� �����.
    Start();
}

void RemoteClient::Disconnect()
{
    // ���� ������� ������ ������������.
    Stop();
}

int32_t RemoteClient::ReadAuthRequest()
{
    std::unique_ptr<proto::ServerToClient> message(new proto::ServerToClient());

    socket_->ReadMessage(&message);

    return message->auth_request().methods();
}

void RemoteClient::ReadAuthResult(int32_t feature)
{
    std::unique_ptr<proto::ServerToClient> message(new proto::ServerToClient());

    socket_->ReadMessage(&message);

    if (!message->auth_result().success())
    {
        throw Exception("Authorisation error.");
    }

    if (!(message->auth_result().features() & feature))
    {
        throw Exception("Requested session feature not available. Access denied.");
    }
}

void RemoteClient::SendAuthReply(int32_t method)
{
    std::unique_ptr<proto::ClientToServer> message(new proto::ClientToServer());

    message->mutable_auth_reply()->set_method(method);

    socket_->WriteMessage(message.get());
}

void RemoteClient::ReadVideoPacket(const proto::VideoPacket &packet)
{
    // ���� ����� �������� ������ � ���������� ����������
    if (packet.flags() & proto::VideoPacket::FIRST_PACKET)
    {
        // �������� ���������
        int32_t encoding = packet.format().encoding();

        // ������������������ ������� ��� �������������
        if (current_encoding_ != encoding)
        {
            current_encoding_ = encoding;

            switch (encoding)
            {
                case proto::VIDEO_ENCODING_VP8:
                    decoder_.reset(new VideoDecoderVP8());
                    break;

                case proto::VIDEO_ENCODING_RAW:
                    decoder_.reset(new VideoDecoderRAW());
                    break;

                case proto::VIDEO_ENCODING_ZLIB:
                    decoder_.reset(new VideoDecoderZLIB());
                    break;

                default:
                    LOG(INFO) << "Unknown encoding: " << encoding;
                    break;
            }
        }

        if (VideoDecoder::IsResizeRequired(&packet))
        {
            pixel_format_ = VideoDecoder::GetPixelFormat(&packet);

            if (pixel_format_.is_empty())
                pixel_format_ = PixelFormat::MakeARGB();

            screen_size_ = VideoDecoder::GetScreenSize(&packet);

            if (decoder_)
            {
                decoder_->Resize(screen_size_, pixel_format_);
            }

            // ���������� ���� ������� �������� ������.
            window_->Resize(screen_size_, pixel_format_);
        }
    }

    // ���� ������� ���������������.
    if (decoder_)
    {
        {
            // �������� �����, � ������� ����� ����������� �������������.
            ScreenWindowWin::LockedMemory memory = window_->GetBuffer();

            decoder_->Decode(&packet, memory.get());
        }

        // �������� ����, ��� ����� ���������.
        window_->Invalidate();
    }
}

void RemoteClient::SendVideoControl(bool enable,
                                    int32_t encoding,
                                    const PixelFormat &pixel_format)
{
    std::unique_ptr<proto::ClientToServer> message(new proto::ClientToServer());

    proto::VideoControl *ctrl = message->mutable_video_control();

    ctrl->set_enable(enable);
    ctrl->set_encoding(encoding);

    //
    // ���� ������� �������� ��������� �� ������ ������ ������ ���
    // ������ ���������, � ��� ��������� - ������������.
    //
    if (encoding == proto::VIDEO_ENCODING_ZLIB ||
        encoding == proto::VIDEO_ENCODING_RAW)
    {
        proto::VideoPixelFormat *pf = ctrl->mutable_pixel_format();

        pf->set_bits_per_pixel(pixel_format.bits_per_pixel());

        pf->set_red_max(pixel_format.red_max());
        pf->set_green_max(pixel_format.green_max());
        pf->set_blue_max(pixel_format.blue_max());

        pf->set_red_shift(pixel_format.red_shift());
        pf->set_green_shift(pixel_format.green_shift());
        pf->set_blue_shift(pixel_format.blue_shift());
    }

    socket_->WriteMessage(message.get());
}

void RemoteClient::SendPointerEvent(int32_t x, int32_t y, int32_t mask)
{
    std::unique_ptr<proto::ClientToServer> message(new proto::ClientToServer());

    proto::PointerEvent *pe = message->mutable_pointer_event();

    pe->set_x(x);
    pe->set_y(y);
    pe->set_mask(mask);

    socket_->WriteMessage(message.get());
}

void RemoteClient::SendKeyEvent(int32_t keycode, bool extended, bool pressed)
{
    std::unique_ptr<proto::ClientToServer> message(new proto::ClientToServer());

    proto::KeyEvent *ke = message->mutable_key_event();

    ke->set_keycode(keycode);
    ke->set_extended(extended);
    ke->set_pressed(pressed);

    socket_->WriteMessage(message.get());
}

void RemoteClient::ProcessMessage(const proto::ServerToClient *message)
{
    if (message->has_video_packet())
    {
        ReadVideoPacket(message->video_packet());
    }
    else if (message->has_cursor())
    {
        LOG(ERROR) << "CursorShape unimplemented yet";
    }
    else if (message->has_clipboard())
    {
        LOG(ERROR) << "Clipboard unimplemented yet";
    }
    else if (message->has_text_chat())
    {
        LOG(ERROR) << "TextChat unimplemented yet";
    }
}

void RemoteClient::OnScreenWindowClosed()
{
    if (window_)
    {
        window_->WaitForEnd();
        window_.reset();
    }

    Disconnect();
}

void RemoteClient::Worker()
{
    LOG(INFO) << "Remote client thread started";

    try
    {
        // ������� �����.
        socket_.reset(new SocketTCP());

        // �������� ������������.
        socket_->Connect(hostname_.c_str(), port_);
    }
    catch (const Exception &err)
    {
        LOG(ERROR) << "Unable to connect: " << err.What();

        //
        // ���������� �������� callback ��� ����������� � ���, ��� ����������
        // ������������.
        //
        std::async(std::launch::async, on_event_, EventType::NotConnected);

        // �������, ����� ��������.
        return;
    }

    try
    {
        int32_t auth_methods = ReadAuthRequest();

        if (!(auth_methods & proto::AUTH_NONE))
        {
            throw Exception("Auth method not enabled.");
        }

        SendAuthReply(proto::AUTH_NONE);

        ReadAuthResult(proto::FEATURE_DESKTOP_MANAGE);
    }
    catch (const Exception &err)
    {
        LOG(ERROR) << "Unable to login: " << err.What();

        //
        // ���������� �������� callback ��� ����������� � ���, ��� ����������
        // ������������.
        //
        std::async(std::launch::async, on_event_, EventType::BadAuth);

        // �������, ����� ��������.
        return;
    }

    // ���� ����� ���� �� ���������������.
    if (!window_)
    {
        ScreenWindowWin::OnClosedCallback on_closed =
            std::bind(&RemoteClient::OnScreenWindowClosed, this);

        // �������������� ��� � ��������� �����.
        window_.reset(new ScreenWindowWin(this, nullptr, on_closed));
        window_->Start();
    }

    MessageQueue::ProcessMessageCallback process_message =
        std::bind(&RemoteClient::ProcessMessage, this, std::placeholders::_1);

    //
    // ��������� ��������� (� ��������� ������������� �����-�������) ����� ��������
    // ���������� �����. ��� ����, ����� �� ����������� ����� ������ ������� �� ����
    // �� ������� ��������� ����� ��� ��������� ���������. ��������� ����������� �
    // ������� � �������������� � ������� ����������� �� ������� ������.
    //
    message_queue_.reset(new MessageQueue(process_message));

    // ��������� ����� ��������� ���������.
    message_queue_->Start();

    try
    {
        //
        // ���������� �������� callback ��� ����������� � ���, ��� �� �������
        // ������������.
        //
        std::async(std::launch::async, on_event_, EventType::Connected);

        SendVideoControl(true, proto::VIDEO_ENCODING_ZLIB, PixelFormat::MakeRGB565());

        // ���������� ���� ���� ����� ���� �� ����������.
        while (!window_->IsEndOfThread())
        {
            // ������� ��������� ���������.
            std::unique_ptr<proto::ServerToClient> message(new proto::ServerToClient());

            // ������ ��������� �� �������.
            socket_->ReadMessage(&message);

            // ��������� ��������� � �������.
            message_queue_->Add(std::move(message));
        }
    }
    catch (const Exception &err)
    {
        LOG(ERROR) << "Exception in remote client thread: " << err.What();
    }

    // ���� ������� ������������ ������ ��������� ���������.
    message_queue_->Stop();

    // ���������� ���������.
    message_queue_->WaitForEnd();

    // ���������� �������� callback ��� ����������� � ���, ��� ����������� ���������.
    std::async(std::launch::async, on_event_, EventType::Disconnected);

    LOG(INFO) << "Remote client thread stopped";
}

void RemoteClient::OnStart()
{
    // Nothing
}

void RemoteClient::OnStop()
{
    // ���� ����� ��� ���������������.
    if (socket_)
    {
        // ��������� ���.
        socket_->Close();
    }
}
