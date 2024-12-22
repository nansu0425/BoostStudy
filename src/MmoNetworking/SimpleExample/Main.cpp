// SimpleExample/Main.cpp

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#ifdef _WIN32
#define WINVER          0x0A00
#define _WIN32_WINNT    0x0A00
#endif // _WIN32

#define ASIO_STANDALONE
#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>

using boost::asio::ip::tcp;

void HandleReadSome(tcp::socket& socket,
                    const boost::system::error_code& errorCode,
                    const size_t nBytesTransferred);

std::vector<char> g_readBuffer(1024);

void StartReadSome(tcp::socket& socket)
{
    socket.async_read_some(boost::asio::buffer(g_readBuffer),
                           [&socket](const boost::system::error_code& errorCode,
                                     const size_t nBytesTransferred)
                           {
                               HandleReadSome(socket, errorCode, nBytesTransferred);
                           });
}

void HandleReadSome(tcp::socket& socket, 
                    const boost::system::error_code& errorCode,
                    const size_t nBytesTransferred)
{
    if (!errorCode)
    {
        std::cout << "\n\nThe number of bytes to be read: " << nBytesTransferred << "\n\n";

        for (int iReadBuffer = 0; iReadBuffer < nBytesTransferred; ++iReadBuffer)
        {
            std::cout.put(g_readBuffer[iReadBuffer]);
        }
    }

    StartReadSome(socket);
}

// #define SYNC_READ_SOME
#define ASYNC_READ_SOME

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "argv[1]: an ip address of the endpoint" << std::endl;
        return 1;
    }

    try
    {
        boost::asio::io_context ioContext;
        auto workGuard = boost::asio::make_work_guard(ioContext);

        std::thread worker([&ioContext]()
                           {
                               ioContext.run();
                           });
        
        tcp::endpoint endpoint(boost::asio::ip::make_address(argv[1]), 80);
        tcp::socket socket(ioContext);

        socket.connect(endpoint);
        std::cout << "Connected!" << std::endl;

#ifdef ASYNC_READ_SOME
        StartReadSome(socket);
#endif // ASYNC_READ_SOME

        std::string request =
            "GET /index.html HTTP/1.1\r\n"
            "Host: david-barr.co.uk\r\n"
            "Connection: close\r\n\r\n";

        socket.write_some(boost::asio::buffer(request));

#ifdef SYNC_READ_SOME
        socket.wait(socket.wait_read);

        size_t nBytesRead = socket.available();
        std::cout << "The number of bytes that may be raed: " << nBytesRead << std::endl;

        if (nBytesRead > 0)
        {
            std::vector<char> readBuffer(nBytesRead);

            socket.read_some(boost::asio::buffer(readBuffer));
            for (char c : readBuffer)
                std::cout.put(c);
        }
#endif // SYNC_READ_SOME

        std::this_thread::sleep_for(std::chrono::seconds(5));

        ioContext.stop();
        if (worker.joinable())
        {
            worker.join();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
