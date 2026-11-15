#include <usv_replay_lab/replay.h>

#include <algorithm>
#include <stdexcept>

usv_replay_lab::LinkEvent::LinkEvent(std::uint64_t _time_ms, EventKind _kind)
    : time_ms_(_time_ms), kind_(_kind)
{
}

bool usv_replay_lab::operator==(const LinkEvent &_left_event, const LinkEvent &_right_event)
{
    return _left_event.time_ms_ == _right_event.time_ms_ && _left_event.kind_ == _right_event.kind_;
}

usv_replay_lab::ReplayResult usv_replay_lab::replayHeartbeats(
    const std::vector<std::uint64_t> &_heartbeat_ms_vector, const ReplayConfig &_config, Subject _subject)
{
    if (_config.end_ms_ > MAX_TIME_MS || _config.timeout_ms_ == 0 || _config.timeout_ms_ > MAX_TIME_MS)
    {
        throw std::invalid_argument("Invalid replay horizon or timeout");
    }
    if (_config.drop_window_ &&
        (_config.drop_window_->begin_ms_ >= _config.drop_window_->end_ms_ ||
            _config.drop_window_->end_ms_ > _config.end_ms_ + 1))
    {
        throw std::invalid_argument("Drop window must be nonempty and inside the replay horizon");
    }
    if (!std::is_sorted(_heartbeat_ms_vector.begin(), _heartbeat_ms_vector.end()))
    {
        throw std::invalid_argument("Trace times must be nondecreasing");
    }
    if (!_heartbeat_ms_vector.empty() && _heartbeat_ms_vector.back() > _config.end_ms_)
    {
        throw std::invalid_argument("Trace contains a heartbeat after end_ms");
    }

    ReplayResult _result;
    std::uint64_t _deadline_ms = _config.timeout_ms_;
    const bool _track_timeout = _subject == Subject::REFERENCE;
    for (const std::uint64_t _time_ms : _heartbeat_ms_vector)
    {
        if (_config.drop_window_ && _time_ms >= _config.drop_window_->begin_ms_ &&
            _time_ms < _config.drop_window_->end_ms_)
        {
            ++_result.dropped_count_;
            continue;
        }

        // At a tie, deliver the heartbeat before the deadline check.
        if (_track_timeout && _deadline_ms < _time_ms)
        {
            _result.event_vector_.emplace_back(_deadline_ms, EventKind::TIMEOUT);
            _result.event_vector_.emplace_back(_time_ms, EventKind::RECOVERED);
        }
        ++_result.delivered_count_;
        _result.final_state_ = LinkState::ONLINE;
        _deadline_ms = _time_ms + _config.timeout_ms_;
    }

    // A trace can end before its last deadline. Advance to the scenario horizon.
    if (_track_timeout && _deadline_ms <= _config.end_ms_)
    {
        _result.event_vector_.emplace_back(_deadline_ms, EventKind::TIMEOUT);
        _result.final_state_ = LinkState::TIMED_OUT;
    }
    return _result;
}
