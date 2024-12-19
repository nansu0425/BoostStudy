#include <thread>
#include <functional>
#include <iostream>
#include <boost/asio.hpp>

thread_local int t_threadId = 0;

class Printer
{
public:
    Printer(boost::asio::io_context& ioContext)
        : _strand(boost::asio::make_strand(ioContext))
        , _timer1(ioContext, boost::asio::chrono::seconds(1))
        , _timer2(ioContext, boost::asio::chrono::seconds(1))
        , _count(0)
    {
        _timer1.async_wait(boost::asio::bind_executor(_strand,
                                                      std::bind(&Printer::Print1, this)));
        _timer2.async_wait(boost::asio::bind_executor(_strand,
                                                      std::bind(&Printer::Print2, this)));
    }

    ~Printer()
    {
        std::cout << "Thread Id: " << t_threadId << ", Final count is " << _count << std::endl;
    }

    void Print1()
    {
        if (_count < 10)
        {
            std::cout << "Thread Id: " << t_threadId << ", Timer1: " << _count << std::endl;
            ++_count;

            _timer1.expires_at(_timer1.expiry() + boost::asio::chrono::seconds(1));
            _timer1.async_wait(boost::asio::bind_executor(_strand,
                                                          std::bind(&Printer::Print1, this)));
        }
    }

    void Print2()
    {
        if (_count < 10)
        {
            std::cout << "Thread Id: " << t_threadId << ", Timer2: " << _count << std::endl;
            ++_count;

            _timer2.expires_at(_timer2.expiry() + boost::asio::chrono::seconds(1));
            _timer2.async_wait(boost::asio::bind_executor(_strand,
                                                          std::bind(&Printer::Print2, this)));
        }
    }

private:
    // strand에 할당된 핸들러가 호출되면 처리가 끝날 때까지 같은 strand에 할당된 다른 핸들러들이 호출되지 않는다
    boost::asio::strand<boost::asio::io_context::executor_type> _strand;
    // timer 비동기 요청 완료 시 어떤 스레드가 핸들러를 처리할지 알 수 없다
    boost::asio::steady_timer _timer1;
    boost::asio::steady_timer _timer2;
    // 핸들러 공유 자원이므로 동시에 접근하지 못 하도록 동기화해야 한다
    int _count; 
};

int main()
{
    boost::asio::io_context ioContext;
    Printer printer(ioContext);

    std::thread t1([&]()
                   {
                       t_threadId = 1;
                       ioContext.run();
                       std::cout << "Thread Id: " << t_threadId 
                                 << ", Completed all asynchronous operations." << std::endl;
                   });

    std::thread t2([&]()
                   {
                       t_threadId = 2;
                       ioContext.run();
                       std::cout << "Thread Id: " << t_threadId
                                 << ", Completed all asynchronous operations." << std::endl;
                   });

    t1.join();
    t2.join();

    return 0;
}
