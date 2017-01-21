//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/exception.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__EXCEPTION_H
#define _ASPIA_BASE__EXCEPTION_H

#include <string>

namespace aspia {

class Exception
{
public:
    //
    // ����������� �� ��������� (�� ����� ����������� ��������
    // ��������� �������� ����������).
    //
    Exception() {}

    //
    // ����������� � ������������ �������� ���������� ��������
    // ����������. �������� description �� ������ ���� ����� nullptr.
    //
    explicit Exception(const char *description)
    {
        description_ = description;
    }

    explicit Exception(const std::string &description)
    {
        description_ = description;
    }

    virtual ~Exception() {}

    // ����� ��� ��������� ���������� �������� ����������.
    const char* What() const
    {
        return description_.c_str();
    }

private:
    std::string description_; // ������ ��������� �������� ����������.
};

} // namespace aspia

#endif // _ASPIA_BASE__EXCEPTION_H
