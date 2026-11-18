#pragma once

// Daily backup schedule, implemented with the Windows Task Scheduler.
//
// The task runs "drbc.exe --auto-backup" once a day at the chosen time. It is
// created per-user with /RL LIMITED, so no administrator rights are needed and
// nothing is stored in HKLM. Because the task belongs to the current user it
// runs while that user is logged on - the app itself does not need to be open.

#include <string>

namespace sched {

enum class State { NotScheduled, Scheduled, Error };

struct Status {
    State       state = State::NotScheduled;
    std::string time;    // "HH:MM" when scheduled
    std::string error;   // filled when state == Error
};

const wchar_t* TaskName();

Status query();
bool   enable(const std::string& hhmm, std::string& error);
bool   disable(std::string& error);

} // namespace sched
