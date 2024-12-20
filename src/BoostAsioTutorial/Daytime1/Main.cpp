// Daytime.1 - A synchronous TCP daytime client

#include <array>
#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

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
        
        // host 이름과 서비스 이름을 엔드 포인트의 IP번호와 포트 번호로 변환
        // 엔드 포인트 리스트 형태로 저장되고 IPv4, Ipv6 엔트리 모두 존재할 수 있다
        tcp::resolver resolver(ioContext);
        tcp::resolver::results_type endpoints = resolver.resolve(argv[1], "Daytime");
        
        // 소켓을 생성하고 엔드 포인트에 연결한다
        // 모든 엔드 포인트에 대해 연결을 시도한다
        tcp::socket socket(ioContext);
        boost::asio::connect(socket, endpoints);

        while (true)
        {
            std::array<char, 128> buffer;
            boost::system::error_code error;

            size_t nBytesRead = socket.read_some(boost::asio::buffer(buffer), error);

            // 서버가 연결을 종료할 경우 error는 eof
            if (error == boost::asio::error::eof)
            {
                break;
            }
            else if (error)
            {
                throw boost::system::system_error(error);
            }

            std::cout.write(buffer.data(), nBytesRead);
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << std::endl;
    }

    return 0;
}
