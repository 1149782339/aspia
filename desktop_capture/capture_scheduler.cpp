/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/capture_scheduler.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/capture_scheduler.h"

static const uint32_t kMaximumDelay = 100;

CaptureScheduler::CaptureScheduler() :
    begin_time_(0),
    diff_time_(0)
{
}

CaptureScheduler::~CaptureScheduler()
{
}

uint32_t CaptureScheduler::NextCaptureDelay()
{
    // �������� ������� ����� ������� � ���������� ����������
    diff_time_ = GetTickCount() - begin_time_;

    // ���� ������� ������ kMaximumDelay
    if (diff_time_ > kMaximumDelay)
    {
        diff_time_ = kMaximumDelay;
    }
    // ���� ������ ����
    else if (diff_time_ < 0)
    {
        diff_time_ = 0;
    }

    // ���������� �������� ��������. �� ����� ���� � �������� �� 0 �� kMaximumDelay
    return kMaximumDelay - diff_time_;
}

void CaptureScheduler::BeginCapture()
{
    // ��������� ����� ������ ���������� (� ��)
    begin_time_ = GetTickCount();
}
