#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "logfile.h"
#include "redis_api.h"

#if 1
#define 	CachePrint(fmt,args...)  do{ \
			char _PrtBuf[1000]; \
			sprintf(_PrtBuf,":" fmt , ## args); \
			Log_MsgLine("redis-svr.log",_PrtBuf); \
			}while(0)
#else
#define 	CachePrint(fmt,args...)
#endif

std::unordered_map<std::string, CLIENT_CONN_PTR> g_client_conn_map;
std::list<std::string> g_client_history_list;
std::unordered_map<std::string, PUBLISHER_CONN_PTR> g_publisher_conn_map;
std::list<std::string> g_publisher_history_list;


static RedisConn_T gRedisConn;
static int gRedisQueueSize=0;
static char gredisHost[128],gredisPass[128];
static int gredisPort,gredisDb;

enum connection_type{
	CONN_TCP,
	CONN_UNIX,
	CONN_FD

};

typedef struct config{
	enum connection_type type;
	struct {
		const char *host;
		int port;
		struct timeval timeout;
	}tcp;
	struct {
		const char *path;
	}unix_sock;

}RedisCfg_T;

static long long  usec(){
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return ((long long)tv.tv_sec*1000000) + tv.tv_usec;
}

#ifdef NDEBUG
	#undef assert
	#define assert(e) void(e)
#endif

static redisContext *_select_database(redisContext *c)
{
    redisReply *reply;

    reply = (redisReply *)redisCommand(c,"SELECT 9");
    assert(reply != NULL);
    freeReplyObject(reply);

    reply = (redisReply *)redisCommand(c,"DBSIZE");
    assert(reply != NULL);
    if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 0) {
        freeReplyObject(reply);
    } else {
        CachePrint("Database #9 is not empty, test can not continue\n");
		return NULL;
	}

    return c;
}

static int _disconnect(redisContext *c, int keep_fd)
{
//    redisReply *reply;

//    reply = redisCommand(c,"SELECT 9");
//    assert(reply != NULL);
//    freeReplyObject(reply);
//    reply = redisCommand(c,"FLUSHDB");
//    assert(reply != NULL);
//    freeReplyObject(reply);

    /* Free the context as well, but keep the fd if requested. */
    if (keep_fd)
        return redisFreeKeepFd(c);
    redisFree(c);
    return -1;
}

static redisContext *_connect(struct config config)
{
    redisContext *c = NULL;

    if (config.type == CONN_TCP) {
        c = redisConnect(config.tcp.host, config.tcp.port);
    } else if (config.type == CONN_UNIX) {
        c = redisConnectUnix(config.unix_sock.path);
    } else if (config.type == CONN_FD) {

        redisContext *dummy_ctx = redisConnectUnix(config.unix_sock.path);
        if (dummy_ctx) {
            int fd = _disconnect(dummy_ctx, 1);
            printf("Connecting to inherited fd %d\n", fd);
            c = redisConnectFd(fd);
        }
    } else {
        assert(NULL);
    }

    if (c == NULL) {
        printf("Connection error: can't allocate redis context\n");
        exit(1);
    } else if (c->err) {
        printf("Connection error: %s\n", c->errstr);
        redisFree(c);
        exit(1);
    }

    return _select_database(c);
}

int redis_Uninit()
{
	if (gRedisConn.hdl){
		redisFree(gRedisConn.hdl); gRedisConn.hdl=NULL;
		pthread_mutex_destroy(&gRedisConn.ThrdLock);
	}
	return 0;
}


int redis_Init(char *hostname,int port,char *authpass,int db)
{
	int ret=0;
    redisContext *hdl=NULL;
    redisReply *reply=NULL;
    struct timeval timeout={1,500000};

	if (gRedisConn.hdl){
		return 0;
	}

	do {
		hdl=redisConnectWithTimeout(hostname,port,timeout);
		if (!hdl){
			printf("redis connect timeout!\n");
			ret=-2;break;
		}
		if (hdl&&(hdl->err)){
			printf("redis connect error[%d]:%s\n", hdl->err,hdl->errstr);
			ret=-3;break;
		}
		if ((authpass)&&(strlen(authpass)>1)){
			reply=(redisReply*)redisCommand(hdl,"AUTH %s", authpass);
			if (!reply){
				ret=-4;break;
			}
			if (reply->type == REDIS_REPLY_ERROR){
				printf("AUTH error:%s\n", reply->str);
				freeReplyObject(reply);
				ret=-5;break;
			}
			freeReplyObject(reply);
		}

		reply=(redisReply*)redisCommand(hdl,"select %d", db);
		freeReplyObject(reply);

		reply=(redisReply*)redisCommand(hdl,"flushdb");
		freeReplyObject(reply);
		strncpy(gredisHost,hostname,sizeof(gredisHost));
		gredisPort=port;
		if (gredisPass)
			strncpy(gredisPass,authpass,sizeof(gredisPass));
		gredisDb=db;
	}while(0);

	if (ret<0){
		if (hdl)
			redisFree(hdl);
	}
	else{
		pthread_mutex_init(&gRedisConn.ThrdLock,NULL);
		gRedisConn.hdl=hdl;
		gRedisConn.db=db;
	}

	return ret;
}

int redis_flushdb()
{
	redisReply *reply=NULL;
	redisContext *hdl=gRedisConn.hdl;

	if (hdl){
		pthread_mutex_lock(&gRedisConn.ThrdLock);
		reply=(redisReply*)redisCommand(hdl,"flushdb");
		freeReplyObject(reply);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
	}
	return 0;
}

int redis_queue_rpush(const char *key, char *value)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;

	if ((!key)||(!value)||(!gRedisConn.hdl))
		return -1;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	if (gRedisQueueSize>=MAX_QUEUE_SIZE){
		CachePrint("warn: redis-queue(%s) is full.\n",key);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		return -11;
	}

	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"rpush %s %s",key,value);
	if (reply){
		CachePrint("rpush(%s,%s)\n",key,value);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			if (reply->type == REDIS_REPLY_INTEGER)
				ret=reply->integer;
			else
				ret=-4;
		}
		freeReplyObject(reply);
	}else{
		ret=-2;
		CachePrint("%s(%s,%s) failed!\n",__FUNCTION__,key,value);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return -2;
	}
	t2 = usec();
	//CachePrint("rpush cost=%.3f ms\n",(t2-t1)/1000.0);
	gRedisQueueSize++;
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return ret;
}

int redis_queue_lpop(const char *key, char *value)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;

	if ((!key)||(!value)||(!gRedisConn.hdl))
		return -1;
	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"lpop %s",key);
	if (reply){
		//CachePrint("lpop(%s)=%s\n",key,reply->str);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			if (!reply->str)
				ret=-4;
			else{
				strcpy(value,reply->str);
				ret=1;
				//CachePrint("lpop(key=%s),value=%s\n",key,value);
			}
		}
		freeReplyObject(reply);
	}else{
		ret=-2;
		CachePrint("%s(%s) failed!\n",__FUNCTION__,key);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return -2;
	}
	t2 = usec();
	//CachePrint("lpop cost=%.3f ms\n",(t2-t1)/1000.0);
	gRedisQueueSize--;
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return ret;
}

int redis_queue_flush(const char *key)
{
	redisReply *reply=NULL;

	if ((!gRedisConn.hdl))
		return -1;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	while(1)
	{
		reply=(redisReply*)redisCommand(gRedisConn.hdl,"lpop %s",key);
		if (reply){
			if (reply->type == REDIS_REPLY_ERROR){
				freeReplyObject(reply);
				break;
			}
			else{
				if (!reply->str){
					freeReplyObject(reply);
					break;
				}
			}
			freeReplyObject(reply);
		}else{
			break;
		}
	}
	//CachePrint("%s(%s)\n",__FUNCTION__,key);
	gRedisQueueSize=0;
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return 0;
}

int redis_hset(const char *key, const char *field, char *value)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;

	if ((!key)||(!field)||(!value)||(!gRedisConn.hdl))
		return -1;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"hset %s %s %s",key,field,value);
	if (reply){
		CachePrint("hset(%s,%s,%s)\n",key,field,value);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			if (reply->type == REDIS_REPLY_INTEGER)
				ret=reply->integer;
			else
				ret=-4;
		}
		freeReplyObject(reply);
	}else{
		ret=-2;
		CachePrint("%s(%s,%s,%s) failed!\n",__FUNCTION__,key,field,value);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return -2;
	}
	t2 = usec();
	//CachePrint("hset cost=%.3f ms\n",(t2-t1)/1000.0);
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return ret;
}

int redis_hget(const char *key, const char *field, char *value)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;

	if ((!key)||(!field)||(!value)||(!gRedisConn.hdl))
		return -1;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"hget %s %s",key,field);
	if (reply){
		CachePrint("hget(%s,%s),value=%s\n",key,field,reply->str);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			if (!reply->str)
				ret=-4;
			else{
				strcpy(value,reply->str);
				ret=1;
			}
		}
		freeReplyObject(reply);
	}else{
		ret=-2;
		CachePrint("%s(%s,%s) failed!\n",__FUNCTION__,key,field);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return -2;
	}
	t2 = usec();
	CachePrint("hget cost=%.3f ms\n",(t2-t1)/1000.0);
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return ret;
}

int redis_append(const char *key, const char *value)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;

	if ((!key)||(!value)||(!gRedisConn.hdl))
		return -1;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"append %s %s,",key,value);
	if (reply){
		CachePrint("append(db=%d,%s,%s)\n",gredisDb,key,value);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			if (reply->type == REDIS_REPLY_INTEGER)
				ret=reply->integer;
			else
				ret=-4;
		}
		freeReplyObject(reply);
	}else{
		ret=-2;
		CachePrint("%s(%s,%s) failed!\n",__FUNCTION__,key,value);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return -2;
	}
	t2 = usec();
	//CachePrint("append cost=%.3f ms\n",(t2-t1)/1000.0);
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return ret;
}

int redis_setex(const char *key, const char *value, int exptime)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;

	if ((!key)||(!value)||(!gRedisConn.hdl))
		return -1;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"setex %s %d %s",key,exptime,value);
	if (reply){
		CachePrint("setex(%s,%d,%s)\n",key,exptime,value);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			ret=1;
		}
		freeReplyObject(reply);
	}else{
		ret=-2;
		CachePrint("%s(%s,%s,%d) failed!\n",__FUNCTION__,key,value,exptime);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return -2;
	}
	t2 = usec();
	//CachePrint("setex cost=%.3f ms\n",(t2-t1)/1000.0);
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return ret;
}

char *redis_get(const char *key)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;
	char *pVal=NULL;

	if ((!key)||(!gRedisConn.hdl))
		return NULL;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"get %s",key);
	if (reply){
		CachePrint("get(%s),value=%s\n",key,reply->str);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			if (!reply->str)
				ret=-4;
			else{
				ret=strlen(reply->str);
				pVal=(char *)malloc(ret+1);
				if (pVal)
					strcpy(pVal,reply->str);
				else
					ret=-5;
			}
		}
		freeReplyObject(reply);
	}else{
		ret=-2;
		CachePrint("%s(%s) failed!\n",__FUNCTION__,key);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return NULL;
	}
	t2 = usec();
	//CachePrint("get cost=%.3f ms\n",(t2-t1)/1000.0);
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return pVal;
}

int redis_del(const char *key)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;

	if ((!key)||(!gRedisConn.hdl))
		return -1;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"del %s",key);
	if (reply){
		CachePrint("del(%s):%ld\n",key,reply->integer);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			ret=reply->integer;
		}
		freeReplyObject(reply);
	}else{
		ret=-2;
		CachePrint("%s(%s) failed!\n",__FUNCTION__,key);
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return -2;
	}
	t2 = usec();
	CachePrint("del cost=%.3f ms\n",(t2-t1)/1000.0);
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return ret;
}

int redis6_append(const char *key, const char *value)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;

	if ((!key)||(!value)||(!gRedisConn.hdl)||strlen(key)==0)
		return -1;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();

	reply=(redisReply*)redisCommand(gRedisConn.hdl,"select %d",REDIS_DVR_DB);
	if (!reply){
		CachePrint("select db-6 failed!\n");
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return NULL;
	}
	freeReplyObject(reply);

	reply=(redisReply*)redisCommand(gRedisConn.hdl,"append %s %s,",key,value);
	if (reply){
		CachePrint("append(db=6,%s,%s)\n",key,value);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			if (reply->type == REDIS_REPLY_INTEGER)
				ret=reply->integer;
			else
				ret=-4;
		}
		freeReplyObject(reply);
	}else{
		ret=-5;
		CachePrint("%s(%s,%s) failed!\n",__FUNCTION__,key,value);
	}
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"select %d",gRedisConn.db);
	if (!reply){
		CachePrint("select db-6 failed!\n");
	}
	freeReplyObject(reply);

	t2 = usec();
	//CachePrint("append cost=%.3f ms\n",(t2-t1)/1000.0);
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return ret;
}

char *redis6_get(const char *key)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;
	char *pVal=NULL;

	if ((!key)||(!gRedisConn.hdl))
		return NULL;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"select %d",REDIS_DVR_DB);
	if (!reply){
		CachePrint("select db-6 failed!\n");

		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return NULL;
	}
	freeReplyObject(reply);

	reply=(redisReply*)redisCommand(gRedisConn.hdl,"get %s",key);
	if (reply){
		//CachePrint("get(%s),value=%s\n",key,reply->str);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			if (!reply->str)
				ret=-4;
			else{
				ret=strlen(reply->str);
				pVal=(char *)malloc(ret+1);
				if (pVal)
					strcpy(pVal,reply->str);
				else
					ret=-5;
			}
		}
		freeReplyObject(reply);
	}else{
		ret=-5;
		CachePrint("%s(%s) failed!\n",__FUNCTION__,key);
	}
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"select %d",gRedisConn.db);
	if (!reply){
		CachePrint("select db failed!\n");
	}
	freeReplyObject(reply);

	t2 = usec();
	//CachePrint("get cost=%.3f ms\n",(t2-t1)/1000.0);
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return pVal;
}

int redis6_del(const char *key)
{
	int ret=-1;
	redisReply *reply=NULL;
	long long t1,t2;

	if ((!key)||(!gRedisConn.hdl))
		return -1;

	pthread_mutex_lock(&gRedisConn.ThrdLock);
	t1=usec();
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"select %d",REDIS_DVR_DB);
	if (!reply){
		CachePrint("select db-6 failed!\n");
		pthread_mutex_unlock(&gRedisConn.ThrdLock);
		redis_Uninit();
		ret=redis_Init(gredisHost,gredisPort,gredisPass,gredisDb);
		CachePrint("redis_Init(%s:%d,%s,db=%d),ret=%d\n",gredisHost,gredisPort,gredisPass,gredisDb,ret);
		return -2;
	}
	freeReplyObject(reply);

	reply=(redisReply*)redisCommand(gRedisConn.hdl,"del %s",key);
	if (reply){
		CachePrint("del(%s):%ld\n",key,reply->integer);
		if (reply->type == REDIS_REPLY_ERROR){
			ret=-3;
		}
		else{
			ret=reply->integer;
		}
		freeReplyObject(reply);
	}else{
		ret=-2;
		CachePrint("%s(%s) failed!\n",__FUNCTION__,key);
	}
	reply=(redisReply*)redisCommand(gRedisConn.hdl,"select %d",gRedisConn.db);
	if (!reply){
		CachePrint("select db failed!\n");
	}
	freeReplyObject(reply);

	t2 = usec();
	//CachePrint("del cost=%.3f ms\n",(t2-t1)/1000.0);
	pthread_mutex_unlock(&gRedisConn.ThrdLock);

	return ret;
}

