// Daytime.5 - A synchronous UDP daytime server

#include <array>
#include <ctime>
#include <iostream>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::udp;

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
        
        udp::socket socket(ioContext, udp::endpoint(udp::v4(), 13));

        while (true)
        {
            std::array<char, 1> receiveBuffer;

            // 클라이언트가 보낸 데이터그램을 receive_from으로 받으면 remoteEndpoint를 클라이언트로 설정한다
            udp::endpoint remoteEndpoint;
            socket.receive_from(boost::asio::buffer(receiveBuffer),
                                remoteEndpoint);

            std::string message = MakeDaytimeString();

            // 클라이언트에 message를 전송한다
            boost::system::error_code ignoredError;
            socket.send_to(boost::asio::buffer(message),
                           remoteEndpoint,
                           0,
                           ignoredError);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
