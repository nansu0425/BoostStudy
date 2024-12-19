#include <functional>
#include <iostream>
#include <boost/asio.hpp>

void Print(boost::asio::steady_timer* pTimer, int* pCount)
{
    if (*pCount < 5)
    {
        std::cout << *pCount << std::endl;
        ++(*pCount);
        
        // timer의 만료 시점을 현재 만료 시점으로부터 1초 뒤로 설정
        pTimer->expires_at(pTimer->expiry() + boost::asio::chrono::seconds(1));
        pTimer->async_wait([=](const boost::system::error_code& ec)
                           {
                               Print(pTimer, pCount);
                           });
    }
}

int main()
{
    boost::asio::io_context ioContext;

    int count = 0;

    // 현재 시점으로부터 1초 뒤에 timer는 만료된다
    boost::asio::steady_timer timer(ioContext, boost::asio::chrono::seconds(1));

    // timer의 만료 시점까지 대기하는 비동기 작업 등록
    timer.async_wait([&](const boost::system::error_code& ec)
                     {
                         Print(&timer, &count);
                     });

    // ioContext에 등록된 모든 비동기 작업의 완료 핸들러를 처리한다
    ioContext.run();
    std::cout << "Final count is " << count << std::endl;

    return 0;
}
