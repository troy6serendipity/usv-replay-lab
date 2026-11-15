#include <usv_replay_lab/case_io.h>
#include <usv_replay_lab/replay.h>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace usv_replay_lab
{
    constexpr int EXIT_PASS = 0;
    constexpr int EXIT_MISMATCH = 1;
    constexpr int EXIT_ERROR = 2;
    constexpr char USAGE[] = "Usage: usv_replay_lab TRACE.csv SCENARIO.case [reference|ignore_timeout]\n";

    Subject parseSubject(std::string_view _name);
} // namespace usv_replay_lab

usv_replay_lab::Subject usv_replay_lab::parseSubject(std::string_view _name)
{
    if (_name == "reference")
    {
        return Subject::REFERENCE;
    }
    if (_name == "ignore_timeout")
    {
        return Subject::IGNORE_TIMEOUT;
    }
    throw std::invalid_argument("Subject must be reference or ignore_timeout");
}

int main(int _argument_count, char *_argument_array[])
{
    if (_argument_count == 2 && std::string_view(_argument_array[1]) == "--help")
    {
        std::cout << usv_replay_lab::USAGE;
        return usv_replay_lab::EXIT_PASS;
    }
    if (_argument_count != 3 && _argument_count != 4)
    {
        std::cerr << usv_replay_lab::USAGE;
        return usv_replay_lab::EXIT_ERROR;
    }
    try
    {
        const auto _subject =
            usv_replay_lab::parseSubject(_argument_count == 4 ? _argument_array[3] : "reference");
        const auto _case_spec = usv_replay_lab::case_io::readCase(_argument_array[2]);
        const auto _heartbeat_ms_vector = usv_replay_lab::case_io::readHeartbeats(_argument_array[1]);
        const auto _result =
            usv_replay_lab::replayHeartbeats(_heartbeat_ms_vector, _case_spec.config_, _subject);
        const bool _passed = _result.event_vector_ == _case_spec.expected_event_vector_;
        usv_replay_lab::case_io::writeReport(std::cout, _case_spec, _subject, _result, _passed);
        std::cout.flush();
        if (!std::cout)
        {
            throw std::runtime_error("Could not write JSON report");
        }
        return _passed ? usv_replay_lab::EXIT_PASS : usv_replay_lab::EXIT_MISMATCH;
    }
    catch (const std::exception &_error)
    {
        std::cerr << "error: " << _error.what() << '\n';
        return usv_replay_lab::EXIT_ERROR;
    }
}
