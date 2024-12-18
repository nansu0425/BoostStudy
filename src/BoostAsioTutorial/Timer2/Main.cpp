#include <iostream>
#include <boost/asio.hpp>

int main()
{
    boost::asio::io_context io;

    boost::asio::steady_timer timer(io, boost::asio::chrono::seconds(5));
    timer.async_wait([](const boost::system::error_code& e)
                     {
                         std::cout << "timeout" << std::endl;
                     });

    // - 비동기 작업의 완료 핸들러를 호출하는 스레드는 오직 run을 호출한 스레드
    // - run을 호출했을 때 진행 중이거나 완료된 비동기 작업이 없으면 즉시 반환
    // - 비동기 작업이 진행 중이면 run을 만난 스레드는 비동기 작업이 완료될 때까지 대기
    io.run();

    return 0;
}
