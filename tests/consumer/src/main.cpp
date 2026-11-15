#include <usv_comm/communication.h>

#include <exception>
#include <iostream>

int main()
{
    try
    {
        usv_comm::UDPSocket _socket;
        _socket.bind("127.0.0.1", 0);
        if (_socket.localPort() == 0)
        {
            std::cerr << "Installed shared library did not bind a port\n";
            return 1;
        }
        std::cout << "Installed usv_comm shared library bound loopback port " << _socket.localPort() << '\n';
        return 0;
    }
    catch (const std::exception &_error)
    {
        std::cerr << _error.what() << '\n';
        return 1;
    }
}
