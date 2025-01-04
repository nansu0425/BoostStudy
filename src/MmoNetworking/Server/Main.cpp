#include <Server/Pch.hpp>
#include <Server/Service.hpp>

int main()
{
    try
    {
        Server::Service service(std::thread::hardware_concurrency(), 60, 60000);
        service.Start();

        service.JoinWorkers();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
