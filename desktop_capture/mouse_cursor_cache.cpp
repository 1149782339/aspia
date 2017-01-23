//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/mouse_cursor_cache.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/mouse_cursor_cache.h"

namespace aspia {

MouseCursorCache::MouseCursorCache(size_t cache_size) :
    cache_size_(cache_size)
{
    // Nothing
}

MouseCursorCache::~MouseCursorCache()
{
    // Nothing
}

int MouseCursorCache::Find(const MouseCursor *mouse_cursor)
{
    int size = cache_.size();

    for (int index = 0; index < size; ++index)
    {
        // ���� ������ ������ � ����.
        if (cache_.at(index)->IsEqual(*mouse_cursor))
        {
            // ���������� ��� ������.
            return index;
        }
    }

    return -1;
}

int MouseCursorCache::Add(std::unique_ptr<MouseCursor> mouse_cursor)
{
    // ��������� ������ � ����� ������.
    cache_.push_back(std::move(mouse_cursor));

    // ���� ������� ������ ���� ��������� ������������ ������ ����.
    if (cache_.size() > cache_size_)
    {
        // ������� ������ ������� � ���� (����� ������).
        cache_.pop_front();
    }

    return cache_.size() - 1;
}

MouseCursor* MouseCursorCache::Get(int index)
{
    return cache_.at(index).get();
}

bool MouseCursorCache::IsEmpty() const
{
    return cache_.empty();
}

void MouseCursorCache::Clear()
{
    cache_.clear();
}

} // namespace aspia
