/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_zlib.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_decoder_zlib.h"

VideoDecoderZLIB::VideoDecoderZLIB()
{
    decompressor_.reset(new DecompressorZLIB());
}

VideoDecoderZLIB::~VideoDecoderZLIB()
{

}

void VideoDecoderZLIB::DecodeRect(const proto::VideoPacket *packet, uint8_t *dst_buffer)
{
    const proto::VideoRect &rect = packet->changed_rect(0);

    const int bytes_per_pixel = current_pixel_format_.bytes_per_pixel();

    const uint8_t *src = reinterpret_cast<const uint8_t*>(packet->data().data());
    const int src_size = packet->data().size();
    const int row_size = rect.width() * bytes_per_pixel;

    const int dst_stride = current_desktop_size_.width() * bytes_per_pixel;
    uint8_t *dst = dst_buffer + dst_stride * rect.y() + rect.x() * bytes_per_pixel;

    // Consume all the data in the message.
    bool decompress_again = true;
    int used = 0;

    int row_y = 0;   // ������� ������
    int row_pos = 0; // ��������� � ������� ������

    while (decompress_again && used < src_size)
    {
        if (row_y > rect.height() - 1)
        {
            LOG(WARNING) << "Too much data is received for the given rectangle.";
            return;
        }

        int written = 0;  // ���������� ���� ���������� � ����� ����������
        int consumed = 0; // ���������� ����, ������� ���� ����� �� ��������� ������

        // ������������� ��������� ������ ������
        decompress_again = decompressor_->Process(src + used,
                                                  src_size - used,
                                                  dst + row_pos,
                                                  row_size - row_pos,
                                                  &consumed,
                                                  &written);
        used += consumed;
        row_pos += written;

        // ���� �� ��������� ����������� ������ � ��������������
        if (row_pos == row_size)
        {
            // ����������� ������� �����
            ++row_y;

            // ���������� ������� ��������� � ������
            row_pos = 0;

            // ��������� � ��������� ������ � ������ ����������
            dst += dst_stride;
        }
    }

    // ���������� ������������ ����� ���������� ������� ��������������
    decompressor_->Reset();
}

bool VideoDecoderZLIB::Decode(const proto::VideoPacket *packet,
                              const PixelFormat &dst_format,
                              uint8_t *dst_buffer)
{
    if (packet->flags() & proto::VideoPacket::FIRST_PACKET)
    {
        current_pixel_format_ = dst_format;

        const proto::VideoPacketFormat &packet_format = packet->format();

        current_desktop_size_ = DesktopSize(packet_format.screen_width(),
                                            packet_format.screen_height());
    }

    if (packet->changed_rect_size() != 1)
        return false;

    DecodeRect(packet, dst_buffer);

    return true;
}
