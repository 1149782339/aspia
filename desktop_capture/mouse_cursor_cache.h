//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/mouse_cursor_cache.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H
#define _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H

#include <deque>

#include "desktop_capture/mouse_cursor.h"

namespace aspia {

class MouseCursorCache
{
public:
    MouseCursorCache(size_t cache_size = 12);
    ~MouseCursorCache();

    //
    // ���� ����������� ������ � ����.
    // ���� ������ ��� ��������� � ����, �� ������������ ������ ������� � ����.
    // ���� ������� ��� � ����, �� ������������ -1.
    //
    int Find(const MouseCursor *mouse_cursor);

    //
    // ����������� ������ � ��� � ���������� ������ ������������ ��������.
    //
    int Add(std::unique_ptr<MouseCursor> mouse_cursor);

    //
    // ���������� ��������� �� �������������� ������ �� ��� ������� � ����.
    //
    MouseCursor* Get(int index);

    //
    // ��������� ������� ��������� � ����.
    //
    bool IsEmpty() const;

    //
    // ������� ���.
    //
    void Clear();

private:
    std::deque<std::unique_ptr<MouseCursor>> cache_;
    const size_t cache_size_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__MOUSE_CURSOR_CACHE_H
