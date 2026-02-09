#include "animation/Scheduler.h"
#include "animation/Animation.h"

namespace fsvng {

Scheduler& Scheduler::instance() {
    static Scheduler inst;
    return inst;
}

void Scheduler::scheduleEvent(std::function<void(void*)> eventCb, void* data, int nframes) {
    ScheduledEvent event;
    event.nframes = nframes;
    event.eventCb = std::move(eventCb);
    event.data = data;

    // Make sure we're animating
    Animation::instance().requestRedraw();

    // Add new scheduled event to queue
    queue_.push_front(event);
}

bool Scheduler::iteration() {
    bool eventExecuted = false;

    // Update entries in queue, executing those that are
    // scheduled for the current frame
    auto it = queue_.begin();
    while (it != queue_.end()) {
        if (--it->nframes <= 0) {
            // Execute event
            if (it->eventCb) {
                it->eventCb(it->data);
            }
            // Remove record
            it = queue_.erase(it);
            eventExecuted = true;
        } else {
            ++it;
        }
    }

    return eventExecuted || !queue_.empty();
}

} // namespace fsvng
