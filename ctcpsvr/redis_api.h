#ifndef __REDIS_API_H__
#define __REDIS_API_H__
#include <hiredis.h>

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include "net.h"
#include <string>
#include <unordered_map>
#include <list>
#include <vector>

#ifdef __cplusplus
       extern "C" {
#endif

	#define REDIS_SERVER "127.0.0.1"
	#define REDIS_SERVER_PORT 6379
	#define REDIS_DVR_DB 6
	#define REDIS_QUEUE_STAT "queue_stat"
	
	typedef struct{
		redisContext *hdl;
		pthread_mutex_t ThrdLock;
		int db;
	}RedisConn_T;

	int redis_Uninit();
	int redis_Init(char *hostname,int port,char *authpass,int db);
	int redis_flushdb();
	int redis_queue_rpush(const char *key, char *value);
	int redis_queue_lpop(const char *key, char *value);
	int redis_queue_flush(const char *key);
	int redis_hset(const char *key, const char *field, char *value);
	int redis_hget(const char *key, const char *field, char *value);	
	int redis_append(const char *key, const char *value);
	int redis_setex(const char *key, const char *value, int exptime=3600);
	char *redis_get(const char *key);
	int redis_del(const char *key);

	int redis6_append(const char *key, const char *value);
	char *redis6_get(const char *key);
	int redis6_del(const char *key);

	#define MAX_TABLE_NUM 100
	#define MAX_QUEUE_SIZE 2000	
	#define KEY_ADD_URL_QUE "addurlque"
	#define KEY_DEL_URL_QUE "delurlque"
	#define KEY_SEGMENTER_QUE "segmenterque"

	typedef struct{
		int online;
		char stream_uri[64];
		char stream_type[16];
		char clt_ip[24];
		int64_t start_ts;
		int64_t end_ts;
		unsigned int duration;
	}CLIENT_CONN_S,*CLIENT_CONN_PTR;
	extern std::unordered_map<std::string, CLIENT_CONN_PTR> g_client_conn_map;
	extern std::list<std::string> g_client_history_list;

	typedef struct{
		int online;
		char stream_uri[64];
		char publisher_ip[24];
		int64_t start_ts;
		int64_t end_ts;
		unsigned int duration;
	}PUBLISHER_CONN_S,*PUBLISHER_CONN_PTR;
	extern std::unordered_map<std::string, PUBLISHER_CONN_PTR> g_publisher_conn_map;
	extern std::list<std::string> g_publisher_history_list;

#ifdef __cplusplus
}
#endif 
#endif

