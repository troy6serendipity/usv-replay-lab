#include <usv_replay_lab/case_io.h>

#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace usv_replay_lab::case_io
{
    constexpr std::size_t MAX_HEARTBEATS = 100'000;
    constexpr char CASE_VERSION[] = "1";
    constexpr int REPORT_VERSION = 1;

    bool readLine(std::istream &_input, std::string &_line);
    std::string readField(std::istream &_input, const std::string &_key);
    std::uint64_t parseTime(std::string_view _text, const std::string &_field);
    const char *describeState(LinkState _state);
    void writeEvents(std::ostream &_output, const std::vector<LinkEvent> &_event_vector);
} // namespace usv_replay_lab::case_io

bool usv_replay_lab::case_io::readLine(std::istream &_input, std::string &_line)
{
    if (!std::getline(_input, _line))
    {
        if (_input.bad() || !_input.eof())
        {
            throw std::runtime_error("Input could not be read");
        }
        return false;
    }
    if (!_line.empty() && _line.back() == '\r')
    {
        _line.pop_back();
    }
    return true;
}

std::string usv_replay_lab::case_io::readField(std::istream &_input, const std::string &_key)
{
    const std::string _prefix = _key + "=";
    std::string _line;
    if (!readLine(_input, _line) || _line.compare(0, _prefix.size(), _prefix) != 0 ||
        _line.size() == _prefix.size())
    {
        throw std::invalid_argument("Missing or empty field: " + _key);
    }
    return _line.substr(_prefix.size());
}

std::uint64_t usv_replay_lab::case_io::parseTime(std::string_view _text, const std::string &_field)
{
    if (_text.empty())
    {
        throw std::invalid_argument("Missing millisecond value: " + _field);
    }
    std::uint64_t _value = 0;
    const char *_end = _text.data() + _text.size();
    const auto _parsed = std::from_chars(_text.data(), _end, _value);
    if (_parsed.ec != std::errc() || _parsed.ptr != _end || _value > MAX_TIME_MS)
    {
        throw std::invalid_argument("Invalid millisecond value: " + _field);
    }
    return _value;
}

usv_replay_lab::case_io::CaseSpec usv_replay_lab::case_io::readCase(const std::string &_path)
{
    std::ifstream _input(_path, std::ios::binary);
    if (!_input)
    {
        throw std::runtime_error("Cannot open case: " + _path);
    }
    if (readField(_input, "schema_version") != CASE_VERSION)
    {
        throw std::invalid_argument("Unsupported case schema_version");
    }

    CaseSpec _case_spec;
    _case_spec.id_ = readField(_input, "scenario_id");
    if (_case_spec.id_.size() > 64 || _case_spec.id_.front() < 'a' || _case_spec.id_.front() > 'z' ||
        _case_spec.id_.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
    {
        throw std::invalid_argument("scenario_id must start with a lowercase letter and use a-z, 0-9, _");
    }
    _case_spec.config_.end_ms_ = parseTime(readField(_input, "end_ms"), "end_ms");
    _case_spec.config_.timeout_ms_ = parseTime(readField(_input, "timeout_ms"), "timeout_ms");
    const std::string _drop_range = readField(_input, "drop_window_ms");
    if (_drop_range != "none")
    {
        const std::size_t _separator = _drop_range.find(':');
        if (_separator == std::string::npos)
        {
            throw std::invalid_argument("drop_window_ms must be begin:end or none");
        }
        DropWindow _drop_window;
        _drop_window.begin_ms_ = parseTime(_drop_range.substr(0, _separator), "drop_window_ms begin");
        _drop_window.end_ms_ = parseTime(_drop_range.substr(_separator + 1), "drop_window_ms end");
        _case_spec.config_.drop_window_ = _drop_window;
    }

    const std::string _expected_events = readField(_input, "expected_events");
    if (_expected_events != "none")
    {
        if (_expected_events.back() == ',')
        {
            throw std::invalid_argument("expected_events cannot end with a comma");
        }
        std::istringstream _events_input(_expected_events);
        std::string _event_text;
        while (std::getline(_events_input, _event_text, ','))
        {
            const std::size_t _separator = _event_text.find('@');
            const std::string _kind = _event_text.substr(0, _separator);
            if (_separator == std::string::npos || (_kind != "timeout" && _kind != "recovered"))
            {
                throw std::invalid_argument("expected_events entries must be timeout@ms or recovered@ms");
            }
            const std::uint64_t _time_ms =
                parseTime(_event_text.substr(_separator + 1), "expected_events time");
            if (_time_ms > _case_spec.config_.end_ms_ ||
                (!_case_spec.expected_event_vector_.empty() &&
                    _time_ms < _case_spec.expected_event_vector_.back().time_ms_))
            {
                throw std::invalid_argument("expected_events must be ordered and within end_ms");
            }
            _case_spec.expected_event_vector_.emplace_back(
                _time_ms, _kind == "timeout" ? EventKind::TIMEOUT : EventKind::RECOVERED);
        }
    }
    std::string _extra_line;
    if (readLine(_input, _extra_line))
    {
        throw std::invalid_argument("Unexpected field or blank line after expected_events");
    }
    return _case_spec;
}

std::vector<std::uint64_t> usv_replay_lab::case_io::readHeartbeats(const std::string &_path)
{
    std::ifstream _input(_path, std::ios::binary);
    if (!_input)
    {
        throw std::runtime_error("Cannot open trace: " + _path);
    }
    std::string _line;
    if (!readLine(_input, _line) || _line != "time_ms,event")
    {
        throw std::invalid_argument("Expected CSV header: time_ms,event");
    }
    std::vector<std::uint64_t> _heartbeat_ms_vector;
    while (readLine(_input, _line))
    {
        const std::size_t _separator = _line.find(',');
        const std::string _location = "trace line " + std::to_string(_heartbeat_ms_vector.size() + 2);
        if (_separator == std::string::npos || _line.substr(_separator + 1) != "heartbeat")
        {
            throw std::invalid_argument("Expected time_ms,heartbeat at " + _location);
        }
        if (_heartbeat_ms_vector.size() == MAX_HEARTBEATS)
        {
            throw std::invalid_argument("Trace exceeds 100000 heartbeat records");
        }
        _heartbeat_ms_vector.push_back(parseTime(_line.substr(0, _separator), _location));
    }
    return _heartbeat_ms_vector;
}

const char *usv_replay_lab::case_io::describeState(LinkState _state)
{
    switch (_state)
    {
    case LinkState::WAITING:
    {
        return "waiting";
    }
    case LinkState::ONLINE:
    {
        return "online";
    }
    case LinkState::TIMED_OUT:
    {
        return "timed_out";
    }
    }
    throw std::logic_error("Unknown link state");
}

void usv_replay_lab::case_io::writeEvents(std::ostream &_output, const std::vector<LinkEvent> &_event_vector)
{
    _output << '[';
    for (std::size_t _index = 0; _index < _event_vector.size(); ++_index)
    {
        if (_index != 0)
        {
            _output << ',';
        }
        const LinkEvent &_event = _event_vector[_index];
        _output << "{\"time_ms\":" << _event.time_ms_ << ",\"kind\":\""
                << (_event.kind_ == EventKind::TIMEOUT ? "timeout" : "recovered") << "\"}";
    }
    _output << ']';
}

void usv_replay_lab::case_io::writeReport(std::ostream &_output, const CaseSpec &_case_spec, Subject _subject,
    const ReplayResult &_result, bool _passed)
{
    _output << "{\"report_version\":" << REPORT_VERSION << ",\"scenario_id\":\"" << _case_spec.id_
            << "\",\"subject\":\"" << (_subject == Subject::REFERENCE ? "reference" : "ignore_timeout")
            << "\",\"end_ms\":" << _case_spec.config_.end_ms_
            << ",\"timeout_ms\":" << _case_spec.config_.timeout_ms_
            << ",\"delivered_heartbeats\":" << _result.delivered_count_
            << ",\"dropped_heartbeats\":" << _result.dropped_count_ << ",\"final_state\":\""
            << describeState(_result.final_state_) << "\",\"expected_events\":";
    writeEvents(_output, _case_spec.expected_event_vector_);
    _output << ",\"observed_events\":";
    writeEvents(_output, _result.event_vector_);
    _output << ",\"passed\":" << (_passed ? "true" : "false") << "}\n";
}
