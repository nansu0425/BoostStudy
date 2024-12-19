#include <iostream>
#include <boost/asio.hpp>

class Printer
{
public:
    Printer(boost::asio::io_context& ioContext)
        : _timer(ioContext, boost::asio::chrono::seconds(1))
        , _count(0)
    {
        _timer.async_wait([this](const boost::system::error_code& ec)
                          {
                              Print();
                          });
    }

    ~Printer()
    {
        std::cout << "Final count is " << _count << std::endl;
    }

    void Print()
    {
        if (_count < 5)
        {
            std::cout << _count << std::endl;
            ++_count;

            _timer.expires_at(_timer.expiry() + boost::asio::chrono::seconds(1));
            _timer.async_wait([this](const boost::system::error_code& ec)
                              {
                                  Print();
                              });
        }
    }

private:
    boost::asio::steady_timer _timer;
    int _count;
};

int main()
{
    boost::asio::io_context ioContext;
    Printer printer(ioContext);

    ioContext.run();

    return 0;
}
