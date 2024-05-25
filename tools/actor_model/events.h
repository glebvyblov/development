#include <library/cpp/actors/core/event_local.h>
#include <library/cpp/actors/core/events.h>

struct TEvents {
    // Вам нужно самостоятельно сюда добавить все необходимые events в NActors::TEvents::ES_PRIVATE
    enum TEvent {
        WriteEvent = EventSpaceBegin(NActors::TEvents::ES_PRIVATE),
        JobEndEvent,
    };

    static_assert(TEvent::JobEndEvent < EventSpaceEnd(NActors::TEvents::ES_PRIVATE), "EventSpace size too small");

    struct TEventWrite : NActors::TEventLocal<TEventWrite, TEvent::WriteEvent> {
    public:
        explicit TEventWrite(int64_t value) : value_(value) {
        }

        [[nodiscard]] int64_t GetValue() const noexcept {
            return value_;
        }

    private:
        const int64_t value_;
    };

    struct TEventJobEnd : NActors::TEventLocal<TEventJobEnd, TEvent::JobEndEvent> {
    };
};
