#include "vos.h"

#include <signal.h>
#include <fcntl.h>
#include <malloc.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <semaphore.h>
#include <pthread.h>
#include <memory.h>
#include <dirent.h>
#include <termios.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <sys/statfs.h>
#include <sys/msg.h>
#include <sys/ipc.h>
//#include <sys/signal.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netdb.h>
#include <getopt.h>
#include <linux/if_ether.h>
#include <net/if.h>

#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/rtc.h>

/*
NOTE: some embeded platform does not support pthread_attr_t operation func.

*/
void* osCreateThread(int prior,pfnThread route,void* args)
{
	pthread_t* ret=NULL;
	//pthread_attr_t attr;
	//struct   sched_param   param;

	ret = (pthread_t*)malloc(sizeof(pthread_t));
	if(ret)
	{
		//pthread_attr_init(&attr);
		//pthread_attr_setstacksize(&attr,MAX_STATCK_SIZE);
		//pthread_attr_setschedpolicy(&attr,   SCHED_RR);
		//param.sched_priority   =   prior;
		//pthread_attr_setschedparam(&attr,&param);

		if(0 == pthread_create(ret,NULL,route,args))
		{
			//pthread_attr_destroy(&attr);
			return (void*)ret;
		}
		//pthread_attr_destroy(&attr);
	}
	return NULL;
}


int osJoinThread(void* thread)
{
	if (thread)
	{
		pthread_t tid = *((pthread_t*)thread);
		pthread_join(tid,NULL);
		free(thread);
	}
	return 0;
}

void* osCreateSemaphore(char* name,int level)
{
	sem_t* ret;
	ret = (sem_t*)malloc(sizeof(sem_t) + strlen(name));
	if(ret)
	{
		sem_init(ret,0,level);
	}
	return ret;
}

int osTryWaitSemaphore(void* sem)
{
	if (sem)
		return sem_trywait((sem_t*)sem);
	else
		return 0;
}

int osWaitSemaphore(void* sem)
{
	if (sem)
		return sem_wait((sem_t*)sem);
	else
		return 0;
}

int osPostSemaphore(void* sem)
{
	if (sem)
		return sem_post((sem_t*)sem);
	else
		return 0;
}

int osDestroySemaphore(void* sem)
{
	if (sem){
		sem_destroy((sem_t*)sem);
		free(sem);
	}
	return 0;
}

void* osMalloc(int size)
{
	void *p=NULL;

	if(size & 0xfff)	// min size=4KB
	{
		size += 4096 - (size & 0xfff);
	}
	p=calloc(size,1);

	return p;
}

void osFree(void* mem)
{
	if (mem)
		free(mem);
}
