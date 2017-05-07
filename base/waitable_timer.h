//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/waitable_timer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WAITABLE_TIMER_H
#define _ASPIA_BASE__WAITABLE_TIMER_H

#include <chrono>
#include <functional>

#include "base/macros.h"

namespace aspia {

class WaitableTimer
{
public:
    WaitableTimer();
    ~WaitableTimer();

    using TimeoutCallback = std::function<void()>;

    // ��������� ���������� |signal_callback| ����� �������� ������� |time_delta_in_ms|.
    // ���� ������ ��� ��������� � ��������� ���������, �� ������� �������� �� �����������.
    // ����� ���������� |signal_callback| ������ ������������� ��������� � ������������� ���������.
    void Start(const std::chrono::milliseconds& time_delta, TimeoutCallback signal_callback);

    // ������������� ������ � ���������� ���������� callback-�������, ���� ��� �����������.
    void Stop();

    // ��������� ��������� �������.
    bool IsActive() const;

private:
    static void NTAPI TimerProc(LPVOID context, BOOLEAN timer_or_wait_fired);

private:
    TimeoutCallback signal_callback_;
    HANDLE timer_handle_;

    DISALLOW_COPY_AND_ASSIGN(WaitableTimer);
};

} // namespace aspia

#endif // _ASPIA_BASE__WAITABLE_TIMER_H
