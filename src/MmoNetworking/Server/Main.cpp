﻿#include <Server/Pch.hpp>
#include <Server/Server.hpp>

int main()
{
    Server server(60000);
    server.Start();

    while (true)
    {
        server.Update();
    }

    return 0;
}