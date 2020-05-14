#include <signal.h>
#include <iostream>
	   
#ifndef _WIN32
#include <unistd.h>
#endif
#include <iomanip>

#include <nlohmann/json.hpp>
	   
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Network/TcpServer.h"
#include "Network/TcpSession.h"
#include "tcpsvr.h"

using namespace std;
using namespace toolkit;
using json = nlohmann::json;


class EchoSession: public TcpSession {
public:
   EchoSession(const Socket::Ptr &sock) :
		   TcpSession(sock) {
	   DebugL;
   }
   ~EchoSession() {
	   DebugL;
   }

   
   /*
   json lib: https://github.com/nlohmann/json
   build: # g++ -o tstjson basic_json__basic_json.cpp -std=c++11 -I../../single_include
   
   */
   int msgProc(char *MsgIn)
   {
	   //const string out="";
	   char buf[200]={0};
	   char msg[200]={0};
   
	   int len=strlen(MsgIn);
	   if ((len<=0) || (len>160)){
		   ErrorL << "msg len is abnormal!";
		   return -1;
	   }
   
	   if ((MsgIn[0] != '@')||(MsgIn[1] != '@') || (MsgIn[len-2] != '#')||(MsgIn[len-1] != '#')){
		   ErrorL << "msg corruption.";
		   return -2;
	   }
	   strncpy(msg, &MsgIn[3], len-5);
	   TraceL << "msg[]=" << msg;
	   
	   try
	   {
		   //("@@1{\"action\":\"redirect\",\"devid\":\"A68000011809SN000320\",\"vid\":\"0001\",\"pid\":\"A680\",\"devtype\":\"5\"}##")
		   auto json_txt = msg;
   
		   json j_obj = json::parse(json_txt);
		   //std::cout << std::setw(4) << j_obj << "\n\n";
		   
		   auto key_action = j_obj.find("action");
		   if (key_action != j_obj.end()){
			   //std::cout << "action="<< *key_action<<endl;
			   
			   auto key_appid = j_obj.find("usrid");
			   if (key_appid != j_obj.end()){
				   std::cout << "usrid="<< *key_appid<<endl;
				   //out = "@@1{\"action\":\"redirect\",\"svrip\":\"" + TCP_SERVER "\",\"svrport\":" + TCP_PORT_APP +"}##";
			   		sprintf(buf,"{\"action\":\"redirect\",\"svrip\":\"%s\",\"svrport\":%d}",TCP_SERVER, TCP_PORT_APP);
			   }
			   
			   auto key_devid = j_obj.find("devid");
			   if (key_devid != j_obj.end()){
				   //std::cout << "devid="<< *key_devid<<endl;
				   //out = "@@1{\"action\":\"redirect\",\"svrip\":\"" + TCP_SERVER "\",\"svrport\":" + TCP_PORT_DEV +"}##";
				   sprintf(buf,"{\"action\":\"redirect\",\"svrip\":\"%s\",\"svrport\":%d}",TCP_SERVER, TCP_PORT_DEV);
			   }
		   }
		   
	   }
	   catch (json::parse_error& e)
	   {
		   // output exception information
		   ErrorL << "exception when msgproc. what=" << e.what();
		   std::cout << "message: " << e.what() << '\n'
					 << "exception id: " << e.id << '\n'
					 << "byte position of error: " << e.byte << std::endl;
		   return -3;
	   }

	   len = strlen(buf);
	   if (len>0){
	   		BufferRaw::Ptr sndBuf = obtainBuffer();
			if (sndBuf){
				sndBuf->assign(buf);
				send(sndBuf);
				TraceL << "send[]=" << buf;
			}
	   }

	   return len;
   }
   
   virtual void onRecv(const Buffer::Ptr &buf) override{
	   //处理客户端发送过来的数据
	   //TraceL << buf->data() <<  " from port:" << get_local_port();
	   TraceL << "recv[]=" << buf->data();
	   msgProc(buf->data());
   }
   virtual void onError(const SockException &err) override{
	   //客户端断开连接或其他原因导致该对象脱离TCPServer管理
	   WarnL << err.what();
   }
   virtual void onManager() override{
	   //定时管理该对象，譬如会话超时检查
	   DebugL;
   }

private:
   Ticker _ticker;
};


int main() {
   printf("built on time=%s\n",BNO);

   //Logger::Instance().add(std::make_shared<ConsoleChannel>());
   Logger::Instance().add(std::make_shared<FileChannel>());
   Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

   TcpServer::Ptr server(new TcpServer());
   server->start<EchoSession>(TCP_PORT_ENTRYSVR);


   static semaphore sem;
   signal(SIGINT, [](int) { sem.post(); });
   signal(SIGTERM, [](int) { sem.post(); });
   sem.wait();
   return 0;
}

