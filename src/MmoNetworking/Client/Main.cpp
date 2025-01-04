﻿#include <Client/Pch.hpp>
#include <Client/Service.hpp>

int main() 
{
    try
    {
        Client::Service service(1, 60);
        service.Start("127.0.0.1", "60000");
        
        service.JoinWorkers();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
