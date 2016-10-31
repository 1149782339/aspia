/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/service_control_win.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SERVICE_CONTROL_WIN_H
#define _ASPIA_BASE__SERVICE_CONTROL_WIN_H

#include "aspia_config.h"

#include <memory>

#include "base/macros.h"
#include "base/logging.h"

class ServiceControl
{
public:
    ServiceControl(const WCHAR *service_short_name);
    ~ServiceControl();

    //
    // ������� ������ � ������� � ���������� ��������� �� ��������� ������
    // ��� ���������� ��.
    // ���� �������� replace ���������� � true � ������ � ��������� ������
    // ��� ���������� � �������, �� ������������� ������ ����� �����������
    // � �������, � ������ ��� ������� �����.
    // ���� �������� replace ���������� � false, �� ��� ������� ������ �
    // ����������� ������ ����� ��������� ������� ���������.
    //
    static std::unique_ptr<ServiceControl> AddService(const WCHAR *exec_path,
                                                      const WCHAR *service_name,
                                                      const WCHAR *service_short_name,
                                                      const WCHAR *service_description,
                                                      bool replace = true);

    //
    // ��������� ���������� ���������� ������.
    // ���� ��������� ������ ��������, �� ������������ true, ���� ���, �� false.
    //
    bool IsValid() const;

    //
    // ��������� ������.
    // ���� ������ ������� ��������, �� ������������ true, ���� ���, �� false.
    //
    bool Start() const;

    //
    // ������������� ������.
    // ���� ������ ������� �����������, �� ������������ true, ���� ���, �� false.
    //
    bool Stop() const;

    //
    // ������� ������ �� �������.
    // ����� ������ ������ ����� ���������� ����������, ������ ������ �������
    // ����� ������������ � ������� � ����� IsValid() ��������� false.
    //
    bool Delete();

private:
    ServiceControl(SC_HANDLE sc_manager, SC_HANDLE service);

private:
    SC_HANDLE sc_manager_;
    SC_HANDLE service_;

    DISALLOW_COPY_AND_ASSIGN(ServiceControl);
};

#endif // _ASPIA_BASE__SERVICE_CONTROL_WIN_H
