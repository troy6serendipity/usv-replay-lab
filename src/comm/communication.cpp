#include <usv_comm/communication.h>

#include <array>
#include <chrono>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace usv_comm::detail
{
#if defined(_WIN32)
    using SocketHandle = SOCKET;
    using AddressLength = int;
    constexpr SocketHandle INVALID_HANDLE = INVALID_SOCKET;
#else
    using SocketHandle = int;
    using AddressLength = socklen_t;
    constexpr SocketHandle INVALID_HANDLE = -1;
#endif
    using Clock = std::chrono::steady_clock;
    using Deadline = Clock::time_point;

    class SocketRuntime
    {
    public:
        SocketRuntime();
        ~SocketRuntime();
        SocketRuntime(const SocketRuntime &) = delete;
        SocketRuntime &operator=(const SocketRuntime &) = delete;
    };

    void initializeRuntime();
    int lastError() noexcept;
    bool shouldRetry(int _error) noexcept;
    bool isConnecting(int _error) noexcept;
    [[noreturn]] void failSystem(const char *_operation, int _error);
    void closeHandle(SocketHandle _handle) noexcept;
    void setNonblocking(SocketHandle _handle);
    std::unique_ptr<SocketState> makeSocket(int _type);
    SocketHandle requireHandle(const std::unique_ptr<SocketState> &_state);
    sockaddr_in makeAddress(const std::string &_address, std::uint16_t _port);
    std::uint16_t readLocalPort(SocketHandle _handle);
    Deadline makeDeadline(int _timeout_ms);
    void waitReady(SocketHandle _handle, bool _writing, Deadline _deadline);
    void sendAll(SocketHandle _handle, const std::uint8_t *_data, std::size_t _size, Deadline _deadline);
    void receiveAll(SocketHandle _handle, std::uint8_t *_data, std::size_t _size, Deadline _deadline);
} // namespace usv_comm::detail

namespace usv_comm
{
    struct SocketState
    {
        ~SocketState();
        detail::SocketHandle handle_ = detail::INVALID_HANDLE;
        bool bound_ = false;
    };
} // namespace usv_comm

usv_comm::CommunicationError::CommunicationError(ErrorCode _code, const std::string &_message)
    : std::runtime_error(_message), code_(_code)
{
}

usv_comm::ErrorCode usv_comm::CommunicationError::getCode() const noexcept
{
    return code_;
}

usv_comm::detail::SocketRuntime::SocketRuntime()
{
#if defined(_WIN32)
    WSADATA _data;
    const int _error = WSAStartup(MAKEWORD(2, 2), &_data);
    if (_error != 0)
    {
        failSystem("WSAStartup", _error);
    }
#endif
}

usv_comm::detail::SocketRuntime::~SocketRuntime()
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

void usv_comm::detail::initializeRuntime()
{
    static SocketRuntime _runtime;
}

int usv_comm::detail::lastError() noexcept
{
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool usv_comm::detail::shouldRetry(int _error) noexcept
{
#if defined(_WIN32)
    return _error == WSAEWOULDBLOCK || _error == WSAEINTR;
#else
    return _error == EWOULDBLOCK || _error == EAGAIN || _error == EINTR;
#endif
}

bool usv_comm::detail::isConnecting(int _error) noexcept
{
#if defined(_WIN32)
    return _error == WSAEWOULDBLOCK || _error == WSAEINPROGRESS || _error == WSAEALREADY ||
        _error == WSAEINTR;
#else
    return _error == EINPROGRESS || _error == EALREADY || _error == EINTR;
#endif
}

void usv_comm::detail::failSystem(const char *_operation, int _error)
{
    ErrorCode _code = ErrorCode::SYSTEM;
#if defined(_WIN32)
    if (_error == WSAECONNRESET || _error == WSAECONNABORTED || _error == WSAENOTCONN)
#else
    if (_error == ECONNRESET || _error == ECONNABORTED || _error == ENOTCONN || _error == EPIPE)
#endif
    {
        _code = ErrorCode::CLOSED;
    }
    throw CommunicationError(
        _code, std::string(_operation) + " failed (socket error " + std::to_string(_error) + ")");
}

void usv_comm::detail::closeHandle(SocketHandle _handle) noexcept
{
    if (_handle == INVALID_HANDLE)
    {
        return;
    }
#if defined(_WIN32)
    closesocket(_handle);
#else
    ::close(_handle);
#endif
}

usv_comm::SocketState::~SocketState()
{
    detail::closeHandle(handle_);
}

void usv_comm::detail::setNonblocking(SocketHandle _handle)
{
#if defined(_WIN32)
    u_long _enabled = 1;
    if (ioctlsocket(_handle, FIONBIO, &_enabled) != 0)
    {
        failSystem("ioctlsocket", lastError());
    }
#else
    const int _flags = fcntl(_handle, F_GETFL, 0);
    if (_flags < 0 || fcntl(_handle, F_SETFL, _flags | O_NONBLOCK) < 0 ||
        fcntl(_handle, F_SETFD, FD_CLOEXEC) < 0)
    {
        failSystem("fcntl", lastError());
    }
#endif
}

std::unique_ptr<usv_comm::SocketState> usv_comm::detail::makeSocket(int _type)
{
    initializeRuntime();
    auto _state = std::make_unique<SocketState>();
    _state->handle_ = ::socket(AF_INET, _type, 0);
    if (_state->handle_ == INVALID_HANDLE)
    {
        failSystem("socket", lastError());
    }
    setNonblocking(_state->handle_);
    return _state;
}

usv_comm::detail::SocketHandle usv_comm::detail::requireHandle(const std::unique_ptr<SocketState> &_state)
{
    if (!_state || _state->handle_ == INVALID_HANDLE)
    {
        throw CommunicationError(ErrorCode::CLOSED, "Socket is closed");
    }
    return _state->handle_;
}

sockaddr_in usv_comm::detail::makeAddress(const std::string &_address, std::uint16_t _port)
{
    sockaddr_in _endpoint = {};
    _endpoint.sin_family = AF_INET;
    _endpoint.sin_port = htons(_port);
    if (_address.find('\0') != std::string::npos ||
        inet_pton(AF_INET, _address.c_str(), &_endpoint.sin_addr) != 1)
    {
        throw CommunicationError(ErrorCode::INVALID_ARGUMENT, "Expected a numeric IPv4 address");
    }
    return _endpoint;
}

std::uint16_t usv_comm::detail::readLocalPort(SocketHandle _handle)
{
    sockaddr_in _address = {};
    AddressLength _length = sizeof(_address);
    if (getsockname(_handle, reinterpret_cast<sockaddr *>(&_address), &_length) != 0)
    {
        failSystem("getsockname", lastError());
    }
    return ntohs(_address.sin_port);
}

usv_comm::detail::Deadline usv_comm::detail::makeDeadline(int _timeout_ms)
{
    if (_timeout_ms <= 0)
    {
        throw CommunicationError(ErrorCode::INVALID_ARGUMENT, "timeout_ms must be positive");
    }
    return Clock::now() + std::chrono::milliseconds(_timeout_ms);
}

void usv_comm::detail::waitReady(SocketHandle _handle, bool _writing, Deadline _deadline)
{
#if !defined(_WIN32)
    if (_handle < 0 || _handle >= FD_SETSIZE)
    {
        throw CommunicationError(ErrorCode::SYSTEM, "Socket descriptor exceeds select capacity");
    }
#endif
    while (true)
    {
        const auto _remaining =
            std::chrono::duration_cast<std::chrono::microseconds>(_deadline - Clock::now());
        if (_remaining.count() <= 0)
        {
            throw CommunicationError(ErrorCode::TIMEOUT, "Operation deadline exceeded");
        }
        timeval _timeout = {};
        _timeout.tv_sec = static_cast<long>(_remaining.count() / 1'000'000);
        _timeout.tv_usec = static_cast<long>(_remaining.count() % 1'000'000);
        fd_set _ready_set;
        fd_set _error_set;
        FD_ZERO(&_ready_set);
        FD_ZERO(&_error_set);
        FD_SET(_handle, &_ready_set);
        FD_SET(_handle, &_error_set);
#if defined(_WIN32)
        const int _descriptor_count = 0;
#else
        const int _descriptor_count = _handle + 1;
#endif
        const int _selected = ::select(_descriptor_count, _writing ? nullptr : &_ready_set,
            _writing ? &_ready_set : nullptr, &_error_set, &_timeout);
        if (_selected > 0)
        {
            return;
        }
        if (_selected == 0)
        {
            throw CommunicationError(ErrorCode::TIMEOUT, "Operation deadline exceeded");
        }
        const int _error = lastError();
        if (!shouldRetry(_error))
        {
            failSystem("select", _error);
        }
    }
}

void usv_comm::detail::sendAll(
    SocketHandle _handle, const std::uint8_t *_data, std::size_t _size, Deadline _deadline)
{
    std::size_t _offset = 0;
    while (_offset < _size)
    {
        waitReady(_handle, true, _deadline);
#if defined(_WIN32)
        const int _sent = ::send(
            _handle, reinterpret_cast<const char *>(_data + _offset), static_cast<int>(_size - _offset), 0);
#else
        const auto _sent = ::send(_handle, _data + _offset, _size - _offset, MSG_NOSIGNAL);
#endif
        if (_sent > 0)
        {
            _offset += static_cast<std::size_t>(_sent);
            continue;
        }
        if (_sent == 0)
        {
            throw CommunicationError(ErrorCode::CLOSED, "Peer closed during frame send");
        }
        const int _error = lastError();
        if (!shouldRetry(_error))
        {
            failSystem("send", _error);
        }
    }
}

void usv_comm::detail::receiveAll(
    SocketHandle _handle, std::uint8_t *_data, std::size_t _size, Deadline _deadline)
{
    std::size_t _offset = 0;
    while (_offset < _size)
    {
        waitReady(_handle, false, _deadline);
#if defined(_WIN32)
        const int _received =
            ::recv(_handle, reinterpret_cast<char *>(_data + _offset), static_cast<int>(_size - _offset), 0);
#else
        const auto _received = ::recv(_handle, _data + _offset, _size - _offset, 0);
#endif
        if (_received > 0)
        {
            _offset += static_cast<std::size_t>(_received);
            continue;
        }
        if (_received == 0)
        {
            throw CommunicationError(ErrorCode::CLOSED, "Peer closed during frame receive");
        }
        const int _error = lastError();
        if (!shouldRetry(_error))
        {
            failSystem("recv", _error);
        }
    }
}

usv_comm::UDPSocket::UDPSocket() : state_(detail::makeSocket(SOCK_DGRAM))
{
}

usv_comm::UDPSocket::~UDPSocket() = default;
usv_comm::UDPSocket::UDPSocket(UDPSocket &&_other) noexcept = default;
usv_comm::UDPSocket &usv_comm::UDPSocket::operator=(UDPSocket &&_other) noexcept = default;

void usv_comm::UDPSocket::bind(const std::string &_address, std::uint16_t _port)
{
    const auto _endpoint = detail::makeAddress(_address, _port);
    if (!state_)
    {
        state_ = detail::makeSocket(SOCK_DGRAM);
    }
    if (state_->bound_)
    {
        throw CommunicationError(ErrorCode::INVALID_ARGUMENT, "UDP socket is already bound");
    }
    if (::bind(state_->handle_, reinterpret_cast<const sockaddr *>(&_endpoint), sizeof(_endpoint)) != 0)
    {
        detail::failSystem("bind", detail::lastError());
    }
    state_->bound_ = true;
}

std::uint16_t usv_comm::UDPSocket::localPort() const
{
    return detail::readLocalPort(detail::requireHandle(state_));
}

void usv_comm::UDPSocket::sendTo(const std::string &_address, std::uint16_t _port,
    const std::vector<std::uint8_t> &_data_vector, int _timeout_ms)
{
    const auto _deadline = detail::makeDeadline(_timeout_ms);
    const auto _endpoint = detail::makeAddress(_address, _port);
    if (_port == 0)
    {
        throw CommunicationError(ErrorCode::INVALID_ARGUMENT, "Destination port must be nonzero");
    }
    if (_data_vector.size() > MAX_DATAGRAM_BYTES)
    {
        throw CommunicationError(ErrorCode::FRAME_TOO_LARGE, "UDP payload exceeds 65507 bytes");
    }
    const auto _handle = detail::requireHandle(state_);
    const char *_data = _data_vector.empty() ? "" : reinterpret_cast<const char *>(_data_vector.data());
    if (!state_->bound_)
    {
        bind("0.0.0.0", 0);
    }
    while (true)
    {
        detail::waitReady(_handle, true, _deadline);
        const auto _sent = ::sendto(_handle, _data, static_cast<int>(_data_vector.size()), 0,
            reinterpret_cast<const sockaddr *>(&_endpoint), sizeof(_endpoint));
        if (_sent >= 0)
        {
            state_->bound_ = true;
            if (static_cast<std::size_t>(_sent) != _data_vector.size())
            {
                throw CommunicationError(ErrorCode::SYSTEM, "Incomplete UDP datagram send");
            }
            return;
        }
        const int _error = detail::lastError();
        if (!detail::shouldRetry(_error))
        {
            detail::failSystem("sendto", _error);
        }
    }
}

usv_comm::Datagram usv_comm::UDPSocket::receive(int _timeout_ms)
{
    const auto _deadline = detail::makeDeadline(_timeout_ms);
    const auto _handle = detail::requireHandle(state_);
    Datagram _datagram;
    _datagram.data_vector_.resize(MAX_DATAGRAM_BYTES);
    while (true)
    {
        detail::waitReady(_handle, false, _deadline);
        sockaddr_in _source = {};
        detail::AddressLength _length = sizeof(_source);
        const auto _received = ::recvfrom(_handle, reinterpret_cast<char *>(_datagram.data_vector_.data()),
            static_cast<int>(_datagram.data_vector_.size()), 0, reinterpret_cast<sockaddr *>(&_source),
            &_length);
        if (_received >= 0)
        {
            _datagram.data_vector_.resize(static_cast<std::size_t>(_received));
            std::array<char, INET_ADDRSTRLEN> _address_array = {};
            if (inet_ntop(AF_INET, &_source.sin_addr, _address_array.data(),
                    static_cast<detail::AddressLength>(_address_array.size())) == nullptr)
            {
                detail::failSystem("inet_ntop", detail::lastError());
            }
            _datagram.address_ = _address_array.data();
            _datagram.port_ = ntohs(_source.sin_port);
            return _datagram;
        }
        const int _error = detail::lastError();
        if (!detail::shouldRetry(_error))
        {
            detail::failSystem("recvfrom", _error);
        }
    }
}

void usv_comm::UDPSocket::close() noexcept
{
    state_.reset();
}

usv_comm::TCPStream::TCPStream() = default;
usv_comm::TCPStream::~TCPStream() = default;
usv_comm::TCPStream::TCPStream(TCPStream &&_other) noexcept = default;
usv_comm::TCPStream &usv_comm::TCPStream::operator=(TCPStream &&_other) noexcept = default;

usv_comm::TCPStream::TCPStream(std::unique_ptr<SocketState> _state) : state_(std::move(_state))
{
}

void usv_comm::TCPStream::connect(const std::string &_address, std::uint16_t _port, int _timeout_ms)
{
    const auto _deadline = detail::makeDeadline(_timeout_ms);
    const auto _endpoint = detail::makeAddress(_address, _port);
    if (_port == 0 || state_)
    {
        throw CommunicationError(
            ErrorCode::INVALID_ARGUMENT, "Connect needs a nonzero port and closed stream");
    }
    auto _candidate = detail::makeSocket(SOCK_STREAM);
    if (::connect(_candidate->handle_, reinterpret_cast<const sockaddr *>(&_endpoint), sizeof(_endpoint)) !=
        0)
    {
        const int _error = detail::lastError();
        if (!detail::isConnecting(_error))
        {
            detail::failSystem("connect", _error);
        }
        detail::waitReady(_candidate->handle_, true, _deadline);
        int _connect_error = 0;
        detail::AddressLength _length = sizeof(_connect_error);
        if (getsockopt(_candidate->handle_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&_connect_error),
                &_length) != 0)
        {
            detail::failSystem("getsockopt", detail::lastError());
        }
        if (_connect_error != 0)
        {
            detail::failSystem("connect", _connect_error);
        }
    }
    state_ = std::move(_candidate);
}

void usv_comm::TCPStream::sendFrame(const std::vector<std::uint8_t> &_data_vector, int _timeout_ms)
{
    const auto _deadline = detail::makeDeadline(_timeout_ms);
    const auto _handle = detail::requireHandle(state_);
    if (_data_vector.size() > MAX_FRAME_BYTES)
    {
        throw CommunicationError(ErrorCode::FRAME_TOO_LARGE, "TCP frame exceeds 1048576 bytes");
    }
    const auto _size = static_cast<std::uint32_t>(_data_vector.size());
    std::array<std::uint8_t, 4> _header_array = {};
    _header_array[0] = static_cast<std::uint8_t>(_size >> 24);
    _header_array[1] = static_cast<std::uint8_t>(_size >> 16);
    _header_array[2] = static_cast<std::uint8_t>(_size >> 8);
    _header_array[3] = static_cast<std::uint8_t>(_size);
    try
    {
        detail::sendAll(_handle, _header_array.data(), _header_array.size(), _deadline);
        detail::sendAll(_handle, _data_vector.data(), _data_vector.size(), _deadline);
    }
    catch (...)
    {
        close();
        throw;
    }
}

std::vector<std::uint8_t> usv_comm::TCPStream::receiveFrame(int _timeout_ms)
{
    const auto _deadline = detail::makeDeadline(_timeout_ms);
    const auto _handle = detail::requireHandle(state_);
    try
    {
        std::array<std::uint8_t, 4> _header_array = {};
        detail::receiveAll(_handle, _header_array.data(), _header_array.size(), _deadline);
        const std::uint32_t _size = (static_cast<std::uint32_t>(_header_array[0]) << 24) |
            (static_cast<std::uint32_t>(_header_array[1]) << 16) |
            (static_cast<std::uint32_t>(_header_array[2]) << 8) | _header_array[3];
        if (_size > MAX_FRAME_BYTES)
        {
            throw CommunicationError(ErrorCode::FRAME_TOO_LARGE, "Received TCP frame exceeds 1048576 bytes");
        }
        std::vector<std::uint8_t> _data_vector(_size);
        detail::receiveAll(_handle, _data_vector.data(), _data_vector.size(), _deadline);
        return _data_vector;
    }
    catch (...)
    {
        close();
        throw;
    }
}

void usv_comm::TCPStream::close() noexcept
{
    state_.reset();
}

usv_comm::TCPServer::TCPServer() = default;
usv_comm::TCPServer::~TCPServer() = default;
usv_comm::TCPServer::TCPServer(TCPServer &&_other) noexcept = default;
usv_comm::TCPServer &usv_comm::TCPServer::operator=(TCPServer &&_other) noexcept = default;

void usv_comm::TCPServer::bindAndListen(const std::string &_address, std::uint16_t _port)
{
    const auto _endpoint = detail::makeAddress(_address, _port);
    if (state_)
    {
        throw CommunicationError(ErrorCode::INVALID_ARGUMENT, "TCP server is already listening");
    }
    auto _candidate = detail::makeSocket(SOCK_STREAM);
    if (::bind(_candidate->handle_, reinterpret_cast<const sockaddr *>(&_endpoint), sizeof(_endpoint)) != 0)
    {
        detail::failSystem("bind", detail::lastError());
    }
    if (::listen(_candidate->handle_, SOMAXCONN) != 0)
    {
        detail::failSystem("listen", detail::lastError());
    }
    state_ = std::move(_candidate);
}

std::uint16_t usv_comm::TCPServer::localPort() const
{
    return detail::readLocalPort(detail::requireHandle(state_));
}

usv_comm::TCPStream usv_comm::TCPServer::accept(int _timeout_ms)
{
    const auto _deadline = detail::makeDeadline(_timeout_ms);
    const auto _handle = detail::requireHandle(state_);
    auto _accepted_state = std::make_unique<SocketState>();
    while (true)
    {
        detail::waitReady(_handle, false, _deadline);
        _accepted_state->handle_ = ::accept(_handle, nullptr, nullptr);
        if (_accepted_state->handle_ != detail::INVALID_HANDLE)
        {
            detail::setNonblocking(_accepted_state->handle_);
            return TCPStream(std::move(_accepted_state));
        }
        const int _error = detail::lastError();
        if (!detail::shouldRetry(_error))
        {
            detail::failSystem("accept", _error);
        }
    }
}

void usv_comm::TCPServer::close() noexcept
{
    state_.reset();
}
