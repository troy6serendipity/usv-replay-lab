#include <usv_comm_examples/example_args.h>

#include <usv_comm/communication.h>

#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int _argument_count, char *_argument_array[])
{
    try
    {
        if (_argument_count < 2 || _argument_count > 3)
        {
            throw std::invalid_argument("Usage: tcp_echo_client PORT [MESSAGE]");
        }
        const std::uint16_t _port = usv_comm_examples::parsePort(_argument_array[1], false);
        const std::string _message = _argument_count == 3 ? _argument_array[2] : "usv heartbeat";
        const std::vector<std::uint8_t> _message_vector(_message.begin(), _message.end());
        usv_comm::TCPStream _client;
        _client.connect("127.0.0.1", _port, usv_comm_examples::IO_TIMEOUT_MS);
        _client.sendFrame(_message_vector, usv_comm_examples::IO_TIMEOUT_MS);
        const auto _reply_vector = _client.receiveFrame(usv_comm_examples::IO_TIMEOUT_MS);
        if (_reply_vector != _message_vector)
        {
            throw std::runtime_error("TCP reply differs from the sent message");
        }
        std::cout << "Verified " << _reply_vector.size() << " TCP echo bytes\n";
        return 0;
    }
    catch (const std::exception &_error)
    {
        std::cerr << _error.what() << '\n';
        return 1;
    }
}
