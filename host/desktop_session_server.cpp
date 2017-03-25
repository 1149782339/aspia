//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session_server.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/desktop_session_server.h"
#include "host/desktop_session_launcher.h"
#include "base/scoped_privilege.h"

namespace aspia {

static const uint32_t kAttachTimeout = 30000; // 30 seconds

DesktopSessionServer::DesktopSessionServer() :
    state_(State::Detached)
{
    // Nothing
}

void DesktopSessionServer::StartSession(const OutgoingMessageCallback& outgoing_message_callback,
                                        const ErrorCallback& error_callback)
{
    outgoing_message_callback_ = outgoing_message_callback;
    error_callback_ = error_callback;

    timer_.Start(kAttachTimeout, error_callback_);

    // ��������� ������������ ��������� ������.
    StartSessionWatching();
}

void DesktopSessionServer::StopSession()
{
    StopSessionWatching();

    // ������������� ������.
    timer_.Stop();
}

void DesktopSessionServer::OnSessionAttached(uint32_t session_id)
{
    DCHECK(state_ != State::Detached); // ������ ������ ���� ���������.
    DCHECK(!process_.IsValid());       // ������� ������ ���� ������.
    DCHECK(!ipc_channel_);             // ������������� ����� ������ ���� ��������.

    std::wstring input_channel_id;
    std::wstring output_channel_id;

    // ������� ����� ��� ������ ������� � ����-��������� ������.
    std::unique_ptr<PipeServerChannel> ipc_channel(PipeServerChannel::Create(&input_channel_id,
                                                                             &output_channel_id));
    if (!ipc_channel)
        return;

    // ��������� ����� ������.
    if (!DesktopSessionLauncher::LaunchSession(session_id, input_channel_id, output_channel_id))
        return;

    // ������������ � ����� ������.
    if (!ipc_channel->Connect(outgoing_message_callback_))
        return;

    {
        //
        // ��� �������� ����-�������� � ��� ���������� ��������� �������������� ����������.
        // ���� ������� ������� ����������� �� ������, �� ������ ���������� ��� �������,
        // ���� ���, �� �������� ��.
        //
        ScopedProcessPrivilege privilege;
        privilege.Enable(SE_DEBUG_NAME);

        // ��������� ����-�������.
        process_ = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, ipc_channel->GetPeerPid());
    }

    if (!process_.IsValid())
    {
        LOG(ERROR) << "Unable to open session process: " << GetLastError();
        return;
    }

    ObjectWatcher::SignalCallback host_process_close_callback =
        std::bind(&DesktopSessionServer::OnHostProcessClose, this);

    if (!process_watcher_.StartWatching(process_.Handle(), host_process_close_callback))
        return;

    ipc_channel_ = std::move(ipc_channel);

    state_ = State::Attached;

    // ������ ����������, ������������� ������.
    timer_.Stop();
}

void DesktopSessionServer::OnSessionDetached()
{
    //
    // �� ����� ��� ������, ������� ����������� ���������� ������ � ���������� ����-��������.
    // ������ ����� ���� ��������� ����� �� ���. ���� ������ ��� ��������� (��� ��������� �
    // ������ ����������), �� �������.
    //
    if (state_ == State::Detached)
        return;

    state_ = State::Detached;

    process_watcher_.StopWatching();

    {
        ScopedProcessPrivilege privilege;
        privilege.Enable(SE_DEBUG_NAME);

        process_.Terminate(0);
    }

    process_.Close();

    ipc_channel_.reset();

    //
    // ��������� ������. ���� ����� ������ �� ����� ���������� � �������
    // ���������� ��������� �������, �� ��������� ������.
    //
    timer_.Start(kAttachTimeout, error_callback_);
}

void DesktopSessionServer::OnHostProcessClose()
{
    switch (state_)
    {
        case State::Attached:
            // TODO: ������� ����������� ����-��������.
            DetachSession();
            break;

        case State::Detached:
            break;
    }
}

void DesktopSessionServer::OnMessageFromClient(const uint8_t* buffer, uint32_t size)
{
    if (ipc_channel_)
        ipc_channel_->WriteMessage(buffer, size);
}

} // namespace aspia
