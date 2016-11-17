/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/exception.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__EXCEPTION_H
#define _ASPIA_BASE__EXCEPTION_H

#include <string>

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
    // ����������. �������� message �� ������ ���� ����� nullptr.
    //
    Exception(const char *description)
    {
        description_ = description;
    }

    virtual ~Exception() {}

    //
    // ����� ��� ��������� ���������� �������� ����������.
    //
    const char* What() const
    {
        return description_.c_str();
    }

private:
    std::string description_; // ������ ��������� �������� ����������.
};

#endif // _ASPIA_BASE__EXCEPTION_H
