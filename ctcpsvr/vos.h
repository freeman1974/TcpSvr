#ifndef __VOS_H__
#define __VOS_H__
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#include <typeinfo>
#include <iostream>

#ifdef __cplusplus
       extern "C" {
#endif

#define	THREAD_STATE_INIT  0
#define	THREAD_STATE_RUN   1
#define	THREAD_STATE_EXIT  2
#define	THREAD_STATE_PAUSE 3
typedef struct{
	void *pTrd;
	int state;
	int port;
}TcpTrd_T;

typedef void* (*pfnThread)(void* args);
#define MAX_STATCK_SIZE		(1024 * 1024)	// = 1MB

void* osCreateThread(int prior,pfnThread route,void* args);
int osJoinThread(void* thread);
void* osCreateSemaphore(char* name,int level);
int osTryWaitSemaphore(void* sem);
int osWaitSemaphore(void* sem);
int osPostSemaphore(void* sem);
int osDestroySemaphore(void* sem);
void* osMalloc(int size);
void osFree(void* mem);

#ifdef __cplusplus
}
#endif 

#endif

