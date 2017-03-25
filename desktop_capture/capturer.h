//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capturer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURER_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURER_H

#include "desktop_capture/desktop_frame.h"
#include "desktop_capture/desktop_region.h"
#include "desktop_capture/mouse_cursor.h"

namespace aspia {

class Capturer
{
public:
    Capturer() {}
    virtual ~Capturer() = default;

    //
    // Метод выполнения захвата экрана
    // Возвращает указатель на буфер, который содержит изображение экрана.
    //
    virtual const DesktopFrame* CaptureImage(bool* desktop_change) = 0;

    virtual MouseCursor* CaptureCursor() = 0;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURER_H
