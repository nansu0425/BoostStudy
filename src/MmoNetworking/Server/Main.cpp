#include <Server/Pch.hpp>
#include <Server/Service.hpp>

int main()
{
    Server::Service service(60000);

    service.Start();

    while (service.Update())
    { }

    return 0;
}
