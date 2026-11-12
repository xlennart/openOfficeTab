#pragma once

#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

// Keep the last composed document frame visible while Office replaces its
// window. The cover has no owner, so closing the document does not destroy it.
class RenameTransition {
    inline static thread_local HWND activeCover = nullptr;
    HWND cover = nullptr;
    HHOOK activationHook = nullptr;
    HWND source = nullptr;
    WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};

    static LRESULT CALLBACK onActivate(int code, WPARAM target, LPARAM detail) {
        const auto result = CallNextHookEx(nullptr, code, target, detail);
        if (code == HCBT_ACTIVATE && !result && activeCover) {
            const auto window = reinterpret_cast<HWND>(target);
            wchar_t className[16]{};
            GetClassNameW(window, className, ARRAYSIZE(className));
            // Save/close/open can show a modal prompt. Let Office show it normally.
            if (GetWindow(window, GW_OWNER) || lstrcmpW(className, L"#32770") == 0)
                ShowWindow(activeCover, SW_HIDE);
        }
        return result;
    }

public:
    RenameTransition() = default;
    RenameTransition(const RenameTransition&) = delete;
    RenameTransition& operator=(const RenameTransition&) = delete;
    ~RenameTransition() {
        if (activationHook) {
            activeCover = nullptr;
            UnhookWindowsHookEx(activationHook);
        }
        if (cover) DestroyWindow(cover);
    }

    bool begin(HWND window, HINSTANCE instance) {
        RECT bounds{};
        if (activeCover || !IsWindowVisible(window) || IsIconic(window) ||
            !GetWindowPlacement(window, &placement) ||
            !GetWindowRect(window, &bounds)) return false;
        SIZE size{bounds.right - bounds.left, bounds.bottom - bounds.top};
        if (size.cx <= 0 || size.cy <= 0) return false;

        // GetWindowRect uses the same coordinate space as the popup. Capture the
        // visible frame including the tabs and Office's custom title bar.
        if (FAILED(DwmFlush())) return false;
        HDC screen = GetDC(nullptr);
        if (!screen) return false;
        HDC image = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, size.cx, size.cy);
        if (image && bitmap) {
            const auto previous = SelectObject(image, bitmap);
            if (previous && BitBlt(image, 0, 0, size.cx, size.cy, screen,
                    bounds.left, bounds.top, SRCCOPY | CAPTUREBLT)) {
                cover = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                    WS_EX_TOPMOST | WS_EX_TRANSPARENT, L"STATIC", nullptr, WS_POPUP,
                    bounds.left, bounds.top, size.cx, size.cy, nullptr, nullptr, instance, nullptr);
                if (cover) {
                    POINT destination{bounds.left, bounds.top}, origin{};
                    if (!UpdateLayeredWindow(cover, screen, &destination, &size, image, &origin, 0, nullptr, ULW_OPAQUE)) {
                        DestroyWindow(cover);
                        cover = nullptr;
                    }
                }
            }
            if (previous) SelectObject(image, previous);
        }
        if (bitmap) DeleteObject(bitmap);
        if (image) DeleteDC(image);
        ReleaseDC(nullptr, screen);
        if (!cover) return false;

        activationHook = SetWindowsHookExW(WH_CBT, onActivate, nullptr, GetCurrentThreadId());
        if (!activationHook) return false;
        activeCover = cover;
        source = window;
        ShowWindow(cover, SW_SHOWNOACTIVATE);
        return SUCCEEDED(DwmFlush());
    }

    void reveal(HWND window) {
        if (!source || !IsWindow(window)) return;
        if (window != source) SetWindowPlacement(window, &placement);
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        DwmFlush();
    }
};
