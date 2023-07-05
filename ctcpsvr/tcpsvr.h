#ifndef __TCPSVR_H__
#define __TCPSVR_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef void* tcp_handle_t;
typedef struct
{
	int idx;
	int ref;
	int state;
	unsigned int timeout;
	int session_id;
	char uuid[40];

	int IpcId;
	int QueId;

	int ConnTcpSock;
	int dataSock;

	tcp_handle_t CtrlTrd;
	tcp_handle_t DataTrd;

	struct sockaddr_in client;
}entity_ctrl_t;

typedef struct{
	entity_ctrl_t* pTrdTcb;
	int port;
}TCPTHRD_S;

#define MAX_TCP_CLIENT 	1000
#define TCP_MSG_BUF_NUM	8
#define TCP_BUFF_LEN 	1024

#define TCP_MSG_LEN 	100
#define MSG_HEAD_MAGIC 	0x28
#define MSG_TAIL_MAGIC 	0x29

#define MSG_TYPE_HELLO  	"hello"
#define MSG_TYPE_START_REC  "start_record"
#define MSG_TYPE_STOP_REC   "stop_record"

int TcpSvr_init(const char *cfgFilename);
int TcpSvr_close(void);
void TcpSvr_sigExit(int sig);

#define START_RECORD 	0x5A01
#define START_RECORD_R 	0x5A02
#define STOP_RECORD 	0x5B01
#define STOP_RECORD_R 	0x5B02

#define UNKNOWN_MSG 	0xffff

#pragma pack(2)
typedef struct{
	int cmd;
	int size;
	short ver;
	char table[4];
	char gmcode[16];
}MSG_CMD_T;

typedef struct{
	int cmd;
	int size;
	short ver;
	char gmcode[16];
	int ret;
}MSG_RES_T;
#pragma pack()


#ifdef __cplusplus
}
#endif

#endif

