#ifndef __CFGFILE_H__
#define __CFGFILE_H__

#ifdef __cplusplus
       extern "C" {
#endif

#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>


#define	CFG_FLOCK_BSD	0		//enable flock or not
#define	CFG_FLOCK_POSIX	1		//enable flock or not, using posix standard
#define	CFG_FMagic	0			//check file Magic
#define	CFG_MyFileLock	0		//my file lock for the whole /mnt/mtd/*.conf
#define CFG_MAX_LINE_LENGTH 1024
#define	CFG_ThreadLock	 1	// 0:no lock, 1:lock

#define	NULL_STR	'\0'
#define	NULL_VAL	0xFFFF
#define	MAX_FILE_NAME		128
#define COMMENT_MAGIC_01 ';'
#define COMMENT_MAGIC_02 '#'


#if (CFG_MyFileLock==1)
typedef struct
{
	char LockFlag;
	char FileName[MAX_FILE_NAME];
}MyFileLock;

#define My_RDLCK		1
#define My_WRLCK		2
#define My_UNLCK		0

#define MyMaxCfgFile		32
#endif

/* configuration file entry */
typedef struct TCFGENTRY
{
    char *section;
    char *id;
    char *value;
    char *comment;
    unsigned short flags;
}
TCFGENTRY, *PCFGENTRY;

/* values for flags */
#define CFE_MUST_FREE_SECTION	0x8000
#define CFE_MUST_FREE_ID	0x4000
#define CFE_MUST_FREE_VALUE	0x2000
#define CFE_MUST_FREE_COMMENT	0x1000

/* configuration file */
typedef struct TCFGDATA
  {
    char *fileName;		// Current file name
    int fd;			//file handler, add by yingcai

    int dirty;			// Did we make modifications?

    char *image;		// In-memory copy of the file 
    size_t size;		// Size of this copy (excl. \0) 
    time_t mtime;		// Modification time 

    unsigned int numEntries;
    unsigned int maxEntries;
    PCFGENTRY entries;

    /* Compatibility */
    unsigned int cursor;
    char *section;
    char *id;
    char *value;
    char *comment;
    unsigned short flags;

  }
TCONFIG, *PCONFIG;

#define CFG_VALID		0x8000
#define CFG_EOF			0x4000

#define CFG_ERROR		0x0000
#define CFG_SECTION		0x0001
#define CFG_DEFINE		0x0002
#define CFG_CONTINUE		0x0003

#define CFG_TYPEMASK		0x000F
#define CFG_TYPE(X)		((X) & CFG_TYPEMASK)
#define cfg_valid(X)	((X) != NULL && ((X)->flags & CFG_VALID))
#define cfg_eof(X)	((X)->flags & CFG_EOF)
#define cfg_section(X)	(CFG_TYPE((X)->flags) == CFG_SECTION)
#define cfg_define(X)	(CFG_TYPE((X)->flags) == CFG_DEFINE)
#define cfg_cont(X)	(CFG_TYPE((X)->flags) == CFG_CONTINUE)

int cfg_init (PCONFIG * ppconf, const char *filename, int doCreate);
int cfg_done (PCONFIG pconfig);

int cfg_freeimage (PCONFIG pconfig);
int cfg_refresh (PCONFIG pconfig);
int cfg_storeentry (PCONFIG pconfig, const char *section, const char *id,char *value, 
	char *comment, int dynamic);
int cfg_rewind (PCONFIG pconfig);
int cfg_nextentry (PCONFIG pconfig);
int cfg_find (PCONFIG pconfig, const char *section, const char *id);
int cfg_next_section (PCONFIG pconfig);
int cfg_write (PCONFIG pconfig, const char *section, const char *id, char *value);
int cfg_writelong ( PCONFIG pconfig,const char *section,const char *id,long value);
int cfg_writeint( PCONFIG pconfig,const char *section,const char *id,int value);
int cfg_writeshort ( PCONFIG pconfig,const char *section,const char *id,unsigned short value);
int cfg_writebyte ( PCONFIG pconfig,const char *section,const char *id,unsigned char value);
int cfg_commit (PCONFIG pconfig);
int cfg_copy(PCONFIG pconfig,const char *newfilename);

int cfg_getstring (PCONFIG pconfig, const char *section, const char *id, char *valptr);
int cfg_getlong (PCONFIG pconfig, const char *section,const char *id, long *valptr);
int cfg_getint (PCONFIG pconfig,const char *section,const char *id, int *valptr);
int cfg_getshort (PCONFIG pconfig,const char *section,const char *id, unsigned short *valptr);
int cfg_getbyte(PCONFIG pconfig,const char *section,const char *id, unsigned char *valptr);

int cfg_getstring2(PCONFIG pconfig,const char *section, const char *id, char *valptr, char *defval);
int cfg_getlong2 (PCONFIG pconfig, const char *section, const char *id, long *valptr, long defval);
int cfg_getint2(PCONFIG pconfig, const char *section, const char *id, int *valptr, int defval);
int cfg_getshort2(PCONFIG pconfig, const char *section, const char *id
	, unsigned short *valptr, unsigned short defval);
int cfg_getbyte2(PCONFIG pconfig, const char *section, const char *id
	, unsigned char *valptr, unsigned char defval);

int cfg_get_item (PCONFIG pconfig, char *section, char *id, char * fmt, ...);
int cfg_write_item(PCONFIG pconfig, char *section, char *id, char * fmt, ...);

int list_entries (PCONFIG pCfg, const char * lpszSection, char * lpszRetBuffer, int cbRetBuffer);
int list_sections (PCONFIG pCfg, char * lpszRetBuffer, int cbRetBuffer);

//APIs//decimal number(not 0xHHHH)
int Cfg_ReadStr(const char *File,const char *Section,const char *Entry,char *valptr, char *defval);
int  Cfg_ReadInt(const char *File,const char *Section,const char *Entry,int *valptr,int defVal);
int Cfg_ReadShort(const char *File,const char *Section,const char *Entry,unsigned short *valptr);
int Cfg_Readbyte(const char *File,const char *Section,const char *Entry,unsigned char *valptr);
int Cfg_WriteInt(const char *File,const char *Section,const char *Entry,long Value);
int Cfg_WriteHexInt(const char *File,const char *Section,const char *Entry,int Value);
int Cfg_WriteStr(const char *File,const char *Section,const char *Entry,char *valptr);

/*
	input: int bLock	// if bLock=1 will lock this file.
	return: 0:success, -nnnn:err code.
*/
int ini_setstr(char *filename, char *key, char *val, int bLock);
/*
	input: int bLock	// if bLock=1 will lock this file.
	return: 1:found. 0:not found. -nnnn:err code.
*/
int ini_getstr(char *filename, char *key, char *val, int bLock);

//for compliant purpose
//int SafeWritePrivateProfileString(char *sect,char *key,char *value,char *ini);
int Cfg_WriteString(const char *sect,const char *key,char *value,const char *ini);
//int SafeGetPrivateProfileString(char *sec,char *key,char *def,char *str,unsigned int size,char *ini);
int Cfg_ReadString(const char *sec,const char *key,const char *def,char *str,unsigned int size,const char *ini);

int  Cfg_GetSections(const char *CFG_file,char *sections[]); 
int  Cfg_GetKeys(const char *CFG_file,const char *section, char *keys[]);
int  Cfg_RemoveKey(const char *CFG_file,const char *section,const char *key);


int utl_fgets (const char *fileName,char *line, int bLock);
int utl_fputs (const char *fileName,char *line, int bLock);


#ifdef __cplusplus
}
#endif 

#endif			//__CFGFILE_H__

