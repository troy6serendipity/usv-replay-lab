#ifndef USV_COMM_EXAMPLES_EXAMPLE_ARGS_H_
#define USV_COMM_EXAMPLES_EXAMPLE_ARGS_H_

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace usv_comm_examples
{
    inline constexpr int IO_TIMEOUT_MS = 5000;
    inline std::uint16_t parsePort(std::string_view _text, bool _allow_zero);
} // namespace usv_comm_examples

inline std::uint16_t usv_comm_examples::parsePort(std::string_view _text, bool _allow_zero)
{
    if (_text.empty())
    {
        throw std::invalid_argument("Port is required");
    }
    unsigned int _port = 0;
    const char *_end = _text.data() + _text.size();
    const auto _result = std::from_chars(_text.data(), _end, _port);
    if (_result.ec != std::errc() || _result.ptr != _end || _port > 65535 || (!_allow_zero && _port == 0))
    {
        throw std::invalid_argument("Invalid port");
    }
    return static_cast<std::uint16_t>(_port);
}

#endif // USV_COMM_EXAMPLES_EXAMPLE_ARGS_H_
