// Daytime.6 - An asynchronous UDP daytime server

#include <array>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::udp;

std::string MakeDaytimeString()
{
    std::time_t now = std::time(0);
    return std::ctime(&now);
}

class UdpServer
{
public:
    UdpServer(boost::asio::io_context& ioContext)
        : _socket(ioContext, udp::endpoint(udp::v4(), 13))
    {
        // 비동기 수신 작업 시작
        StartReceive();
    }

private:
    void StartReceive()
    { 
        // remoteEndpoint를 데이터그램을 전송한 클라이언트로 설정하고 HandleReceive 호출
        _socket.async_receive_from(boost::asio::buffer(_receiveBuffer),
                                   _remoteEndpoint,
                                   [this](const boost::system::error_code& error,
                                          const size_t nBytesTransferred)
                                   {
                                       HandleReceive(error, nBytesTransferred);
                                   });
    }

    void HandleReceive(const boost::system::error_code& error,
                       const size_t nBytesTransferred)
    {
        if (!error)
        {
            auto pMessage = std::make_shared<std::string>(MakeDaytimeString());

            // 클라이언트에게 message 비동기 전송 작업 시작
            // 람다 함수가 message의 shared_ptr를 복사하여 람다 함수 종료될 때 message 소멸
            _socket.async_send_to(boost::asio::buffer(*pMessage),
                                  _remoteEndpoint,
                                  [this, pMessage](const boost::system::error_code& error,
                                                   const size_t nBytesTransferred)
                                  {
                                      OnSendStarted(pMessage, error, nBytesTransferred);
                                  });
        }

        // 다시 비동기 수신 작업 시작
        StartReceive();
    }

    void OnSendStarted(std::shared_ptr<std::string> pMessage,
                    const boost::system::error_code& error,
                    const size_t nBytesTransferred)
    {
        if (!error)
        {
            std::cout << "[message: " << *pMessage
                      << "nBytesTransferred: " << nBytesTransferred << "]" << std::endl;
        }
    }

private:
    udp::socket _socket;
    udp::endpoint _remoteEndpoint;
    std::array<char, 1> _receiveBuffer;
};

int main()
{
    try
    {
        boost::asio::io_context ioContext;
        UdpServer udpServer(ioContext);

        // 현재 스레드가 모든 비동기 작업의 완료 핸들러를 처리
        ioContext.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
