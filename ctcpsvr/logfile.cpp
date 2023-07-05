#ifndef __LOGFILE_C__
#define __LOGFILE_C__

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>

#include "logfile.h"

#ifdef __cplusplus
       extern "C" {
#endif

#define MIN(a,b) ((a>b)?b:a)

static char gLogDir[256]=LOG_FILE_DIR;
static unsigned int gMaxLogFileSize=LOG_FILE_LIMIT;
static char gsLine[LOG_LINE_LIMIT+32];
static pthread_mutex_t  gLogThrdLock;
static int gLogInitFlag=0;

static inline unsigned long fGetSize(const char *filename)
{
	struct stat buf;

	if(stat(filename, &buf)<0)
	{
		return 0;
	}
	return (unsigned long)buf.st_size;
}

int Log_Init(char *LogDir,unsigned int MaxLogFileSize)
{
	if (gLogInitFlag)
		return 0;
	
	if (LogDir){
		if (strlen(LogDir)>sizeof(gLogDir))
			return -1;
		strcpy(gLogDir,LogDir);
	}
	if ((MaxLogFileSize>(2*KB))&&(MaxLogFileSize<=(500*KB)))
		gMaxLogFileSize=MaxLogFileSize;

	pthread_mutex_init(&gLogThrdLock,NULL);
	gLogInitFlag=1;
	return 0;
}

int Log_UnInit(void)
{
	if (gLogInitFlag){
		pthread_mutex_destroy(&gLogThrdLock);
		gLogInitFlag=0;
	}
	
	return 0;
}

//return errcode or written string length
int Log_MsgLine2(char *Dir,char *FileName,char *sLine,unsigned int FileSizeLimit)
{
	int ret=0;
	
	FILE *fp=NULL;
	unsigned long fSize=0;
	static char FullFileName[128]={0};
	struct tm *pLocalTime=NULL;
	time_t timep;

	if ( (!Dir)||(!FileName)||(!sLine)||(FileSizeLimit<=0) )
	{
		ret=-1;goto End;
	}

	ret=strlen(sLine);
	if (ret>LOG_LINE_LIMIT)
	{
		ret=-2;goto End;
	}
		
	time(&timep);
	pLocalTime=localtime(&timep);	//convert to local time to create timestamp
	sprintf(gsLine,"%04d%02d%02d-%02d:%02d:%02d %s",pLocalTime->tm_year+1900,pLocalTime->tm_mon+1,pLocalTime->tm_mday,pLocalTime->tm_hour,pLocalTime->tm_min,pLocalTime->tm_sec,sLine);
	
	sprintf(FullFileName,"%s/",Dir);
	strcat(FullFileName,FileName);
	
	fSize=fGetSize(FullFileName);
	if (fSize==0){
		fp = fopen(FullFileName, "w+");
	}
	else if (fSize>=FileSizeLimit){
		char destFilename[128]={0};
		sprintf(destFilename,"%s-%04d%02d%02d-%02d%02d%02d",FullFileName,
			pLocalTime->tm_year+1900,pLocalTime->tm_mon+1,pLocalTime->tm_mday,pLocalTime->tm_hour,pLocalTime->tm_min,pLocalTime->tm_sec);

		rename(FullFileName,destFilename);
		fp = fopen(FullFileName, "w+");
	}
	else{
		fp = fopen(FullFileName, "a+");
	}
	
	if (fp)
	{
		fputs(gsLine,fp);		//write a line to file
		fflush(fp);		//only block device has buffer(cache) requiring flush operation.
		fclose(fp);
		ret=strlen(gsLine);
	}
	
End:
	return ret;
}

//return errcode or written string length
int Log_MsgLine(char *LogFileName,char *sLine)
{
	int ret=0;
	
	pthread_mutex_lock(&gLogThrdLock);
	ret=Log_MsgLine2(gLogDir,LogFileName,sLine,gMaxLogFileSize);
	pthread_mutex_unlock(&gLogThrdLock);
	
	return ret;
}


#ifdef __cplusplus
}
#endif 

#endif	// __LOGFILE_C__
