//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/service.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SERVICE_H
#define _ASPIA_BASE__SERVICE_H

#include "aspia_config.h"

#include <string>

#include "base/macros.h"

namespace aspia {

class Service
{
public:
    explicit Service(const WCHAR *service_name);
    virtual ~Service();

    //
    // ����� ��������� ���������� ������ � ���������� ���������� ����� �� ���������.
    // ���� ������ ���� ������� �������� � ���������, �� ������������ true, ����
    // ��������� ���������� ������ �� �������, �� ������������ false.
    //
    bool DoWork();

    //
    // �����, � ������� ����������� ������ ������. ����� ��� ���������� ������
    // ��������� � ��������� "�����������".
    // ����� ����� ���� ���������� �������� ������.
    //
    virtual void Worker() = 0;

    //
    // ����� ���������� ��� ��������� ������.
    //
    virtual void OnStop() = 0;

private:
    static void WINAPI ServiceMain(int argc, LPWSTR argv);

    static DWORD WINAPI ServiceControlHandler(DWORD control_code,
                                              DWORD event_type,
                                              LPVOID event_data,
                                              LPVOID context);

    void SetStatus(DWORD state);

private:
    std::wstring service_name_;

    SERVICE_STATUS_HANDLE status_handle_;
    SERVICE_STATUS status_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

} // namespace aspia

#endif // _ASPIA_BASE__SERVICE_H
