/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/thread.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__THREAD_H
#define _ASPIA_BASE__THREAD_H

#include "aspia_config.h"

#include <stdint.h>
#include <assert.h>
#include <process.h>
#include <atomic>

#include "base/macros.h"
#include "base/mutex.h"
#include "base/logging.h"

class Thread
{
public:
    //
    // ����� ���������� ��������� � ���������������� ���������.
    // ��� ������� ������ ���������� ������� ����� Start().
    //
    Thread();
    virtual ~Thread();

    bool IsValid() const;

    //
    // ����� ��� ��������� ���������� ������.
    // ��� ������ ������� ������ ������������� ���������� �������������
    // �������-�������� ����� OnStop().
    // ����� OnStop() �� ����������, ���� ����� �� ������� ��� ����� ��
    // �������� ��������.
    //
    bool Stop();

    //
    // ���������� true, ���� ����� ��������� � ������ ���������� � false,
    // ���� ���.
    //
    bool IsEndOfThread() const;

    enum class Priority
    {
        Unknown      = 0, // �����������
        Idle         = 1, // ����� ������
        Lowest       = 2, // ������
        BelowNormal  = 3, // ���� �����������
        Normal       = 4, // ����������
        AboveNormal  = 5, // ���� �����������
        Highest      = 6, // �������
        TimeCritical = 7  // ���������
    };

    //
    // ������������� ��������� ���������� ������.
    //
    bool SetThreadPriority(Priority value = Priority::Normal);

    //
    // ���������� ������� ��������� ������.
    //
    Priority GetThreadPriority() const;

    //
    // ��������� �����.
    // ��� ������ ������ ���������� ����� OnStart(), ������� �����������
    // �������� ������.
    // ����� OnStart() �� ����������, ���� ����� �� �������� �������� ���
    // ���� ����� ��� �������.
    //
    bool Start();

    //
    // �������� ���������� ������.
    // ���� ����� ������� ����������, �� ������������ true, ���� ���, �� false.
    //
    bool WaitForEnd() const;

    //
    // �������� ���������� ������ �� ��������� �����.
    // ���� ����� ������� ����������, �� ������������ true, ���� ���, �� false.
    //
    bool WaitForEnd(uint32_t milliseconds) const;

    //
    // ����� �������� ��������� ������ �� �����, ������� ������� � ���������.
    //
    static void Sleep(uint32_t milliseconds);

    //
    // ���������� ���������� ������������ (ID) ������.
    //
    uint32_t GetThreadId() const;

    //
    // �����, ������� ��������������� ����������� � ������.
    // ����� ������ ���� ���������� �������� ������.
    //
    virtual void Worker() = 0;

    //
    // ���������� ��� ������ ������ Start().
    // ����� ������ ���� ���������� �������� ������.
    //
    virtual void OnStart() = 0;

    //
    // ���������� ��� ������ ������ Stop().
    // ����� ������ ���� ���������� �������� ������.
    //
    virtual void OnStop() = 0;

private:
    static UINT CALLBACK ThreadProc(LPVOID param);

private:
    HANDLE thread_;
    uint32_t thread_id_;
    std::atomic_bool started_;

    DISALLOW_COPY_AND_ASSIGN(Thread);
};

#endif // _ASPIA_BASE__THREAD_H
