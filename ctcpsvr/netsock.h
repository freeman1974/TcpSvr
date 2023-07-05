
#ifndef __NETSOCK_H__
#define __NETSOCK_H__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

	int NET_OpenSock(char *SockType);		//return sock handler
	int NET_CloseSock(int Sockfd);
	int NET_OptSet(int Sockfd,int timeout, int bufsize);
	int NET_PortBind(int Sockfd,int port,char *ip);
	int NET_GetAddr(int Sockfd,void *LocalAddr,void *RemoteAddr,int len);
	int NET_Connect(int Sockfd,char *ip,int port);
	int NET_Send(int Sockfd,const char *buf,int len);

	int NET_RcvPoll(int Sockfd,int timeout);
	int NET_Rev(int Sockfd,void *buf,int len);
	int NET_PollAndRev(int fd,char *buf, int len,int timeout);
	int NET_SelectRev(int fd,char *buf, int len);

	int NET_Ser_Bind(unsigned short port,int timeout_ms);
	int NET_Ser_Accept(int fd,int timeout,void *clientaddr);
	int NET_CheckTcpLink(int fd);
	int NET_ConnTcpSvr(char *svr, int port);

#ifdef __cplusplus
}
#endif //__cplusplus */

#endif	//__NETSOCK_H__


