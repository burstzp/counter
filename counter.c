#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <evhtp.h>
#include <tcbdb.h>
#include<fcntl.h>
#include<sys/stat.h>
#include <signal.h>
evbase_t *evbase;
TCBDB *db;

#define FILE_MODE  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define PIDFILE "/var/run/zqueue.pid"

#define SERVER "kv server" 

#define write_lock(fd, offset, whence, len) \
                lock_reg(fd, F_SETLK, F_WRLCK, offset, whence, len)

static const char * method_strmap[] = {
    "GET",
    "HEAD",
    "POST",
    "PUT",
    "DELETE",
    "MKCOL",
    "COPY",
    "MOVE",
    "OPTIONS",
    "PROPFIND",
    "PROPATCH",
    "LOCK",
    "UNLOCK",
    "TRACE",
    "CONNECT",
    "PATCH",
    "UNKNOWN",
};

int
lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
    struct flock lock;
    lock.l_type = type;
    lock.l_start = offset;
    lock.l_whence = whence;
    lock.l_len = len;
    return( fcntl(fd, cmd, &lock) );
}

char *urldecode(char *input_str) 
{
    int len = strlen(input_str);
    char *str = strdup(input_str);

    char *dest = str; 
    char *data = str; 

    int value; 
    int c; 

    while (len--) { 
        if (*data == '+') { 
            *dest = ' '; 
        } 
        else if (*data == '%' && len >= 2 && isxdigit((int) *(data + 1)) 
                && isxdigit((int) *(data + 2))) 
        { 

            c = ((unsigned char *)(data+1))[0]; 
            if (isupper(c)) 
                c = tolower(c); 
            value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16; 
            c = ((unsigned char *)(data+1))[1]; 
            if (isupper(c)) 
                c = tolower(c); 
            value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10; 

            *dest = (char)value ; 
            data += 2; 
            len -= 2; 
        } else { 
            *dest = *data; 
        } 
        data++; 
        dest++; 
    } 
    *dest = '\0'; 
    return str; 
}

void
handler(evhtp_request_t * req, void * a) {
    const char *uri;
    uri = req->uri->path->full;
    if (strcmp(uri, "/favicon.ico") == 0) {
        evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", SERVER, 0, 1));
        evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "text/html", 0, 0));
        evhtp_send_reply(req, EVHTP_RES_OK);
        
        return;
    }
    
    evhtp_kvs_t *params;
    params = req->uri->query;
    const char *str_k = evhtp_kv_find(params, "k");
    char *str_v = evhtp_kv_find(params, "v");
   	const char *step = evhtp_kv_find(params, "step");
   
    char buf[1024];
    if (!strcmp(uri,"/add") && str_k != NULL && str_v != NULL)  {
        tcbdbput2(db, str_k, urldecode(str_v)); 
        sprintf(buf, "%s", "add ok!");
    } else if (!strcmp(uri, "/get") && str_k != NULL) {
        char *tmp = tcbdbget2(db, str_k);
        if (tmp != NULL) {
            sprintf(buf, "%s", tmp);
            free(tmp);
        }
    } else if (strcmp(uri, "/increment") == 0 && str_k != NULL) {
        str_v = tcbdbget2(db, str_k);
    	unsigned long long counter = (str_v) ? atoll(str_v) : 0;
    	if (step != NULL) counter += atol(step);
    	else counter++;
        
		snprintf (buf, sizeof(buf), "%ld", counter);
        tcbdbput2(db, str_k, buf); 
	} else if (strcmp(uri, "/decrement") == 0) {
    	str_v = tcbdbget2(db, str_k);
    	unsigned long long counter = (str_v) ? atoll(str_v) : 0;
        if (counter >0) {
            if (step != NULL) counter -= atol(step);
            else counter--;

            snprintf (buf, sizeof(buf), "%ld", counter);
            tcbdbput2(db, str_k, buf); 
        }
	} 
    
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Server", SERVER, 0, 1));
    evhtp_headers_add_header(req->headers_out, evhtp_header_new("Content-Type", "text/html", 0, 0));
    if (buf != NULL) {
        evbuffer_add_reference(req->buffer_out, buf, strlen(buf), NULL, NULL);
    } else {
        evbuffer_add_reference(req->buffer_out, "NULL", 4, NULL, NULL);
    }
    
    evhtp_send_reply(req, EVHTP_RES_OK);
}
/* 定时同步线程，定时将内存中的内容写入磁盘 */
static void sync_worker(const int sig) {
    pthread_detach(pthread_self());

    while(1) {
        /* 间隔httpsqs_settings_syncinterval秒同步一次数据到磁盘 */
        sleep(10);

        /* 同步内存数据到磁盘 */
        tcbdbsync(db);
    }
}

int is_already_running() {
    int fd, val;
    char buf[10];
    if ((fd = open(PIDFILE, O_WRONLY | O_CREAT, FILE_MODE)) < 0) {
        perror("open pidfile error");
    }

    if (write_lock(fd, 0, SEEK_SET, 0) < 0){
        if (errno == EACCES || errno == EAGAIN)
            return -1;
        else
            perror("lock pid error");
    }

    if (ftruncate(fd, 0) < 0) perror("truncate pid file error");

    sprintf(buf, "%d\n", getpid());
    if (write(fd, buf, strlen(buf)) != strlen(buf)) perror("write pid file error");

    if ( (val = fcntl(fd, F_GETFD, 0)) < 0)
        perror("fcntl F_GETFD error");
    val |= FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, val) < 0)
        perror("fcntl F_SETFD error");

    return 0;
}
//当向进程发出SIGTERM/SIGHUP/SIGINT/SIGQUIT的时候，终止event的事件侦听循环
void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGHUP:
        case SIGQUIT:
        case SIGINT:
            event_base_loopbreak(evbase);  //终止侦听event_dispatch()的事件侦听循环，执行之后的代码
            tcbdbsync(db);
            tcbdbclose(db);
            break;
    }
}
void show_help() {
    char *help = "written by guopan1\n\n"
        "-l <ip_addr> interface to listen on, default is 0.0.0.0\n"
        "-p <num>     port number to listen on, default is 1984\n"
        "-d           run as a deamon\n"
        "-t <second>  timeout for a http request, default is 120 seconds\n"
        "-h           print this help and exit\n"
        "\n";
    fprintf(stderr, help);
}

int main (int argc, char * const * argv)
{
    //自定义信号处理函数
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    
    //默认参数
    char *httpd_option_listen = "0.0.0.0";
    int httpd_option_port = 8080;
    int httpd_option_daemon = 0;
    int httpd_option_timeout = 120; //in seconds

    //获取参数
    int c;
    while ((c = getopt(argc, argv, "l:p:dt:h")) != -1) {
        switch (c) {
            case 'l' :
                httpd_option_listen = optarg;
                break;
            case 'p' :
                httpd_option_port = atoi(optarg);
                break;
            case 'd' :
                httpd_option_daemon = 1;
                break;
            case 't' :
                httpd_option_timeout = atoi(optarg);
                break;
            case 'h' :
            default :
                show_help();
                exit(EXIT_SUCCESS);
        }
    }

    //判断是否设置了-d，以daemon运行
    if (httpd_option_daemon) {
        pid_t pid;
        pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            //生成子进程成功，退出父进程
            exit(EXIT_SUCCESS);
        }
    }
    
    if (is_already_running()) {
        perror("daemon is already running");
        exit(EXIT_FAILURE);
    }

    const char *datapath = "/tmp/keyword.db";
    db = tcbdbnew();
    tcbdbsetmutex(db);
    tcbdbtune(db,128,256,32749,8,10,100);  
    tcbdbsetcache(db,1024,512);     
    tcbdbsetxmsiz(db,1024100);
    if (!tcbdbopen(db,datapath,BDBOWRITER|BDBOCREAT)){
        fprintf(stderr,"It is failure to open a database !\n");
        exit(1);
    }

    pthread_t sync_worker_tid;
    pthread_create(&sync_worker_tid, NULL, (void *) sync_worker, NULL);

    //evbase_t 
    evbase = event_base_new();
    evhtp_t *htp = evhtp_new(evbase, NULL);

    evhtp_set_cb(htp, "/", handler, NULL); /* 设置回调函数 */
    evhtp_use_threads(htp, NULL, 4, NULL); /* 设置4个线程 */

    /* 监听本地所有IP的8080端口, backlog为1024 */
    evhtp_bind_socket(htp, httpd_option_listen, httpd_option_port, 1024);

    /* 进入循环、监听连接，http server开始工作 */
    event_base_loop(evbase, 0);

    return 0;
}
