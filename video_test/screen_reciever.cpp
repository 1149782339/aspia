/*
* PROJECT:         Aspia Remote Desktop
* FILE:            video_test/screen_reciever.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "video_test/screen_reciever.h"

ScreenReciever::ScreenReciever() :
    current_encoding_(proto::VIDEO_ENCODING_UNKNOWN)
{

}

ScreenReciever::~ScreenReciever()
{

}

void ScreenReciever::ReadPacket(proto::VideoPacket *packet)
{
    LockGuard<Mutex> guard(&packet_queue_lock_);

    // ��������� ����� � �������
    packet_queue_.push(packet->SerializeAsString());

    // ���������� ����� �������� ������� � ������� ����� �������
    packet_event_.Notify();
}

void ScreenReciever::Worker()
{
    // ���� ����� ���� �� ���������������
    if (!window_)
    {
        // �������������� ��� � ��������� �����
        window_.reset(new ScreenWindowWin(nullptr));
        window_->Start();
    }

    PixelFormat pixel_format = PixelFormat::MakeARGB();

    proto::VideoPacket *packet = google::protobuf::Arena::CreateMessage<proto::VideoPacket>(&arena_);

    // ���������� ���� ���� ����� ���� �� ����������
    while (!window_->IsEndOfThread())
    {
        // ���� ����������� � ���, ��� �������� ����� �����
        packet_event_.WaitForEvent();

        // ���������� ������������ ������� ���� ��� �� ����� ���������� ���������
        while (packet_queue_.size())
        {
            // ��������� ������� �������
            packet_queue_lock_.lock();

            // �������� ������ ����� �� �������
            packet->ParseFromString(packet_queue_.front());

            // ������� ������ ����� �� �������
            packet_queue_.pop();

            // ������������ ������� �������
            packet_queue_lock_.unlock();

            // ���� ����� �������� ������ � ���������� ����������
            if (packet->flags() & proto::VideoPacket::FIRST_PACKET)
            {
                const proto::VideoPacketFormat &packet_format = packet->format();

                // �������� ���������
                proto::VideoEncoding encoding = packet_format.encoding();

                // ������������������ ������� ��� �������������
                if (current_encoding_ != encoding)
                {
                    current_encoding_ = encoding;

                    if (encoding == proto::VIDEO_ENCODING_RAW)
                    {
                        decoder_.reset(new VideoDecoderRAW());
                    }
                    else if (encoding == proto::VIDEO_ENCODING_VP8)
                    {
                        decoder_.reset(new VideoDecoderVP8());
                    }
                    else if (encoding == proto::VIDEO_ENCODING_ZLIB)
                    {
                        decoder_.reset(new VideoDecoderZLIB());
                    }
                    else
                    {
                        LOG(INFO) << "Unknown encoding: " << packet_format.encoding();
                    }
                }

                // ���� ������� ������ �������� � ���������
                if (packet_format.has_pixel_format())
                {
                    // �������� ���
                    const proto::VideoPixelFormat &video_format = packet_format.pixel_format();

                    pixel_format.set_bits_per_pixel(video_format.bits_per_pixel());

                    pixel_format.set_red_max(video_format.red_max());
                    pixel_format.set_green_max(video_format.green_max());
                    pixel_format.set_blue_max(video_format.blue_max());

                    pixel_format.set_red_shift(video_format.red_shift());
                    pixel_format.set_green_shift(video_format.green_shift());
                    pixel_format.set_blue_shift(video_format.blue_shift());
                }
                else
                {
                    // ��� ���������� ������ ��-���������
                    pixel_format = PixelFormat::MakeARGB();
                }

                // ���������� ���� ������� �������� ������
                window_->Resize(DesktopSize(packet_format.screen_width(),
                                            packet_format.screen_height()),
                                pixel_format);
            }

            // ���� ������� ���������������
            if (decoder_)
            {
                // �������� �����, � ������� ����� ����������� �������������
                ScreenWindowWin::LockedMemory memory = window_->GetBuffer();

                // ���� ������� ������� ����������� �����
                if (decoder_->Decode(packet, pixel_format, memory.get()))
                {
                    // �������� ����, ��� ����� ���������
                    window_->Invalidate();
                }
            }
        }
    }
}

void ScreenReciever::OnStart() {}

void ScreenReciever::OnStop() {}
