//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/screen_sender.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/screen_sender.h"

#include <thread>

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

ScreenSender::ScreenSender(OnMessageCallback on_message_callback) :
    encoding_(proto::VIDEO_ENCODING_UNKNOWN),
    on_message_callback_(on_message_callback)
{
    // Nothing
}

ScreenSender::~ScreenSender()
{
    if (IsActiveThread())
    {
        Stop();
        WaitForEnd();
    }
}

void ScreenSender::Configure(const proto::VideoControl &msg)
{
    // ��������� ����������.
    LockGuard<Lock> guard(&update_lock_);

    // ���� ���������� ���������.
    if (encoding_ != msg.encoding())
    {
        encoding_ = msg.encoding();

        // ������������������ ����������.
        switch (encoding_)
        {
            case proto::VIDEO_ENCODING_VP8:
                video_encoder_.reset(new VideoEncoderVP8());
                break;

            case proto::VIDEO_ENCODING_VP9:
                video_encoder_.reset(new VideoEncoderVP9());
                break;

            case proto::VIDEO_ENCODING_ZLIB:
                video_encoder_.reset(new VideoEncoderZLIB());
                break;

            default:
                LOG(ERROR) << "Unsupported video encoding: " << encoding_;
                throw Exception("Unsupported video encoding");
                break;
        }

        //
        // ���������� ������ ������, ����� ���������� ��� ��������������� � ������������
        // � ������� ��������.
        //
        prev_size_.Clear();
    }

    if (encoding_ == proto::VIDEO_ENCODING_ZLIB)
    {
        VideoEncoderZLIB *encoder = reinterpret_cast<VideoEncoderZLIB*>(video_encoder_.get());

        // ���� �������� ����� �������� ������ ������.
        if (msg.compress_ratio())
        {
            // ������������� ����� ������� ������.
            encoder->SetCompressRatio(msg.compress_ratio());
        }
    }

    // ���� ��� ������� ������ �������� �� �������.
    if (msg.has_pixel_format())
    {
        curr_format_.FromVideoPixelFormat(msg.pixel_format());
    }

    // ������� ������ ���� ���������������.
    DCHECK(video_encoder_);

    // ���� �������� ������� �������� �������� ����������� �������.
    if (msg.flags() & proto::VideoControl::ENABLE_CURSOR_SHAPE)
    {
        // ���� ������� ����������� ������� �� ���������������.
        if (!cursor_encoder_)
        {
            cursor_encoder_.reset(new CursorEncoder());
        }
    }
    else
    {
        // ���������������� ������� (��������� ��������).
        cursor_encoder_.reset();
    }

    // ���� �������� ������� ��������� ������� �������� �����.
    if (msg.flags() & proto::VideoControl::DISABLE_DESKTOP_EFFECTS)
    {
        // ���� ��� ��� �� ���������.
        if (!desktop_effects_)
        {
            // ���������.
            desktop_effects_.reset(new ScopedDesktopEffects());
        }
    }
    else
    {
        // �������� ������� �������� �����.
        desktop_effects_.reset();
    }

    int32_t interval = msg.update_interval();

    // ���� �������� ����� �������� ��������� ���������� � ��� �� ����� �����������.
    if (interval)
    {
        // ������������������ �����������.
        scheduler_.reset(new CaptureScheduler(interval));
    }

    // ���� ����������� �� ���������������.
    if (!scheduler_)
    {
        // ������������� ��� � ���������� ���������� �� ���������.
        scheduler_.reset(new CaptureScheduler(30));
    }

    // ���� ����� �� ������� �� ����������.
    if (!IsActiveThread())
    {
        // ������������� ��������� ��� ������ � ��������� ���.
        SetThreadPriority(Priority::Highest);
        Start();
    }
}

void ScreenSender::Worker()
{
    try
    {
        // ������� ��������� ������ ������� ������.
        std::unique_ptr<Capturer> capturer(new CapturerGDI());

        // ���������� ���� ���� �� ����� ���� ������� ��������� �����.
        while (!IsThreadTerminating())
        {
            //
            // ������ ���������� �������� �����-������� (���������, ������ ��������
            // �� ����� ���������� ��� �������� ������ ����������� ����������).
            //
            LockGuard<Lock> guard(&update_lock_);

            // ������ ������� ������� ������ ��������.
            scheduler_->BeginCapture();

            //
            // ������ ������ �����������, �������� ��� ������, ������ ��������
            // � ���������� ������.
            //
            const uint8_t *screen_buffer = capturer->CaptureImage(&dirty_region_, &curr_size_);

            // ���� ������� ������� ��������������� (�������� �������� ����������� �������).
            if (cursor_encoder_)
            {
                // ��������� ������ ����������� �������.
                std::unique_ptr<MouseCursor> mouse_cursor(capturer->CaptureCursor());

                // ���� ������ ������� (���������� �� �����������).
                if (mouse_cursor)
                {
                    // �������� ����������� �������.
                    cursor_encoder_->Encode(message_.mutable_cursor(), std::move(mouse_cursor));
                }
            }

            // ���� ���������� ������ �� ����.
            if (!dirty_region_.IsEmpty())
            {
                // ���� ���������� ������� ������ ��� ������ ��������.
                if (curr_size_ != prev_size_ || curr_format_ != prev_format_)
                {
                    // ��������� ����� ��������.
                    prev_format_ = curr_format_;
                    prev_size_ = curr_size_;

                    // �������� ������ ��������.
                    video_encoder_->Resize(curr_size_, curr_format_);

                    // ����� ��������� ��������� � �������� ������ ��� ������� ������.
                    dirty_region_.AddRect(DesktopRect::MakeSize(curr_size_));
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
                    // ��������� ����������� �����������.
                    flags = video_encoder_->Encode(message_.mutable_video_packet(),
                                                   screen_buffer,
                                                   dirty_region_);

                    // ���������� �����-����� �������.
                    on_message_callback_(&message_);

                    // ������� ��������� �� ���������� ������.
                    message_.Clear();

                } while (!(flags & proto::VideoPacket::LAST_PACKET));

                // ������� ������ �� ���������� ���������.
                dirty_region_.Clear();
            }
            // ���� ����������� ������ �� ����� �������, �� ������� ��������� �������.
            else if (message_.has_cursor())
            {
                // ���������� ���������.
                on_message_callback_(&message_);

                // ������� ��������� �� ���������� ������.
                message_.Clear();
            }

            // ���� ���������� ���������� ������ � �������.
            scheduler_->Wait();
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
