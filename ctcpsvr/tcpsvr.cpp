#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#include <linux/if_ether.h>
#include <linux/watchdog.h>

#include <time.h>
#include <sys/time.h>
#include <linux/rtc.h>
#include <pthread.h>

#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>  // htons
#include <netdb.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <linux/uuid.h>

#include "vos.h"
#include "netsock.h"
#include "cfgfile.h"
#include "logfile.h"
#include "tcpsvr.h"

using namespace std;

#if 1
#define 	TcpPrint(fmt,args...)  do{ \
			char info[3000]; \
			sprintf(info,":" fmt , ## args); \
			Log_MsgLine("tcpsvr.log",info); \
			}while(0)
#else
#define 	TcpPrint(fmt,args...)
#endif

static TCPTHRD_S gTcpIns[MAX_TCP_CLIENT];
static TcpTrd_T gTcpMainTrd;

static int _Msg_handler1(entity_ctrl_t* svr, char *msgBuf, unsigned int bufLen)
{
	int ret=0;
	int msgNum=bufLen/TCP_MSG_LEN;
	int i=0;
	char *pMsg=NULL;
	char sCut[]={',','\0'};
	char *pSubStr[4]={NULL};
	char magicHead=0,magicTail=0;
	char TcpMsg[TCP_MSG_LEN]={0};

	TcpPrint("---msg_process. bufLen=%d, msgNum=%d\n",bufLen, msgNum);
	for (i=0;i<msgNum;i++){
		magicHead=msgBuf[i*TCP_MSG_LEN];
		magicTail=msgBuf[i*TCP_MSG_LEN + TCP_MSG_LEN -1];
		if ((magicHead!=MSG_HEAD_MAGIC)||(magicTail!=MSG_TAIL_MAGIC)){
			TcpPrint("!!!msg packet corruption. magicHead=%x, magicTail=%x\n",magicHead,magicTail);
			break;
		}
		pMsg=msgBuf + i*TCP_MSG_LEN + 1;
		if (strlen(pMsg)>=TCP_MSG_LEN){
			TcpPrint("string(real) msg-size is too long(limit to 90 chars). break\n");
			break;
		}
		TcpPrint("msg[%d]=%s\n",i,pMsg);
		strncpy(TcpMsg,pMsg,sizeof(TcpMsg));
		pSubStr[0]=strtok(pMsg,sCut);
		if ((pSubStr[0])&&(!strcmp(pSubStr[0], MSG_TYPE_HELLO))){
			TcpPrint("TcpSvr ignore hello. no reply to client.\n");
			continue;
		}
		pSubStr[1]=strtok(NULL,sCut);
		pSubStr[2]=strtok(NULL,sCut);
		TcpPrint("action=%s, table_name=%s, round=%s\n",pSubStr[0],pSubStr[1], pSubStr[2]);
		if (!pSubStr[0]||!pSubStr[1]||!pSubStr[2]){
			continue;
		}
		ret=NET_Send(svr->ConnTcpSock,"ack",3);
		TcpPrint("send back 'ack' to confirm.\n");
	}

	return ret;
}

static int _Msg_handler2(entity_ctrl_t* svr, char *msgBuf, unsigned int bufLen)
{
	int ret=0;
	char TcpMsg[TCP_MSG_LEN]={0};
	int action=0,response=0;
	int TcpMsgLen=sizeof(MSG_CMD_T);
	int msgNum=bufLen/TcpMsgLen;
	int i=0;
	MSG_CMD_T *pkg;
	int pkgSize=0;
	char table_name[8]={0};
	char round_name[20]={0};
	int RespMsgLen=sizeof(MSG_RES_T);
	MSG_RES_T RespMsg;
	int retCode=0;

	TcpPrint("---msg_process. bufLen=%d,msgNum=%d\n",bufLen,msgNum);
	for (i=0;i<msgNum;i++){
		pkg=(MSG_CMD_T *)msgBuf;
		pkgSize=ntohl(pkg->size);
		if ((pkgSize != TcpMsgLen)){
			TcpPrint("!!!msg packet corruption. pkgSize=%d\n",pkgSize);
			break;
		}
		action = ntohl(pkg->cmd);
		memcpy(table_name, pkg->table, 4);
		memcpy(round_name, pkg->gmcode, 16);
		TcpPrint("action=0x%x, table_name=%s, round=%s\n",action,table_name,round_name);
		if ((action == START_RECORD)&&(strlen(round_name)>0)){
			response = START_RECORD_R;
			snprintf(TcpMsg,sizeof(TcpMsg),"start_record,%s,%s",table_name,round_name);
		}
		else if ((action == STOP_RECORD)&&(strlen(round_name)>0)){
			response = STOP_RECORD_R;
			snprintf(TcpMsg,sizeof(TcpMsg),"stop_record,%s,%s",table_name,round_name);
		}
		else{
			response = UNKNOWN_MSG;
			TcpPrint("unkonwn cmd! or round_name=NULL\n");
			retCode=0x1001;
		}

		RespMsg.cmd = htonl(response);
		RespMsg.size = htonl(RespMsgLen);
		RespMsg.ver = htons(1);
		memcpy(RespMsg.gmcode,pkg->gmcode,16);
		RespMsg.ret = htonl(retCode);
		ret=NET_Send(svr->ConnTcpSock,(const char*)&RespMsg,RespMsgLen);
		TcpPrint("send-response:0x%x. retCode=0x%x\n", response,retCode);
	}

	return ret;
}


void* TcpConnection_thread(void* args)
{
	int ret=0;
	entity_ctrl_t* svr = (entity_ctrl_t*)args;
	int pollTick=500000;	// 500ms
	int clientTimeout=300,tickCount=0; //300*tick=2.5min

	char RevBuf[TCP_BUFF_LEN]={0};
	int RevLen=0;
	int TcpMsgLen=sizeof(MSG_CMD_T);

	int idx=-1;
	int ConnTcpSock=-1;

	signal(SIGPIPE,SIG_IGN);

	ConnTcpSock=svr->ConnTcpSock;
	idx=svr->idx;

	TcpPrint(">>Enter TcpSvr connection thread!sockfd=%d,ipcid=%x,idx=%d\n"
		,ConnTcpSock,svr->IpcId,svr->idx);

	while(svr->state != THREAD_STATE_EXIT)
	{
		memset(RevBuf,0,sizeof(RevBuf));
		RevLen=NET_PollAndRev(ConnTcpSock,RevBuf,TCP_BUFF_LEN,1000);
		if (RevLen>0){
			//TcpPrint("RevBuf[%d]=%s\n",RevLen,RevBuf);
			if (RevLen%TcpMsgLen>0){
				TcpPrint("!!!msg packet imcomplete. RevLen=%d, fixed pkg_len=%d\n",RevLen,TcpMsgLen);
			}
			else{
				ret=_Msg_handler1(svr, RevBuf, RevLen);
				if (ret>=0){
					tickCount=0;
				}
			}
		}
		else if (RevLen==0){
			ret=NET_CheckTcpLink(ConnTcpSock);
			if (ret!=1)
				break;
			else
				continue;
		}
		else{
			//ret=NET_CheckTcpLink(ConnTcpSock);
			//if (ret!=1)
			{
				TcpPrint("TCP pipe broken! RevLen=%d !exit thread[idx=%d]!\n",RevLen,svr->idx);
				break;
			}
		}

		if (tickCount++ >= clientTimeout){
			TcpPrint("Tcp client[%d] timeout! close socket.\n", svr->idx);
			break;
		}
	}

	svr->state=THREAD_STATE_EXIT;
	TcpPrint("<<Exit TcpSvr connection thread.idx=%d\n",svr->idx);
	return args;
}

static int _TcpSvr_destroy(entity_ctrl_t* svr)
{
	int ret=0;
	int idx=-1;

	if (svr){
		idx=svr->idx;

		if(svr->DataTrd)
		{
			//osJoinThread(svr->DataTrd);
			svr->DataTrd=NULL;
		}

		if(svr->CtrlTrd){
			//osJoinThread(svr->CtrlTrd);
			svr->CtrlTrd=NULL;
		}

		if(svr->ConnTcpSock >= 0){
			NET_CloseSock(svr->ConnTcpSock);
			svr->ConnTcpSock=-1;
		}

		if(svr->dataSock >= 0){
			NET_CloseSock(svr->dataSock);
			svr->dataSock=-1;
		}

		TcpPrint("%s(idx=%d)\n",__FUNCTION__,idx);
		osFree(svr);
	}

	return ret;
}


static entity_ctrl_t* _TcpSvr_create(int sock,struct sockaddr_in* client,int dataSock,int idx)
{
	entity_ctrl_t* svr = (entity_ctrl_t*)osMalloc(sizeof(entity_ctrl_t));

	if(svr)
	{
		memset(svr,0,sizeof(entity_ctrl_t));

		svr->idx=idx;
		svr->ref = 1;
		svr->timeout = 0;

		svr->client = *client;
		svr->ConnTcpSock = sock;
		svr->dataSock=dataSock;
		svr->DataTrd=NULL;
		svr->CtrlTrd=NULL;

		svr->session_id=svr->ConnTcpSock;
		svr->state=THREAD_STATE_RUN;
		svr->CtrlTrd = osCreateThread(21,TcpConnection_thread,(void*)svr);

		TcpPrint("%s(idx=%d) success!\n",__FUNCTION__,idx);
	}
	else{
		TcpPrint("%s(idx=%d).Faied to malloc memory!!\n",__FUNCTION__,idx);
	}

	return svr;
}

static void* TcpListen_thread(void* args)
{
	int ret=0;
	int i;
	int TcpSock=-1,ConnTcpSock=-1;
	int dataSock=-1;
	entity_ctrl_t* svr=NULL;
	struct sockaddr_in client;
	struct in_addr addr1;
	int port=gTcpMainTrd.port;

	signal(SIGPIPE,SIG_IGN);

	TcpSock = NET_Ser_Bind(port,800);
	if (TcpSock<0){
		TcpPrint("Fail to bind to port:%d ,exit!\n", port);
		return NULL;
	}
	else{
		TcpPrint("bind tcp port:%d success!waiting for tcp connect...\n",port);
	}

	while(gTcpMainTrd.state != THREAD_STATE_EXIT)
	{
		ConnTcpSock = NET_Ser_Accept(TcpSock,5*1000,&client);

		if(ConnTcpSock >= 0)		//accept success!
		{
			memcpy(&addr1,&(client.sin_addr.s_addr),4);
			TcpPrint("TcpSvr connected by ip:%s ,ConnTcpSock=%d\n",inet_ntoa(addr1),ConnTcpSock);

			for(i = 0; i < MAX_TCP_CLIENT; i ++)
			{
				if(gTcpIns[i].pTrdTcb==NULL)
					break;
			}

			if(i >= MAX_TCP_CLIENT){
				TcpPrint("reach max_tcp_clients=%d,close socket!\n",MAX_TCP_CLIENT);
				NET_CloseSock(ConnTcpSock);ConnTcpSock=-1;
			}
			else{
				svr = _TcpSvr_create(ConnTcpSock,&client,dataSock,i);
				if(svr == NULL)
				{
					TcpPrint("Fail to create TcpSvr!close socket!\n");
					NET_CloseSock(ConnTcpSock);ConnTcpSock=-1;
				}
				else
				{
					TcpPrint("Create Tcp session_id[%d] success!\n",i);
					gTcpIns[i].pTrdTcb=svr;
				}
			}

		}
		else
		{
			for(i=0; i<MAX_TCP_CLIENT; i++)
			{
				svr = gTcpIns[i].pTrdTcb;
				if(!svr)
					continue;
				if(svr->timeout)
				{
					--svr->timeout;
					if(!svr->timeout)
					{
						svr->state=THREAD_STATE_EXIT;
						TcpPrint("Tcp session_id[%d] handshake timeout with devid=%s ,devToken=%d\n"
							,i,svr->uuid,svr->session_id);
					}
				}

				if(THREAD_STATE_EXIT==svr->state)
				{
					_TcpSvr_destroy(svr);gTcpIns[i].pTrdTcb=NULL;
					TcpPrint("Delete Device Tcp session_id[%d]!destory svr!\n",i);
				}
			}
		}
	}

	if (TcpSock>=0)
		NET_CloseSock(TcpSock);

	TcpPrint("<<Exit TcpSvr listener thread<<\n");
	return args;
}


int TcpSvr_Init(const char *cfgFilename)
{
	int ret=0;

	ret=Cfg_ReadInt(cfgFilename,"tcp","port",&gTcpMainTrd.port,10017);
	if (!ret){
		TcpPrint("TcpSvr init. Listening on port:%d \n",gTcpMainTrd.port);

		memset(gTcpIns, 0, sizeof(gTcpIns));

		gTcpMainTrd.pTrd = osCreateThread(10,TcpListen_thread,(void*)&gTcpMainTrd);
		if(gTcpMainTrd.pTrd != NULL) {
			gTcpMainTrd.state = THREAD_STATE_RUN;
			ret=1;
		}
		else{
			TcpPrint("Failed to create TcpListen_thread!\n");
			ret=-2;
		}
	}
	
	TcpPrint("TcpSvr_Init(%s). ret=%d\n",cfgFilename,ret);
	return ret;
}

int TcpSvr_close(void)
{
	int i;

	for(i=0; i<MAX_TCP_CLIENT; i++)	{
		if(gTcpIns[i].pTrdTcb)	{
			gTcpIns[i].pTrdTcb->state=THREAD_STATE_EXIT;
			_TcpSvr_destroy(gTcpIns[i].pTrdTcb);
			gTcpIns[i].pTrdTcb=NULL;
		}
	}

	if(gTcpMainTrd.pTrd != NULL)	{
		gTcpMainTrd.state = THREAD_STATE_EXIT;
		//osJoinThread(gTcpMainTrd);
		gTcpMainTrd.pTrd = NULL;
		TcpPrint("TcpSvr_close().\n");
	}

	return 0;
}

void TcpSvr_sigExit(int sig)
{
	printf("pid:%d receive signal:%d, prepare to exit!\n",getpid(),sig);
	TcpPrint("pid:%d receive signal:%d, prepare to exit!\n",getpid(),sig);

	TcpSvr_close();

	exit(-sig);
}

int main(int argc, char **argv)
{
	int ret=0;
	int sleepTick = 60;

	printf("built on time=%s\n",BNO);
	TcpPrint("built on time=%s\n",BNO);
	if (argc != 2){
		printf("#usage: %s <cfgfile>\n", argv[0]);
		return 0;
	}

	signal(SIGINT, TcpSvr_sigExit);	
	signal(SIGTERM, TcpSvr_sigExit);
	signal(SIGUSR1, TcpSvr_sigExit);

	ret=TcpSvr_Init(argv[1]);

	printf("Enter main-loop!\n" );
	TcpPrint("Enter main-loop!\n" );
	while(1)
	{
		sleep(sleepTick);
	}

	TcpSvr_sigExit(ret);
	return ret;
}
