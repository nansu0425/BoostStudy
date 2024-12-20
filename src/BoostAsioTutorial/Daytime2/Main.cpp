// Daytime.2 - A synchronous TCP daytime server

#include <ctime>
#include <iostream>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::string MakeDaytimeString()
{
    std::time_t now = std::time(0);

    return std::ctime(&now);
}

int main()
{
    try
    {
        boost::asio::io_context ioContext;

        // 새로운 연결에 대한 listener 역할
        // TCP port 13에 대한 연결을 받고, IPv4를 사용한다
        tcp::acceptor acceptor(ioContext, tcp::endpoint(tcp::v4(), 13));

        while (true)
        {
            // 반복할 때마다 하나의 하나의 연결을 받고 끊는다
            // socket은 클라이언트와 서버 사이의 TCP 연결을 의미
            tcp::socket socket(ioContext);
            acceptor.accept(socket);

            std::string message = MakeDaytimeString();

            boost::system::error_code ignoredError;
            boost::asio::write(socket, boost::asio::buffer(message), ignoredError);
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
    }

    return 0;
}
