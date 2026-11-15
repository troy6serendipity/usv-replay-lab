#ifndef USV_COMM_COMMUNICATION_H_
#define USV_COMM_COMMUNICATION_H_

#include "export.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace usv_comm
{
    inline constexpr std::size_t MAX_FRAME_BYTES = 1024 * 1024;
    inline constexpr std::size_t MAX_DATAGRAM_BYTES = 65507;

    enum class ErrorCode
    {
        TIMEOUT,
        CLOSED,
        INVALID_ARGUMENT,
        SYSTEM,
        FRAME_TOO_LARGE
    };

#if defined(_MSC_VER)
    // The public C++ API requires a matching MSVC runtime in the consumer.
#pragma warning(push)
#pragma warning(disable : 4275)
#endif
    class USV_COMM_API CommunicationError : public std::runtime_error
    {
    public:
        CommunicationError(ErrorCode _code, const std::string &_message);
        ErrorCode getCode() const noexcept;

    private:
        ErrorCode code_;
    };
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    struct Datagram
    {
        std::vector<std::uint8_t> data_vector_;
        std::string address_;
        std::uint16_t port_ = 0;
    };

    struct SocketState;

    // Each object has one owner. Serialize calls on the same object.
    // IPv4 numeric addresses are required. All operation timeouts must be > 0.
    class UDPSocket
    {
    public:
        USV_COMM_API UDPSocket();
        USV_COMM_API ~UDPSocket();
        USV_COMM_API UDPSocket(UDPSocket &&_other) noexcept;
        USV_COMM_API UDPSocket &operator=(UDPSocket &&_other) noexcept;
        UDPSocket(const UDPSocket &) = delete;
        UDPSocket &operator=(const UDPSocket &) = delete;

        USV_COMM_API void bind(const std::string &_address, std::uint16_t _port);
        USV_COMM_API std::uint16_t localPort() const;
        USV_COMM_API void sendTo(const std::string &_address, std::uint16_t _port,
            const std::vector<std::uint8_t> &_data_vector, int _timeout_ms = 1000);
        USV_COMM_API Datagram receive(int _timeout_ms = 1000);
        USV_COMM_API void close() noexcept;

    private:
        std::unique_ptr<SocketState> state_;
    };

    class TCPServer;

    // Frames use a 4-byte big-endian length followed by that many payload bytes.
    // Frame I/O failures or timeouts close the stream, preserving frame boundaries.
    // Argument validation failures preserve the stream.
    class TCPStream
    {
    public:
        USV_COMM_API TCPStream();
        USV_COMM_API ~TCPStream();
        USV_COMM_API TCPStream(TCPStream &&_other) noexcept;
        USV_COMM_API TCPStream &operator=(TCPStream &&_other) noexcept;
        TCPStream(const TCPStream &) = delete;
        TCPStream &operator=(const TCPStream &) = delete;

        USV_COMM_API void connect(const std::string &_address, std::uint16_t _port, int _timeout_ms = 1000);
        USV_COMM_API void sendFrame(const std::vector<std::uint8_t> &_data_vector, int _timeout_ms = 1000);
        USV_COMM_API std::vector<std::uint8_t> receiveFrame(int _timeout_ms = 1000);
        USV_COMM_API void close() noexcept;

    private:
        friend class TCPServer;
        explicit TCPStream(std::unique_ptr<SocketState> _state);
        std::unique_ptr<SocketState> state_;
    };

    class TCPServer
    {
    public:
        USV_COMM_API TCPServer();
        USV_COMM_API ~TCPServer();
        USV_COMM_API TCPServer(TCPServer &&_other) noexcept;
        USV_COMM_API TCPServer &operator=(TCPServer &&_other) noexcept;
        TCPServer(const TCPServer &) = delete;
        TCPServer &operator=(const TCPServer &) = delete;

        USV_COMM_API void bindAndListen(const std::string &_address, std::uint16_t _port);
        USV_COMM_API std::uint16_t localPort() const;
        USV_COMM_API TCPStream accept(int _timeout_ms = 1000);
        USV_COMM_API void close() noexcept;

    private:
        std::unique_ptr<SocketState> state_;
    };
} // namespace usv_comm

#endif // USV_COMM_COMMUNICATION_H_
