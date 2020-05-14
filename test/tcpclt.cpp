#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Network/TcpClient.h"
using namespace std;
using namespace toolkit;

class TestClient: public TcpClient {
public:
    typedef std::shared_ptr<TestClient> Ptr;
    TestClient():TcpClient() {
        DebugL;
    }
    ~TestClient(){
        DebugL;
    }
protected:
    virtual void onConnect(const SockException &ex) override{
        //连接结果事件
        InfoL << (ex ?  ex.what() : "success");
    }
    virtual void onRecv(const Buffer::Ptr &pBuf) override{
        //接收数据事件
        DebugL << pBuf->data() << " from port:" << get_peer_port();
    }
    virtual void onFlush() override{
        //发送阻塞后，缓存清空事件
        DebugL;
    }
    virtual void onErr(const SockException &ex) override{
        //断开连接事件，一般是EOF
        WarnL << ex.what();
    }
    virtual void onManager() override{

        BufferRaw::Ptr buf = obtainBuffer();
        if(buf){
            buf->assign("@@1{\"action\":\"redirect\",\"devid\":\"A68000011809SN000320\",\"vid\":\"0001\",\"pid\":\"A680\",\"devtype\":\"5\"}##");
            (*this) << (Buffer::Ptr &)buf;
        }
    }
private:
    int _nTick = 0;
};


int main() {

    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    TestClient::Ptr client(new TestClient());//必须使用智能指针
    client->startConnect("127.0.0.1",16081);//连接服务器

    //TcpClientWithSSL<TestClient>::Ptr clientSSL(new TcpClientWithSSL<TestClient>());//必须使用智能指针
    //clientSSL->startConnect("127.0.0.1",9001);//连接服务器

    //退出程序事件处理
    static semaphore sem;
    signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
    sem.wait();
    return 0;
}

