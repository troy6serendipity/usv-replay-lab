#include <usv_comm/communication.h>

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(!std::is_copy_constructible_v<usv_comm::UDPSocket>);
static_assert(!std::is_copy_constructible_v<usv_comm::TCPStream>);
static_assert(!std::is_copy_constructible_v<usv_comm::TCPServer>);
static_assert(std::is_nothrow_move_constructible_v<usv_comm::UDPSocket>);
static_assert(std::is_nothrow_move_constructible_v<usv_comm::TCPStream>);
static_assert(std::is_nothrow_move_constructible_v<usv_comm::TCPServer>);

namespace usv_comm_tests
{
    constexpr int IO_TIMEOUT_MS = 3000;
    constexpr int SHORT_TIMEOUT_MS = 25;
    constexpr std::size_t LARGE_MESSAGE_BYTES = 256 * 1024;

    void require(bool _condition, const char *_message);
    void expectError(usv_comm::ErrorCode _code, const std::function<void()> &_action);
    void verifyUDP();
    void verifyTCPFrames();
    void verifyTCPTimeout();
    void verifyInvalidAddress();
} // namespace usv_comm_tests

void usv_comm_tests::require(bool _condition, const char *_message)
{
    if (!_condition)
    {
        throw std::runtime_error(_message);
    }
}

void usv_comm_tests::expectError(usv_comm::ErrorCode _code, const std::function<void()> &_action)
{
    try
    {
        _action();
    }
    catch (const usv_comm::CommunicationError &_error)
    {
        require(_error.getCode() == _code, "Unexpected communication error category");
        return;
    }
    throw std::runtime_error("Expected CommunicationError was not thrown");
}

void usv_comm_tests::verifyUDP()
{
    usv_comm::UDPSocket _receiver;
    _receiver.bind("127.0.0.1", 0);
    require(_receiver.localPort() != 0, "UDP did not bind an available port");
    expectError(usv_comm::ErrorCode::TIMEOUT,
        [&_receiver]()
        {
            _receiver.receive(SHORT_TIMEOUT_MS);
        });
    expectError(usv_comm::ErrorCode::INVALID_ARGUMENT,
        [&_receiver]()
        {
            _receiver.receive(0);
        });

    usv_comm::UDPSocket _sender;
    _sender.bind("127.0.0.1", 0);
    const std::uint16_t _sender_port = _sender.localPort();
    usv_comm::UDPSocket _moved_sender(std::move(_sender));
    require(_moved_sender.localPort() == _sender_port, "UDP move lost the endpoint");

    std::vector<std::uint8_t> _binary_vector(256);
    for (std::size_t _index = 0; _index < _binary_vector.size(); ++_index)
    {
        _binary_vector[_index] = static_cast<std::uint8_t>(_index);
    }
    _moved_sender.sendTo("127.0.0.1", _receiver.localPort(), _binary_vector, IO_TIMEOUT_MS);
    const auto _datagram = _receiver.receive(IO_TIMEOUT_MS);
    require(_datagram.data_vector_ == _binary_vector, "Binary UDP payload changed");
    require(_datagram.address_ == "127.0.0.1" && _datagram.port_ == _sender_port,
        "UDP sender endpoint is incorrect");

    usv_comm::UDPSocket _unbound_sender;
    _unbound_sender.sendTo("127.0.0.1", _receiver.localPort(), _binary_vector, IO_TIMEOUT_MS);
    require(_receiver.receive(IO_TIMEOUT_MS).data_vector_ == _binary_vector,
        "UDP automatic local binding failed");
    require(_unbound_sender.localPort() != 0, "UDP automatic binding did not assign a port");

    usv_comm::UDPSocket _assigned_sender;
    _assigned_sender = std::move(_moved_sender);
    const std::vector<std::uint8_t> _empty_vector;
    _assigned_sender.sendTo("127.0.0.1", _receiver.localPort(), _empty_vector, IO_TIMEOUT_MS);
    require(_receiver.receive(IO_TIMEOUT_MS).data_vector_.empty(), "Empty UDP datagram was lost");
    const std::vector<std::uint8_t> _oversized_vector(usv_comm::MAX_DATAGRAM_BYTES + 1);
    expectError(usv_comm::ErrorCode::FRAME_TOO_LARGE,
        [&]()
        {
            _assigned_sender.sendTo("127.0.0.1", _receiver.localPort(), _oversized_vector, IO_TIMEOUT_MS);
        });
    _assigned_sender.close();
    _assigned_sender.close();
    expectError(usv_comm::ErrorCode::CLOSED,
        [&_assigned_sender]()
        {
            _assigned_sender.receive(SHORT_TIMEOUT_MS);
        });
    std::cout << "PASS UDP binary, empty, endpoint, timeout, move, limit and close\n";
}

void usv_comm_tests::verifyTCPFrames()
{
    usv_comm::TCPServer _server;
    _server.bindAndListen("127.0.0.1", 0);
    const std::uint16_t _port = _server.localPort();
    usv_comm::TCPServer _moved_server(std::move(_server));
    require(_moved_server.localPort() == _port, "TCP server move lost the endpoint");
    expectError(usv_comm::ErrorCode::TIMEOUT,
        [&_moved_server]()
        {
            _moved_server.accept(SHORT_TIMEOUT_MS);
        });

    usv_comm::TCPStream _connecting_client;
    _connecting_client.connect("127.0.0.1", _port, IO_TIMEOUT_MS);
    usv_comm::TCPStream _client(std::move(_connecting_client));
    usv_comm::TCPStream _peer;
    _peer = _moved_server.accept(IO_TIMEOUT_MS);
    const std::vector<std::uint8_t> _oversized_vector(usv_comm::MAX_FRAME_BYTES + 1);
    expectError(usv_comm::ErrorCode::FRAME_TOO_LARGE,
        [&]()
        {
            _client.sendFrame(_oversized_vector, IO_TIMEOUT_MS);
        });

    std::vector<std::uint8_t> _large_vector(LARGE_MESSAGE_BYTES);
    for (std::size_t _index = 0; _index < _large_vector.size(); ++_index)
    {
        _large_vector[_index] = static_cast<std::uint8_t>(_index % 251);
    }
    std::vector<std::vector<std::uint8_t>> _message_vector;
    _message_vector.push_back(_large_vector);
    _message_vector.emplace_back();
    _message_vector.emplace_back(17, 0xA5);

    std::exception_ptr _worker_error;
    std::thread _worker(
        [&]()
        {
            try
            {
                for (const auto &_payload_vector : _message_vector)
                {
                    _client.sendFrame(_payload_vector, IO_TIMEOUT_MS);
                }
                for (const auto &_payload_vector : _message_vector)
                {
                    require(
                        _client.receiveFrame(IO_TIMEOUT_MS) == _payload_vector, "TCP echo changed a frame");
                }
            }
            catch (...)
            {
                _worker_error = std::current_exception();
            }
            _client.close();
        });

    std::exception_ptr _receiver_error;
    try
    {
        for (const auto &_expected_vector : _message_vector)
        {
            const auto _received_vector = _peer.receiveFrame(IO_TIMEOUT_MS);
            require(_received_vector == _expected_vector, "TCP frame boundary or binary payload changed");
            _peer.sendFrame(_received_vector, IO_TIMEOUT_MS);
        }
        expectError(usv_comm::ErrorCode::CLOSED,
            [&_peer]()
            {
                _peer.receiveFrame(IO_TIMEOUT_MS);
            });
    }
    catch (...)
    {
        _receiver_error = std::current_exception();
        _peer.close();
    }
    _worker.join();
    if (_worker_error)
    {
        std::rethrow_exception(_worker_error);
    }
    if (_receiver_error)
    {
        std::rethrow_exception(_receiver_error);
    }
    std::cout
        << "PASS TCP large binary, empty and multiple frames, move, limit, accept timeout and peer close\n";
}

void usv_comm_tests::verifyTCPTimeout()
{
    usv_comm::TCPServer _server;
    _server.bindAndListen("127.0.0.1", 0);
    usv_comm::TCPStream _client;
    _client.connect("127.0.0.1", _server.localPort(), IO_TIMEOUT_MS);
    auto _peer = _server.accept(IO_TIMEOUT_MS);
    expectError(usv_comm::ErrorCode::TIMEOUT,
        [&_peer]()
        {
            _peer.receiveFrame(SHORT_TIMEOUT_MS);
        });
    expectError(usv_comm::ErrorCode::CLOSED,
        [&_peer]()
        {
            _peer.receiveFrame(SHORT_TIMEOUT_MS);
        });
    std::cout << "PASS TCP receive deadline invalidates the stream\n";
}

void usv_comm_tests::verifyInvalidAddress()
{
    usv_comm::UDPSocket _udp;
    expectError(usv_comm::ErrorCode::INVALID_ARGUMENT,
        [&_udp]()
        {
            _udp.bind("invalid-ipv4", 0);
        });
    usv_comm::TCPStream _tcp;
    expectError(usv_comm::ErrorCode::INVALID_ARGUMENT,
        [&_tcp]()
        {
            _tcp.connect("invalid-ipv4", 12345, IO_TIMEOUT_MS);
        });
    usv_comm::TCPServer _server;
    expectError(usv_comm::ErrorCode::INVALID_ARGUMENT,
        [&_server]()
        {
            _server.bindAndListen("invalid-ipv4", 0);
        });
    std::cout << "PASS invalid IPv4 addresses rejected\n";
}

int main()
{
    try
    {
        usv_comm_tests::verifyUDP();
        usv_comm_tests::verifyTCPFrames();
        usv_comm_tests::verifyTCPTimeout();
        usv_comm_tests::verifyInvalidAddress();
        return 0;
    }
    catch (const std::exception &_error)
    {
        std::cerr << "FAIL " << _error.what() << '\n';
        return 1;
    }
}
