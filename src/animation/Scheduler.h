#pragma once

#include <functional>
#include <list>

namespace fsvng {

struct ScheduledEvent {
    int nframes;
    std::function<void(void*)> eventCb;
    void* data;
};

class Scheduler {
public:
    static Scheduler& instance();

    void scheduleEvent(std::function<void(void*)> eventCb, void* data, int nframes);

    // Process events, returns true if any events pending or just executed
    bool iteration();

    bool hasPending() const { return !queue_.empty(); }

private:
    Scheduler() = default;
    std::list<ScheduledEvent> queue_;
};

} // namespace fsvng
