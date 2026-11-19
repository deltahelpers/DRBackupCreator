#include "ui/update_ui.h"

#include <string>
#include <thread>

#include "update.h"

namespace ui {

std::shared_ptr<UpdateCheck> StartUpdateCheck(HWND target)
{
    auto state = std::make_shared<UpdateCheck>();

    std::thread([state, target]() {
        upd::ReleaseInfo info;
        std::string error;
        const bool ok = upd::FetchLatestRelease(info, error);

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->ok = ok;
            state->info = std::move(info);
            state->error = std::move(error);
        }

        if (IsWindow(target)) PostMessageW(target, kUpdateCheckDone, 0, 0);
    }).detach();

    return state;
}

} // namespace ui
