#include <Client/Pch.hpp>
#include <Client/Service.hpp>

int main() 
{
    try
    {
        Client::Service service;
        service.Connect("127.0.0.1", "60000");

        while (service.Update())
        {}
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
