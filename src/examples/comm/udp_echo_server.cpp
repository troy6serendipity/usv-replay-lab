#include <usv_comm_examples/example_args.h>

#include <usv_comm/communication.h>

#include <exception>
#include <iostream>

int main(int _argument_count, char *_argument_array[])
{
    try
    {
        if (_argument_count != 2)
        {
            throw std::invalid_argument("Usage: udp_echo_server PORT (0 selects an available port)");
        }
        usv_comm::UDPSocket _server;
        _server.bind("127.0.0.1", usv_comm_examples::parsePort(_argument_array[1], true));
        std::cout << "Listening on 127.0.0.1:" << _server.localPort() << std::endl;
        const auto _datagram = _server.receive(usv_comm_examples::IO_TIMEOUT_MS);
        _server.sendTo(
            _datagram.address_, _datagram.port_, _datagram.data_vector_, usv_comm_examples::IO_TIMEOUT_MS);
        std::cout << "Echoed " << _datagram.data_vector_.size() << " UDP bytes\n";
        return 0;
    }
    catch (const std::exception &_error)
    {
        std::cerr << _error.what() << '\n';
        return 1;
    }
}
