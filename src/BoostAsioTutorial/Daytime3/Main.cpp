// Daytime.3 - An asynchronous TCP daytime server

#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::string MakeDaytimeString()
{
    std::time_t now = std::time(0);

    return std::ctime(&now);
}

class TcpConnection
    : public std::enable_shared_from_this<TcpConnection>
{
public:
    using Pointer = std::shared_ptr<TcpConnection>;

    // 정적 팩토리 메서드로 TcpConnection은 이 메서드로만 생성 가능
    // 인스턴스가 shared_ptr로 관리되도록 강제할 수 있다
    static Pointer Create(boost::asio::io_context& ioContext)
    {
        return Pointer(new TcpConnection(ioContext));
    }

    tcp::socket& GetReferenceSocket()
    {
        return _socket;
    }

    void StartWrite()
    {
        _message = MakeDaytimeString();

        // 비동기 write이 완료되면 HandleWrite이 호출된다
        // shared_ptr로 관리되고 있는 객체는 자기 자신을 참조할 때 this대신 shared_from_this 사용
        boost::asio::async_write(_socket,
                                 boost::asio::buffer(_message),
                                 [self = shared_from_this()](const boost::system::error_code& error,
                                                             const size_t nBytesTransferred)
                                 {
                                     self->HandleWrite();
                                 });
    }

private:
    TcpConnection(boost::asio::io_context& ioContext)
        : _socket(ioContext)
    {}

    void HandleWrite()
    {}

private:
    tcp::socket _socket;
    std::string _message;
};

class TcpServer
{
public:
    TcpServer(boost::asio::io_context& ioContext)
        : _ioContext(ioContext)
        , _acceptor(ioContext, tcp::endpoint(tcp::v4(), 13))
    {
        StartAccept();
    }

private:
    void StartAccept()
    {
        // 클라이언트와 연결된 TCP를 관리할 객체 생성
        TcpConnection::Pointer pNewConnection = TcpConnection::Create(_ioContext);

        // accept가 완료되면 HandleAccept 호출
        _acceptor.async_accept(pNewConnection->GetReferenceSocket(),
                               [this, pNewConnection](const boost::system::error_code& error)
                               {
                                   HandleAccept(pNewConnection, error);
                               });

        // 핸들러가 shared_ptr인 pNewConnection을 복사했기 때문에
        // StartAccept가 종료돼도 생성된 TcpConnection 인스턴스는 유지되고 추적 가능하다
        // TcpConnection 인스턴스는 pNewConnection을 복사한 핸들러가 종료되면 소멸된다
    }

    void HandleAccept(TcpConnection::Pointer pNewConnection,
                      const boost::system::error_code& error)
    {
        // 에러가 없으면 TCP 연결에 비동기 쓰기 작업을 요청한다
        if (!error)
        {
            pNewConnection->StartWrite();
        }

        // 다시 비동기 accpet 작업 요청
        StartAccept();
    }

private:
    boost::asio::io_context& _ioContext;
    tcp::acceptor _acceptor;
};

int main()
{
    try
    {
        boost::asio::io_context ioContext;
        TcpServer server(ioContext);

        // 현재 스레드가 모든 비동기 작업이 끝날 때까지 핸들러를 처리한다
        ioContext.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
