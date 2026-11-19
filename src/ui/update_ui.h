#pragma once

// Starts the GitHub update check on a worker thread and hands the result back
// to a window through a message, so neither the main window nor the settings
// dialog ever blocks on the network.

#include <windows.h>

#include <memory>
#include <mutex>

#include "update_parse.h"

namespace ui {

// Posted to the target window (no parameters) when the check is finished.
constexpr UINT kUpdateCheckDone = WM_APP + 71;

struct UpdateCheck {
    std::mutex      mutex;
    bool            ok = false;
    upd::ReleaseInfo info;
    std::string     error;
};

// The window keeps the returned state alive; the worker only writes into it and
// then posts kUpdateCheckDone. If the window is gone, nothing happens and the
// state is released normally.
std::shared_ptr<UpdateCheck> StartUpdateCheck(HWND target);

} // namespace ui
