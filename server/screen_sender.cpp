/*
* PROJECT:         Aspia Remote Desktop
* FILE:            server/screen_sender.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "server/screen_sender.h"

#include "base/logging.h"

ScreenSender::ScreenSender(int32_t encoding,
                           const PixelFormat &client_pixel_format,
                           OnMessageAvailabeCallback on_message_available) :
    current_encoding_(proto::VIDEO_ENCODING_UNKNOWN),
    client_pixel_format_(client_pixel_format),
    on_message_available_(on_message_available)
{
    Configure(encoding, client_pixel_format);
}

ScreenSender::~ScreenSender()
{
    // Nothing
}

void ScreenSender::Configure(int32_t encoding, const PixelFormat &client_pixel_format)
{
    // ��������� �������� �����-�������.
    LockGuard<Mutex> guard(&update_lock_);

    // ���� ���������� ���������.
    if (current_encoding_ != encoding)
    {
        // ������������������ ����������.
        if (encoding == proto::VIDEO_ENCODING_VP8)
        {
            encoder_.reset(new VideoEncoderVP8());
            current_encoding_ = encoding;
        }
        else if (encoding == proto::VIDEO_ENCODING_ZLIB)
        {
            encoder_.reset(new VideoEncoderZLIB());
            current_encoding_ = encoding;
        }
        else if (encoding == proto::VIDEO_ENCODING_RAW)
        {
            encoder_.reset(new VideoEncoderRAW());
            current_encoding_ = encoding;
        }
        else
        {
            LOG(WARNING) << "Unsupported video encoding: " << current_encoding_;
            throw Exception("Unsupported video encoding.");
        }
    }

    // ��������� ������ �������� �������.
    client_pixel_format_ = client_pixel_format;

    // ������� ������ ���� ���������������.
    CHECK(encoder_);
}

void ScreenSender::Worker()
{
    DLOG(INFO) << "Screen sender thread started";

    //
    // ��� ����������� �������� ��� �������� ��������� �����-�������
    // ������������� ������� ��������� ��� ������.
    //
    SetThreadPriority(Priority::Highest);

    DesktopRegion changed_region;

    DesktopSize screen_size;
    DesktopSize prev_screen_size;

    PixelFormat host_pixel_format;
    PixelFormat prev_host_pixel_format;

    PixelFormat prev_client_pixel_format;

    try
    {
        // ������� ���������� ������� ������� ������ � ������������ ������� ������.
        std::unique_ptr<CaptureScheduler> scheduler(new CaptureScheduler());
        std::unique_ptr<Capturer> capturer(new CapturerGDI());

        // ������� ��������� ��� �������� �������.
        std::unique_ptr<proto::ServerToClient> message(new proto::ServerToClient());

        // ���������� ���� ���� �� ����� ���� ������� ��������� �����.
        while (!IsEndOfThread())
        {
            // ������ ������� ������� ������ ��������
            scheduler->BeginCapture();

            //
            // ������ ������ �����������, �������� ��� ������, ������ ��������
            // � ���������� ������.
            //
            const uint8_t *screen_buffer =
                capturer->CaptureImage(changed_region, screen_size, host_pixel_format);

            // ���� ���������� ������ �� ����.
            if (!changed_region.is_empty())
            {
                //
                // ������ ���������� �������� �����-������� (���������, ������ ��������
                // �� ����� ���������� ��� �������� ������ ����������� ����������).
                //
                LockGuard<Mutex> guard(&update_lock_);

                // ���� ���������� ������� ������ ��� ������ ��������.
                if (screen_size != prev_screen_size ||
                    client_pixel_format_ != prev_client_pixel_format ||
                    host_pixel_format != prev_host_pixel_format)
                {
                    // �������� ������ ��������.
                    encoder_->Resize(screen_size, host_pixel_format, client_pixel_format_);

                    // ��������� ����� ��������.
                    prev_host_pixel_format = host_pixel_format;
                    prev_client_pixel_format = client_pixel_format_;
                    prev_screen_size = screen_size;
                }

                VideoEncoder::Status status = VideoEncoder::Status::Next;

                //
                // ���� ���������� ������ ����� ���� ������� �� ��������� �����-�������.
                // ��������, ��� ������������� ��������� VIDEO_ENCODING_ZLIB ������
                // ���������� ������������� ������ ������������ ��������� �������.
                // ���������� ���� ����������� ���� �� ����� ��������� ��������� �����.
                //
                while (status == VideoEncoder::Status::Next)
                {
                    // ������� ��������� �� ���������� ������.
                    message->Clear();

                    // ��������� ����������� �����������.
                    status = encoder_->Encode(message->mutable_video_packet(),
                                              screen_buffer,
                                              changed_region);

                    // ���������� �����-����� �������.
                    on_message_available_(message.get());
                }
            }

            // ���� ��������� ��������.
            Sleep(scheduler->NextCaptureDelay());
        }
    }
    catch (const Exception &err)
    {
        //
        // ���� ��������� ���������� ��� �������� �����-������ ��� ������ ������
        // (��������, ��� ������������� �����-���� �������), �� ��������� �����.
        //
        LOG(ERROR) << "Exception in screen sender thread: " << err.What();
    }

    DLOG(INFO) << "Screen sender thread stopped";
}

void ScreenSender::OnStop()
{
    // Nothing
}
