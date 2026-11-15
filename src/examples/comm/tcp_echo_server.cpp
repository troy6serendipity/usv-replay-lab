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
            throw std::invalid_argument("Usage: tcp_echo_server PORT (0 selects an available port)");
        }
        usv_comm::TCPServer _server;
        _server.bindAndListen("127.0.0.1", usv_comm_examples::parsePort(_argument_array[1], true));
        std::cout << "Listening on 127.0.0.1:" << _server.localPort() << std::endl;
        auto _stream = _server.accept(usv_comm_examples::IO_TIMEOUT_MS);
        const auto _message_vector = _stream.receiveFrame(usv_comm_examples::IO_TIMEOUT_MS);
        _stream.sendFrame(_message_vector, usv_comm_examples::IO_TIMEOUT_MS);
        std::cout << "Echoed " << _message_vector.size() << " TCP frame bytes\n";
        return 0;
    }
    catch (const std::exception &_error)
    {
        std::cerr << _error.what() << '\n';
        return 1;
    }
}
