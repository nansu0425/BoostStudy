// Daytime.4 - A synchronous UDP daytime client

#include <array>
#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::udp;

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: client <host>" << std::endl;
            return 1;
        }

        boost::asio::io_context ioContext;

        // 엔드 포인트 리스트는 iterator로 순회할 수 있다
        // resolve가 실패하지 않는다면 엔드 포인트 리스트엔 적어도 하나의 엔드 포인트가 존재한다
        // udp::v4()로 인해 IPv6 엔드 포인트는 만들지 않는다
        udp::resolver resolver(ioContext);
        udp::endpoint receiverEndpoint = *resolver.resolve(udp::v4(), argv[1], "daytime").begin();

        udp::socket socket(ioContext);
        socket.open(udp::v4());

        // 엔드 포인트에 데이터그램 전송
        std::array<char, 1> sendBuffer = {0};
        socket.send_to(boost::asio::buffer(sendBuffer), receiverEndpoint);

        // 서버가 전송하는 데이터를 받을 준비를 한다
        // senderEndpoint는 receive_from에서 초기화된다
        std::array<char, 128> receiveBuffer;
        udp::endpoint senderEndPoint;
        size_t nBytesReceived = socket.receive_from(boost::asio::buffer(receiveBuffer), 
                                                    senderEndPoint);

        std::cout.write(receiveBuffer.data(), nBytesReceived);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
