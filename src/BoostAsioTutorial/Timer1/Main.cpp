#include <iostream>
#include <boost/asio.hpp>

int main()
{
    boost::asio::io_context ioContext;
    
    boost::asio::steady_timer timer(ioContext, boost::asio::chrono::seconds(5));
    timer.wait();

    std::cout << "Hello, world!" << std::endl;

    return 0;
}
