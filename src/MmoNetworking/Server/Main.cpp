#include <Server/Pch.hpp>
#include <Server/Service.hpp>

int main()
{
    try
    {
        Server::Service service(60000);
        service.Start();

        while (service.Update())
        {}
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
