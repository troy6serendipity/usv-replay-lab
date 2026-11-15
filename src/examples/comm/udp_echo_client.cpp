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
            throw std::invalid_argument("Usage: udp_echo_client PORT [MESSAGE]");
        }
        const std::uint16_t _port = usv_comm_examples::parsePort(_argument_array[1], false);
        const std::string _message = _argument_count == 3 ? _argument_array[2] : "usv heartbeat";
        const std::vector<std::uint8_t> _message_vector(_message.begin(), _message.end());
        usv_comm::UDPSocket _client;
        _client.bind("127.0.0.1", 0);
        _client.sendTo("127.0.0.1", _port, _message_vector, usv_comm_examples::IO_TIMEOUT_MS);
        const auto _reply = _client.receive(usv_comm_examples::IO_TIMEOUT_MS);
        if (_reply.address_ != "127.0.0.1" || _reply.port_ != _port || _reply.data_vector_ != _message_vector)
        {
            throw std::runtime_error("UDP reply differs from the sent message or endpoint");
        }
        std::cout << "Verified " << _reply.data_vector_.size() << " UDP echo bytes\n";
        return 0;
    }
    catch (const std::exception &_error)
    {
        std::cerr << _error.what() << '\n';
        return 1;
    }
}
