//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/cursor_encoder.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/cursor_encoder.h"

#include "base/logging.h"

namespace aspia {

CursorEncoder::CursorEncoder() :
    compressor_(6)
{
    // Nothing
}

CursorEncoder::~CursorEncoder()
{
    // Nothing
}

uint8_t* CursorEncoder::GetOutputBuffer(proto::CursorShape *packet, size_t size)
{
    packet->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet->mutable_data()->data()));
}

void CursorEncoder::CompressCursor(proto::CursorShape *packet, const MouseCursor *mouse_cursor)
{
    // ������ ����� ����������� ��� ������ ������� �������.
    compressor_.Reset();

    int width = mouse_cursor->Size().Width();
    int height = mouse_cursor->Size().Height();

    // ������ ������ ������� � ������.
    const int row_size = width * sizeof(uint32_t);

    int packet_size = row_size * height;
    packet_size += packet_size / 100 + 16;

    uint8_t *compressed_pos = GetOutputBuffer(packet, packet_size);
    uint8_t *source_pos = mouse_cursor->Data();

    int filled = 0;   // ���������� ���� � ������ ����������.
    int row_pos = 0;  // Position in the current row in bytes.
    int row_y = 0;    // Current row.
    bool compress_again = true;

    while (compress_again)
    {
        Compressor::CompressorFlush flush = Compressor::CompressorNoFlush;

        // ���� �� �������� ��������� ������ � ��������������
        if (row_y == height - 1)
        {
            // ������ ��������������� ����
            flush = Compressor::CompressorFinish;
        }

        int consumed = 0; // ���������� ����, ������� ���� ����� �� ��������� ������.
        int written = 0;  // ���������� ����, ������� ���� �������� � ����� ����������.

        // ������� ��������� ������ ������.
        compress_again = compressor_.Process(source_pos + row_pos, row_size - row_pos,
                                             compressed_pos + filled, packet_size - filled,
                                             flush, &consumed, &written);

        row_pos += consumed; // �������� ��������� � ������� ������ ��������������.
        filled += written;   // ����������� ������� ��������� ������� ������ ����������.

        // If we have filled the message or we have reached the end of stream.
        if (filled == packet_size || !compress_again)
        {
            packet->mutable_data()->resize(filled);
            return;
        }

        // ���� �� �������� ����� ������� ������ � �������������� � ��� �� ��������� ������.
        if (row_pos == row_size && row_y < height - 1)
        {
            // ��������� ��������� � ������� ������.
            row_pos = 0;

            // ��������� � ��������� ������ � ������.
            source_pos += row_size;

            // ����������� ����� ������� ������.
            ++row_y;
        }
    }
}

void CursorEncoder::Encode(proto::CursorShape *packet, std::unique_ptr<MouseCursor> mouse_cursor)
{
    int index = cache_.Find(mouse_cursor.get());

    // ������ �� ������ � ����.
    if (index == -1)
    {
        packet->set_encoding(proto::CursorShapeEncoding::CURSOR_SHAPE_ENCODING_ZLIB);

        packet->set_width(mouse_cursor->Size().Width());
        packet->set_height(mouse_cursor->Size().Height());
        packet->set_hotspot_x(mouse_cursor->Hotspot().x());
        packet->set_hotspot_y(mouse_cursor->Hotspot().y());

        CompressCursor(packet, mouse_cursor.get());

        // ���� ��� ����.
        if (cache_.IsEmpty())
        {
            //
            // ������������� ������ � ���� � ����� �� ������� �������� ��� ����,
            // ����� �� ������� ������� ��� ��� �������.
            //
            packet->set_cache_index(-1);
        }

        // ��������� ������ � ���.
        cache_.Add(std::move(mouse_cursor));
    }
    else
    {
        packet->set_encoding(proto::CursorShapeEncoding::CURSOR_SHAPE_ENCODING_CACHE);
        packet->set_cache_index(index);
    }
}

} // namespace aspia
