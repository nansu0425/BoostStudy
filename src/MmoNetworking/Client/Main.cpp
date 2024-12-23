#include <Client/Pch.hpp>
#include <NetCommon/Message.hpp>
#include <NetCommon/ClientInterface.hpp>

enum class MessageId : uint32_t
{
    MovePlayer,
    AttackMonster,
    FireBullet,
};

class Client : public NetCommon::IClient<MessageId>
{
public:
    bool FireBullet(float x, float y)
    {
        NetCommon::Message<MessageId> message;
        message.header.id = MessageId::FireBullet;

        message << x << y;
        Send(message);

        return true;
    }
};

int main() 
{
    try
    {
        Client client;

        client.Connect("127.0.0.1", "3000");
        client.FireBullet(2.3f, 5.2f);

    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
