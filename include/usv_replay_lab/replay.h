#ifndef USV_REPLAY_LAB_REPLAY_H_
#define USV_REPLAY_LAB_REPLAY_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace usv_replay_lab
{
    inline constexpr std::uint64_t MAX_TIME_MS = 86'400'000;

    enum class LinkState
    {
        WAITING,
        ONLINE,
        TIMED_OUT
    };

    enum class EventKind
    {
        TIMEOUT,
        RECOVERED
    };

    enum class Subject
    {
        REFERENCE,
        IGNORE_TIMEOUT
    };

    struct LinkEvent
    {
        LinkEvent(std::uint64_t _time_ms, EventKind _kind);

        std::uint64_t time_ms_;
        EventKind kind_;
    };

    struct DropWindow
    {
        std::uint64_t begin_ms_ = 0;
        std::uint64_t end_ms_ = 0;
    };

    struct ReplayConfig
    {
        std::uint64_t end_ms_ = 0;
        std::uint64_t timeout_ms_ = 0;
        std::optional<DropWindow> drop_window_;
    };

    struct ReplayResult
    {
        std::size_t delivered_count_ = 0;
        std::size_t dropped_count_ = 0;
        LinkState final_state_ = LinkState::WAITING;
        std::vector<LinkEvent> event_vector_;
    };

    bool operator==(const LinkEvent &_left_event, const LinkEvent &_right_event);

    ReplayResult replayHeartbeats(const std::vector<std::uint64_t> &_heartbeat_ms_vector,
        const ReplayConfig &_config, Subject _subject);
} // namespace usv_replay_lab

#endif // USV_REPLAY_LAB_REPLAY_H_
