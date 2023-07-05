#ifndef __LOGFILE_H__
#define __LOGFILE_H__

#ifdef __cplusplus
       extern "C" {
#endif

#define KB 1000
#define MB 1000000
#define	LOG_FILE_LIMIT	(10*MB)
#define	LOG_FILE_DIR	"./logs/"
#define LOG_LINE_LIMIT 	1000

int Log_Init(char *LogDir,unsigned int MaxLogFileSize);
int Log_UnInit(void);

//return errcode or written string length
int Log_MsgLine(char *LogFileName,char *sLine);

//return errcode or written string length
int Log_MsgLine2(char *Dir,char *FileName,char *sLine,unsigned int FileSizeLimit);

#ifdef __cplusplus
}
#endif 

#endif			//__LOGFILE_H__

