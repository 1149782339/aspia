/*
* PROJECT:         Aspia Remote Desktop
* FILE:            host/screen_sender.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "host/screen_sender.h"

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

ScreenSender::ScreenSender(int32_t encoding,
                           const PixelFormat &client_pixel_format,
                           OnMessageCallback on_message) :
    current_encoding_(proto::VIDEO_ENCODING_UNKNOWN),
    client_pixel_format_(client_pixel_format),
    on_message_(on_message)
{
    Configure(encoding, client_pixel_format);

    SetThreadPriority(Priority::Highest);
    Start();
}

ScreenSender::~ScreenSender()
{
    if (!IsThreadTerminated())
    {
        Stop();
        WaitForEnd();
    }
}

void ScreenSender::Configure(int32_t encoding, const PixelFormat &client_pixel_format)
{
    // ��������� �������� �����-�������.
    LockGuard<Lock> guard(&update_lock_);

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
        else
        {
            LOG(ERROR) << "Unsupported video encoding: " << current_encoding_;
            throw Exception("Unsupported video encoding.");
        }
    }

    // ��������� ������ �������� �������.
    client_pixel_format_ = client_pixel_format;

    // ������� ������ ���� ���������������.
    DCHECK(encoder_);
}

void ScreenSender::Worker()
{
    // ������� ��������� ��� �������� �������.
    std::unique_ptr<proto::HostToClient> message(new proto::HostToClient());

    DesktopSize screen_size;
    DesktopSize prev_screen_size;

    PixelFormat host_pixel_format;
    PixelFormat prev_host_pixel_format;

    PixelFormat prev_client_pixel_format;

    DesktopRegion changed_region;

    try
    {
        // ������� ���������� ������� ������� ������ � ������������ ������� ������.
        std::unique_ptr<CaptureScheduler> scheduler(new CaptureScheduler());
        std::unique_ptr<Capturer> capturer(new CapturerGDI());

        // ���������� ���� ���� �� ����� ���� ������� ��������� �����.
        while (!IsThreadTerminating())
        {
            // ������ ������� ������� ������ ��������.
            scheduler->BeginCapture();

            // ������� ������ �� ���������� ���������.
            changed_region.Clear();

            //
            // ������ ������ �����������, �������� ��� ������, ������ ��������
            // � ���������� ������.
            //
            const uint8_t *screen_buffer =
                capturer->CaptureImage(changed_region, screen_size, host_pixel_format);

            // ���� ���������� ������ �� ����.
            if (!changed_region.IsEmpty())
            {
                //
                // ������ ���������� �������� �����-������� (���������, ������ ��������
                // �� ����� ���������� ��� �������� ������ ����������� ����������).
                //
                LockGuard<Lock> guard(&update_lock_);

                // ���� ���������� ������� ������ ��� ������ ��������.
                if (screen_size          != prev_screen_size ||
                    client_pixel_format_ != prev_client_pixel_format ||
                    host_pixel_format    != prev_host_pixel_format)
                {
                    // �������� ������ ��������.
                    encoder_->Resize(screen_size, host_pixel_format, client_pixel_format_);

                    // ��������� ����� ��������.
                    prev_host_pixel_format   = host_pixel_format;
                    prev_client_pixel_format = client_pixel_format_;
                    prev_screen_size         = screen_size;
                }

                int32_t flags;

                //
                // ���� ���������� ������ ����� ���� ������� �� ��������� �����-�������.
                // ��������, ��� ������������� ��������� VIDEO_ENCODING_ZLIB ������
                // ���������� ������������� ������ ������������ ��������� �������.
                // ���������� ���� ����������� ���� �� ����� ��������� ��������� �����.
                //
                do
                {
                    // ������� ��������� �� ���������� ������.
                    message->Clear();

                    // ��������� ����������� �����������.
                    flags = encoder_->Encode(message->mutable_video_packet(),
                                             screen_buffer,
                                             changed_region);

                    // ���������� �����-����� �������.
                    on_message_(message.get());

                } while (!(flags & proto::VideoPacket::LAST_PACKET));
            }

            scheduler->Sleep();
        }
    }
    catch (const Exception &err)
    {
        DLOG(ERROR) << "An exception occurred: " << err.What();
        Stop();
    }
}

void ScreenSender::OnStop()
{
    // Nothing
}

} // namespace aspia
