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

    // - io_context에 등록된 모든 비동기 작업을 처리할 때까지 이벤트 루프 실행
    // - 비동기 작업의 완료 핸들러를 호출하는 스레드는 오직 run을 호출한 스레드
    // - run을 호출했을 때 진행 중이거나 완료된 비동기 작업이 없으면 즉시 반환
    io.run();

    return 0;
}
