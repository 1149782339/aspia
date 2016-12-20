/*
* PROJECT:         Aspia Remote Desktop
* FILE:            host/sas_injector.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "host/sas_injector.h"

#include "base/service_control.h"

namespace aspia {

static const WCHAR kSasServiceName[] = L"aspia-sas-service";

SasInjector::SasInjector() :
    Service(kSasServiceName),
    kernel32_(new ScopedKernel32Library())
{
    // Nothing
}

SasInjector::~SasInjector()
{
    // Nothing
}

//
// ������ ����� ������ SendSAS �������� ������ ���� �� �������� �� ������.
// ������ ���������� �������� �� ����� �������������������� ������������
// ������ ���, �.�. ���� ��� ���� ������������ SYSTEM, �� ��� ����� ��
// ������ ������������� �� ������ ������� ���� ����� ������.
//
void SasInjector::SendSAS()
{
    std::unique_ptr<ScopedSasPolice> sas_police(new ScopedSasPolice());

    DWORD session_id = kernel32_->WTSGetActiveConsoleSessionId();

    if (session_id == 0xFFFFFFFF)
    {
        DLOG(ERROR) << "Wrong session id";
        return;
    }

    ScopedNativeLibrary wmsgapi("wmsgapi.dll");

    typedef DWORD(WINAPI *PWMSGSENDMESSAGE)(DWORD session_id, UINT msg, WPARAM wParam, LPARAM lParam);

    PWMSGSENDMESSAGE send_message_func =
        reinterpret_cast<PWMSGSENDMESSAGE>(wmsgapi.GetFunctionPointer("WmsgSendMessage"));

    if (!send_message_func)
    {
        LOG(ERROR) << "WmsgSendMessage() not found in wmsgapi.dll";
        return;
    }

    BOOL as_user_ = FALSE;
    send_message_func(session_id, 0x208, 0, reinterpret_cast<LPARAM>(&as_user_));
}

void SasInjector::InjectSAS()
{
    DWORD session_id = 0;

    // �������� ID ������ ������������ ��� ������� ������� ������� �������.
    if (!kernel32_->ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        DLOG(WARNING) << "ProcessIdToSessionId() failed: " << GetLastError();
        return;
    }

    //
    // ���� ������� ID ������, �� ���������� �������� �� ��� ������, � ���
    // ���������� SAS ��� ���������� ��������� ��� �� ������.
    //
    if (session_id)
    {
        WCHAR module_path[MAX_PATH];

        // �������� ������ ���� � ������������ �����.
        if (!GetModuleFileNameW(nullptr, module_path, ARRAYSIZE(module_path)))
        {
            LOG(ERROR) << "GetModuleFileNameW() failed: " << GetLastError();
            return;
        }

        std::wstring command_line(module_path);

        // ��������� ���� ������� � ���� ������.
        command_line += L" --run_mode=sas";

        // ������������� ������ � �������.
        std::unique_ptr<ServiceControl> service_control =
            ServiceControl::Install(command_line.c_str(),
                                    kSasServiceName,
                                    kSasServiceName,
                                    kSasServiceName,
                                    true);

        // ���� ������ �����������.
        if (service_control)
        {
            // ��������� ��.
            service_control->Start();
        }
    }
    else
    {
        SendSAS();
    }
}

void SasInjector::DoService()
{
    // ��������� ������ ��� ���������� ������ Worker().
    DoWork();

    // ������� ������.
    ServiceControl(kSasServiceName).Delete();
}

void SasInjector::Worker()
{
    SendSAS();
}

void SasInjector::OnStop()
{
    // Nothing
}

} // namespace aspia
