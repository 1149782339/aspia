/*
* PROJECT:         Aspia Remote Desktop
* FILE:            video_test/screen_sender.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "video_test/screen_sender.h"

ScreenSender::ScreenSender(ScreenReciever *reciever) : reciever_(reciever) {}

ScreenSender::~ScreenSender() {}

void ScreenSender::Worker()
{
    // �������������� ������ ������������ ������� ������, ������ ������ ������� ������ � �����������
    scheduler_.reset(new CaptureScheduler());
    capturer_.reset(new CapturerGDI());
    encoder_.reset(new VideoEncoderZLIB());

    // ��������� ����� ��������� �����-�������
    reciever_->Start();

    DesktopRegion region;
    DesktopSize size;
    PixelFormat pixel_format;

    // ���������� ���� ���� �� ���������� ����� ���������� ���������
    while (!reciever_->IsEndOfThread())
    {
        // ������ ������� ������� ������ ��������
        scheduler_->BeginCapture();

        // ������ ������ �����������, �������� ��� ������, ������ �������� � ���������� ������
        const uint8_t *buffer = capturer_->CaptureImage(region, size, pixel_format);

        // ���� ���������� ������ �� ����
        if (!region.is_empty())
        {
            int last = 0;

            // ���������� ���� ����������� ���� �� ����� ��������� ��������� �����
            while (!last)
            {
                // �������� ����
                proto::VideoPacket *packet =
                    encoder_->Encode(size, pixel_format, PixelFormat::MakeRGB64(), region, buffer);

                // ���� �� ������� ������� �����, �� ��������� ����
                if (!packet)
                {
                    break;
                }

                // ���� ��������� ��������� �����, �� ��������� ����
                last = (packet->flags() & proto::VideoPacket::LAST_PACKET);

                // ���������� ��������� �� ����� ����������
                reciever_->ReadPacket(packet);
            }
        }

        // ���� ���������� ������� ������
        Sleep(scheduler_->NextCaptureDelay());
    }
}

void ScreenSender::OnStart() {}

void ScreenSender::OnStop() {}
