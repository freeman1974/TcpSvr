#ifndef __NETSOCK_C__
#define __NETSOCK_C__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/watchdog.h>

#include <time.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>  // htons 
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "logfile.h"
#include "netsock.h"

#ifdef __cplusplus
       extern "C" {
#endif


extern int Log_MsgLine(char *LogFileName,char *sLine);
#if 1
#define 	NetPrint(fmt,args...)  do{ \
			char _PrtNetBuf[2000]; \
			sprintf(_PrtNetBuf,":" fmt , ## args); \
			Log_MsgLine("net.log",_PrtNetBuf); \
			}while(0)
#else
#define 	NetPrint(fmt,args...)  
#endif

int NET_OpenSock(char *SockType)		//return sock handler
{
	int ret=0;

	if (!strcmp(SockType,"TCP"))
		ret=socket(AF_INET,SOCK_STREAM,0);
	else
		ret=socket(AF_INET,SOCK_DGRAM,0);
	
	signal(SIGPIPE,SIG_IGN);

	return ret;
}



int NET_CloseSock(int Sockfd)
{
	int ret=0;

	if (Sockfd>=0)
	{
		ret=shutdown(Sockfd,SHUT_RDWR);
		close(Sockfd);
	}

	return ret;
}

int NET_OptSet(int Sockfd,int timeout, int bufsize)	//timeout: sec, bufsize: KB
{
	int ret=0;

	struct timeval tv;
	int len=0;

	memset(&tv,0,sizeof(tv));
	tv.tv_sec=timeout;		//unit:sec

	do{
		len=sizeof(tv);
		ret=setsockopt(Sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,(socklen_t)len);
		if(ret)
		{
			printf("rsock: set rcv timeout fail=%d,%d\n",ret,timeout);
			ret=-2;break;
		}
		
		ret=setsockopt(Sockfd,SOL_SOCKET,SO_SNDTIMEO,&tv,(socklen_t)len);
		if(ret)
		{
			printf("rsock: set snd timeout fail=%d,%d\n",ret,timeout);
			ret=-3;break;
		}

		bufsize<<=10;		//unit:kb
		len=sizeof(bufsize);
		ret=setsockopt(Sockfd,SOL_SOCKET,SO_RCVBUF,&bufsize,(socklen_t)len);
		if(ret)
		{
			printf("rsock: set rcv timeout fail=%d,%d\n",ret,timeout);
			ret=-4;break;
		}
		
		ret=setsockopt(Sockfd,SOL_SOCKET,SO_SNDBUF,&bufsize,(socklen_t)len);
		if(ret)
		{
			printf("rsock: set snd timeout fail=%d,%d\n",ret,timeout);
			ret=-5;break;
		}
	
	}while(0);

	NetPrint("%s(%d,%d),ret=%d\n",__FUNCTION__,timeout,bufsize,ret);
	return ret;
}


int NET_PortBind(int Sockfd,int port,char *ip)
{
	int ret=0;
	struct sockaddr_in  svr;
	int flag=1;
	
	if (port<1){
		return -1;
	}

	do {
		//allow bind to an address in use already!
		ret=setsockopt(Sockfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
		if (ret<0)
		{
			ret=-2;break;
		}
		
		memset(&svr,0,sizeof(svr));
		svr.sin_family=AF_INET;
		if(ip)
			svr.sin_addr.s_addr=inet_addr(ip);
		else
			svr.sin_addr.s_addr=INADDR_ANY;
		svr.sin_port=htons(port);

		ret=bind(Sockfd,(struct sockaddr *)&svr,sizeof(svr));
		if(ret)
		{
			printf("rtcp: bind port [%d] fail=%d\n",port,ret);
			printf("%s\n",strerror(errno));
			ret=-10;break;
		}
		
		ret=listen(Sockfd,SOMAXCONN);
		if(ret)
		{
			printf("listen port [%d] fail=%d\n",port,ret);
			ret=-11;break;
		}

	}while(0);

	NetPrint("%s(%d,%s),ret=%d\n",__FUNCTION__,port,ip,ret);
	return ret;
}

int NET_GetAddr(int Sockfd,void *LocalAddr,void *RemoteAddr,int len)
{
	int ret=0;

	if ( (!LocalAddr) || (!RemoteAddr) || (len<sizeof(struct sockaddr_in)) )
	{
		return -1;
	}
	
	memset(LocalAddr,0,len);
	memset(RemoteAddr,0,len);
	
	ret=getsockname(Sockfd,(struct sockaddr *)LocalAddr,(socklen_t *)&len);
	if(ret)
	{
		printf("rsock: get local address fail=%d\n",ret);
		return -10;
	}

	ret=getpeername(Sockfd,(struct sockaddr *)RemoteAddr,(socklen_t *)&len);
	if(ret)
	{
		printf("rsock: get remote address fail=%d\n",ret);
		return -11;
	}
	
	return ret;
}


int NET_Connect(int Sockfd,char *ip,int port)
{
	int ret=0;
	
	struct sockaddr_in svr;

	if(!ip)
		return -1;
	if(port<1)
		return -2;

	memset(&svr,0,sizeof(svr));
	svr.sin_family=AF_INET;
	svr.sin_addr.s_addr=inet_addr(ip);
	svr.sin_port=htons(port);

	ret=connect(Sockfd,(struct sockaddr *)&svr,(socklen_t)sizeof(svr));
	if(ret)
	{
		NetPrint("connect %s:%d fail=%d\n",ip,port,ret);
		ret=-10;
	}

	return ret;
}

int NET_Send(int Sockfd,const char *buf,int len)
{
	int ret=0;
	int cnt=0;
	const char *p=buf;

	if ((!buf)||(len<1))
	{
		return -1;
	}

	while(len>0)
	{
		ret=send(Sockfd,p,len,0);
		if(ret<1)
			break;

		cnt+=ret;
		p+=ret;
		len-=ret;
	} //while

	if(len)
		ret=cnt;

	return ret;
}

int NET_RcvPoll(int Sockfd,int timeout)
{
	int ret=0;
	
	fd_set fds;
	struct timeval tv;

	while(1)
	{
		if(timeout>0)
		{
			memset(&tv,0,sizeof(tv));
			tv.tv_sec=timeout/1000;
			tv.tv_usec= timeout % 1000;
			tv.tv_usec*=1000;       
		}
		
		FD_ZERO(&fds);
		FD_SET(Sockfd,&fds);

		if(timeout>0)
			ret=select(Sockfd+1,&fds,NULL,NULL,&tv);
		else
			ret=select(Sockfd+1,&fds,NULL,NULL,NULL);
		if(ret==-1)
		{
			ret=-10;
			break;
		}
		else if(!ret) //timeout
		{
			ret=0;
			break;
//			continue;
		}
		else
		{
			if(FD_ISSET(Sockfd,&fds)) //data recv
			{
				ret=1;
				break;
			}
		}
	}

	return ret;
}

int NET_Rev(int Sockfd,void *buf,int len)
{
	int ret=0;
	
	int cnt=0;
	char *p=(char *)buf;

	if(!buf)
		return -1;
	if(len<1)
		return -2;

	while(len>0)
	{
		ret=recv(Sockfd,p,len,0);
		if(ret<1)
		{
			//printf("sock: recv =%d\n",ret);
			break;
		}
		
//		printf("sock: recv =%d \n",ret);
		cnt+=ret;
		p+=ret;
		len-=ret;
	} //while

	return cnt;
}

int NET_PollAndRev(int fd,char *buf, int len,int timeout)
{
	int ret=0;
	fd_set rd;
	struct timeval t;
	int cnt=0;

	if(timeout <= 0){
		t.tv_sec = 1;
		t.tv_usec = 0;
	}	
	else{
		t.tv_sec = timeout / 1000;
		t.tv_usec = (timeout % 1000) * 1000;
	}	

	FD_ZERO(&rd);
	FD_SET(fd,&rd);

	ret = select(fd + 1, &rd,NULL, NULL, &t);
	if(ret>0)
	{
		if (FD_ISSET(fd,&rd)){
			cnt = recv(fd, buf,len,0);
			//cnt=NET_Rev(fd,buf,len);
			if ((cnt<0)&&(errno!=0)){
				NetPrint("%s(fd=%d) recv()<0! errno=%s[%d]\n"
					,__FUNCTION__,fd,strerror(errno),errno);
				cnt=-10;
			}
		}
		if ((ret==1)&&(cnt==0)&&(errno!=0)){
			NetPrint("%s(fd=%d) broken pipe! errno=%s[%d]\n"
				,__FUNCTION__,fd,strerror(errno),errno);
			cnt=-11;
		}
	}
	
	return cnt;
}

int NET_SelectRev(int fd,char *buf, int len)
{
	int ret=-1;
	fd_set rd;
	struct timeval t;

	t.tv_sec = 1;
	t.tv_usec = 0;
	
	FD_ZERO(&rd);
	FD_SET(fd,&rd);
	ret = select(fd + 1, &rd,NULL, NULL, &t);
	if(ret>0)
	{
		if (FD_ISSET(fd,&rd))
			ret = recv(fd, buf,len,0);
	}

	return ret;
}


int NET_Ser_Bind(unsigned short port,int timeout_ms)
{
	int s=-1;
	struct sockaddr_in addr;
	int ret;
	int on = 1;

	//pthread_mutex_lock(&gNetThrdLock);
	do{
		s = socket(PF_INET, SOCK_STREAM, 0);	//apply new socket
		if (s < 0){ 
			ret=-2;break;
		}
	
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof (on));
		memset(&addr, 0, sizeof(struct sockaddr_in));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);
		ret = bind(s,(struct sockaddr *)&addr,sizeof(addr));
		if (ret < 0) 
		{
			NET_CloseSock(s);
			ret=-3;break;
		}
		
		ret = listen(s, 256);
		if (ret < 0) 
		{
			NET_CloseSock(s);
			ret=-4;break;
		}

		int bufsize = 512 * 1024;
		setsockopt (s,SOL_SOCKET,SO_SNDBUF,(const char*)&bufsize, sizeof(bufsize));
		setsockopt (s,SOL_SOCKET,SO_RCVBUF,(const char*)&bufsize, sizeof(bufsize));

		struct timeval tv;
		tv.tv_sec = timeout_ms/1000;
		tv.tv_usec = (timeout_ms%1000)*1000;
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
		setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
	}while(0);
	
	//pthread_mutex_unlock(&gNetThrdLock);
	return s;
}


int NET_Ser_Accept(int fd,int timeout,void * clientaddr)
{
	int ret=0;
	int new_fd=-1;
	int maxfd=0;
	
	socklen_t addrlen;
	fd_set fds;
	struct timeval tv;

	if (fd>maxfd)
		maxfd=fd;
	
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	memset(&tv,0,sizeof(tv));
	tv.tv_sec=timeout/1000;
	tv.tv_usec=(timeout%1000)*1000;

	do{
		ret = select(maxfd+1, &fds, NULL, NULL, &tv);
		if(ret <= 0)	//timeout or failed
		{
			ret=-2;break;
		}

		if(!FD_ISSET(fd, &fds))
		{
			ret=-4;break;
		}

		addrlen = sizeof(struct sockaddr_in);
		new_fd = accept(fd,(struct sockaddr *)clientaddr, (socklen_t*)&addrlen);
		if(new_fd >= 0)	//success to accept  client connection request
		{
		/*
	To avoid the following error:
	TS:21:59:09 :NET_PollAndRev(fd=49) broken pipe! errno=Numerical argument out of domain[33]
	TS:21:59:21 :NET_PollAndRev(fd=52) broken pipe! errno=Numerical argument out of domain[33]
		*/
		#if 1
			tv.tv_sec = timeout/1000;
	  		tv.tv_usec = (timeout%1000)*1000;		
	  		setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
	  		setsockopt(new_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));

			int bufsize = 512 * 1024;
			setsockopt (new_fd,SOL_SOCKET,SO_SNDBUF,(const char*)&bufsize, sizeof(bufsize));
			setsockopt (new_fd,SOL_SOCKET,SO_RCVBUF,(const char*)&bufsize, sizeof(bufsize));

			int on=1;
			setsockopt(new_fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&on, sizeof (on));
		#endif
		
			ret=new_fd;
		}
		else
		{
			ret=-5;break;
		}
	}while(0);
		
	//NetPrint("%s(),%s,ret=%d\n",__FUNCTION__,ret>=0?"success":"failed",ret);
	return ret;
}

int NET_CheckTcpLink(int fd)
{
	int ret=0;
	struct tcp_info tf; 
	int len=sizeof(tf); 
	
	ret=getsockopt(fd, IPPROTO_TCP, TCP_INFO, &tf, (socklen_t *)&len);
	if (!ret){
		if((tf.tcpi_state==TCP_ESTABLISHED))
			ret=1;
	}

	if ((ret!=1)&&(errno!=0)){
	//if ((ret!=1)){
		NetPrint("%s(fd=%d) tcp link(pipe) broken !\n",__FUNCTION__,fd);
		NetPrint("net err:%s[errno=%d]\n",strerror(errno),errno);
	}
	
	return ret;
}


int NET_ConnTcpSvr(char *svr, int port)
{
	int ret=0;
	int sock=-1;
	struct sockaddr_in remaddr;
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1)
	{
		NetPrint("socket error! errno=%d \n", errno);
		return -2;
	}
	
	bzero(&remaddr, sizeof(remaddr));
	remaddr.sin_family = AF_INET;
	remaddr.sin_port = htons(port);
	remaddr.sin_addr.s_addr = inet_addr(svr);
	bzero(&(remaddr.sin_zero), 8);

	if(connect(sock, (struct sockaddr *)&remaddr, sizeof(remaddr))!= 0)
	{
		NET_CloseSock(sock);
		return -3;
	}

	int bufsize = 512 * 1024;
	int on = 1;
	setsockopt (sock,SOL_SOCKET,SO_SNDBUF,(const char*)&bufsize, sizeof(bufsize));
	setsockopt (sock,SOL_SOCKET,SO_RCVBUF,(const char*)&bufsize, sizeof(bufsize));
 	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof (on));

	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 800 * 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));

	ret=sock;
	NetPrint("Tcp Connection %s\n",ret>=0?"established":"failed");
	return ret;
}


#ifdef __cplusplus
}
#endif 

#endif		//__NETSOCK_C__

