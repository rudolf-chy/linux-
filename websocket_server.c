#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include <sys/types.h>
// #include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <sys/sendfile.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#define BUFFER_LENGTH   1024
#define MAX_EPOLL_EVENTS    1024
#define RESOURCE_LENGTH 4096
#define ACCEPT_KEY_LENGTH 64
#define HTTP_METHOD_GET 0
#define HTTP_METHOD_POST 1
#define SERVER_PORT 9999
#define PORT_COUNT 1
#define HTTP_WEB_ROOT "/root/home"
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

enum{
    WS_HANDSHAKE = 0,
    WS_TRANMISSION = 1,
    WS_END = 2,
    WS_COUNT
};

struct ws_ophdr{

    unsigned char opcode: 4,
                rsv3: 1,
                rsv2: 1,
                rsv1: 1,
                fin: 1;
        
    unsigned char pl_len: 7,
                mask: 1;
};

typedef int NCALLBACK(int, int, void*);

struct ntyevent{
    int fd;
    int events;
    void *arg;
    int (*callback)(int fd, int event, void *arg);

    int status;

    int ret_code;

    char rbuffer[BUFFER_LENGTH];
    char wbuffer[BUFFER_LENGTH];

    int rlength;
    int wlength;

    int method;
    char resource[RESOURCE_LENGTH];

    char sec_accept[ACCEPT_KEY_LENGTH];

    int wsstatus;
    char mask_key[4];
};

struct eventblock{
    struct eventblock *next;
    struct ntyevent *events;
};

struct ntyreactor{
    int epfd;
    int blkcnt;

    struct eventblock *evblks;;
};

int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);
struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd);


void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg){

    ev->fd = fd;
    ev->callback = callback;
    ev->events = 0;
    ev->arg = arg;

    return ;
}

int nty_event_add(int epfd, int events, struct ntyevent *ev){

    struct epoll_event ep_ev = {0, {0}};
    ep_ev.data.ptr = ev;
    ep_ev.events = ev->events = events;

    int op;
    if(ev->status == 1){
        op = EPOLL_CTL_MOD;
    }else{
        op = EPOLL_CTL_ADD;
        ev->status = 1;
    }

    if(epoll_ctl(epfd, op, ev->fd, &ep_ev) < 0){
        printf("event add failed [fd=%d], events[%d]\n", ev->fd, events);
        return -1;
    }
    return 0;
}

int nty_event_del(int epfd, struct ntyevent *ev){
    
    struct epoll_event ep_ev = {0, {0}};

    if(ev->status != 1){
        return -1;
    }

    ep_ev.data.ptr = ev;
    ev->status = 0;
    epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &ep_ev);

    return 0;
}

int readline(char *allbuf, int idx, char* linebuf){
    int len = strlen(allbuf);

    for(; idx < len; ++idx){
        if(allbuf[idx] == '\r' && allbuf[idx + 1] == '\n')
            return idx + 2;
        else
            *(linebuf++) = allbuf[idx];
    }

    return -1;
}

int nty_http_request(struct ntyevent *ev){
    
    char linebuffer[1024] = {0};

    int idx = readline(ev->rbuffer, 0, linebuffer);
    if(strstr(linebuffer, "GET")){
        ev->method = HTTP_METHOD_GET;
        int i = 0;
        while(linebuffer[sizeof("GET ") + i] != ' ') i ++;
        linebuffer[sizeof("GET ") + i] = '\0';

        sprintf(ev->resource, "%s/%s", HTTP_WEB_ROOT, linebuffer + sizeof("GET "));
        printf("request uri: %s\n", ev->resource);
    }else if(strstr(linebuffer, "POST")){
        ev->method = HTTP_METHOD_POST;
    }
}

// int nty_http_response_get_method(struct ntyevent *ev){

//     int len;
//     int filefd = open(ev->resource, O_RDONLY);
//     if(filefd == -1){
//         ev->ret_code = 404;
//         const char *html = "<html><head><title>chy</title></head><body><h1>chy</h1></body></html>";
//         ev->wlength = sprintf(ev->wbuffer,
//         "HTTP/1.1 404 Not Found\r\n"
//         "Accept-Ranges: bytes\r\n"
//         "Content-Length: %ld\r\n"
//         "Content-Type: text/html\r\n"
//         "Date: Sat, 06 Aug 2022 13:16:46 GMT\r\n\r\n%s"
//         , strlen(html), html);
//     }else{
//         struct stat stat_buf;
//         fstat(filefd, &stat_buf);
//         close(filefd);
//         if(S_ISDIR(stat_buf.st_mode)){
//             ev->ret_code = 404;
//             const char *html = "<html><head><title>chy</title></head><body><h1>chy</h1></body></html>";
//             ev->wlength = sprintf(ev->wbuffer,
//             "HTTP/1.1 404 Not Found\r\n"
//             "Accept-Ranges: bytes\r\n"
//             "Content-Length: %ld\r\n"
//             "Content-Type: text/html\r\n"
//             "Date: Sat, 06 Aug 2022 13:16:46 GMT\r\n\r\n%s"
//             , strlen(html), html);
//         }else if(S_ISREG(stat_buf.st_mode)){
//             ev->ret_code = 200;

//             ev->wlength = sprintf(ev->wbuffer,
//             "HTTP/1.1 200 OK\r\n"
//             "Accept-Ranges: bytes\r\n"
//             "Content-Length: %ld\r\n"
//             //"Content-Type: text/html\r\n"
//             "Content-Type: image/jpeg\r\n" 
//             "Date: Sat, 06 Aug 2022 13:16:46 GMT\r\n\r\n", stat_buf.st_size);
//         }
//     }

//     return ev->wlength;
// }

int nty_http_response(struct ntyevent *ev){
    if(ev->method == HTTP_METHOD_GET){
        // return nty_http_response_get_method(ev);
    }
    else if(ev->method == HTTP_METHOD_POST){

    }
}

int base64_encode(char *in_str, int in_len, char *out_str) {    
	BIO *b64, *bio;    
	BUF_MEM *bptr = NULL;    
	size_t size = 0;    

	if (in_str == NULL || out_str == NULL)        
		return -1;    

	b64 = BIO_new(BIO_f_base64());    
	bio = BIO_new(BIO_s_mem());    
	bio = BIO_push(b64, bio);
	
	BIO_write(bio, in_str, in_len);    
	BIO_flush(bio);    

	BIO_get_mem_ptr(bio, &bptr);    
	memcpy(out_str, bptr->data, bptr->length);    
	out_str[bptr->length-1] = '\0';    
	size = bptr->length;    

	BIO_free_all(bio);    
	return size;
}

int ws_handshake(struct ntyevent *ev){
    int idx = 0;
    char sec_data[128] = {0};
    char sec_accept[128] = {0};

    do{
        char linebuf[1024] = {0};
        idx = readline(ev->rbuffer, idx, linebuf);

        if(strstr(linebuf, "Sec-WebSocket-Key")){

            strcat(linebuf, GUID);
            SHA1(linebuf + 19, strlen(linebuf + 19), sec_data);
            base64_encode(sec_data, strlen(sec_data), sec_accept);

            memcpy(ev->sec_accept, sec_accept, ACCEPT_KEY_LENGTH);
        }
    } while((ev->rbuffer[idx] != '\r' || ev->rbuffer[idx + 1] != '\n') && idx != -1);
}

void umask(char *payload, int length, char *mask_key){

    int i = 0;

    for(i = 0; i < length; i ++){
        payload[i] ^= mask_key[i % 4];
    }
}

int ws_transmission(struct ntyevent *ev){

    struct ws_ophdr *hdr = (struct ws_ophr*)ev->rbuffer;
    int i = 0;
    hdr->fin = 0;
    unsigned char uchr;
    char *fp = (char *)hdr;
    for(i = 0; i < 2; i ++){
        uchr = *(fp + i);
        printf("%02x", uchr);
    }
    printf("\n");

    if(hdr->pl_len < 126){

        unsigned char *payload = NULL;
        memcpy(ev->wbuffer, ev->rbuffer, BUFFER_LENGTH);
        if(hdr->mask){
            payload = ev->rbuffer + 6;
            umask(payload, hdr->pl_len, ev->rbuffer + 2);
            memcpy(ev->mask_key, ev->rbuffer + 2, 4);
        }else{
            payload = ev->rbuffer + 2;
        }

        printf("payload: %s\n", payload);

    }else if(hdr->pl_len == 126){

    }else if(hdr->pl_len == 127){
        
    }else{
        // assert(0);
    }

}

int ws_request(struct ntyevent *ev){
    if(ev->wsstatus == WS_HANDSHAKE){
        ws_handshake(ev);
        // ev->wsstatus = WS_TRANMISSION;
    }else if(ev->wsstatus == WS_TRANMISSION){
        ws_transmission(ev);
    }
}

int ws_handshake_r(struct ntyevent *ev){
    ev->wlength = sprintf(ev->wbuffer, "HTTP/1.1 101 Switching Protocols\r\n"
                        "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Accept: %s\r\n\r\n", ev->sec_accept);
    return ev->wlength;
}

int ws_response(struct ntyevent *ev){
    printf("wsstatus: %d\n", ev->wsstatus);
    if(ev->wsstatus == WS_HANDSHAKE){
        ws_handshake_r(ev);
        ev->wsstatus = WS_TRANMISSION;
    }
    else if(ev->wsstatus == WS_TRANMISSION){
        struct ws_ophdr *hdr = (struct ws_ophr*)malloc(2);
        int i = 0;
        memset(hdr, 0, 2);
        hdr->fin = 1;
        hdr->opcode = 1;
        hdr->mask = 0;
        hdr->pl_len = 5;
        unsigned char uchr;
        char *fp = (char *)hdr;
        for(i = 0; i < 2; i ++){
            uchr = *(fp + i);
            printf("%02x", uchr);
        }
        printf("\n");

        send(ev->fd, hdr, 2, 0);
        ev->wlength = sprintf(ev->wbuffer, "hello");
        ev->wbuffer[5] = 0;
    }
    return ev->wlength;
}

int recv_cb(int fd, int events, void *arg){
    printf("recv:\n");
    
    struct ntyreactor *reactor = (struct ntyreactor*)arg;
    struct ntyevent *ev = ntyreactor_idx(reactor, fd);

    if(ev == NULL) return -1;

    int len = recv(fd, ev->rbuffer, BUFFER_LENGTH, 0);
    nty_event_del(reactor->epfd, ev);

    if(len > 0){
        ev->rlength = len;
        ev->rbuffer[len] = '\0';
        printf("rbuffer: %s\n", ev->rbuffer);
        ws_request(ev);
        // nty_http_request(ev);

        nty_event_set(ev, fd, send_cb, reactor);
        nty_event_add(reactor->epfd, EPOLLOUT, ev);
    }else if(len == 0){
        nty_event_del(reactor->epfd, ev);
        ev->wsstatus = 0;
        close(ev->fd);
    }else{
        if(errno == EAGAIN && errno == EWOULDBLOCK){

        }else if(errno == ECONNRESET){
            ev->wsstatus = 0;
            nty_event_del(reactor->epfd, ev);
            close(ev->fd);
        }
    }

    return len;
}

int send_cb(int fd, int events, void *arg){
    printf("send:\n");

    struct ntyreactor *reactor = (struct ntyreactor*)arg;
    struct ntyevent *ev = ntyreactor_idx(reactor, fd);

    if(ev == NULL) return -1;

    // nty_http_response(ev);
    ws_response(ev);
    printf("length: %d, wbuffer: %s\n", ev->wlength, ev->wbuffer);
    int len = send(fd, ev->wbuffer, ev->wlength, 0);
    if(len > 0){
        
        // if(ev->ret_code == 200){
        //     int filefd = open(ev->resource, O_RDONLY);

        //     struct stat stat_buf;
        //     fstat(filefd, &stat_buf);

        //     int flag = fcntl(fd, F_GETFL, 0);
        //     flag &= ~O_NONBLOCK;
        //     fcntl(fd, F_SETFL, flag);

        //     int ret = sendfile(fd, filefd, NULL, stat_buf.st_size);
        //     printf("file size: %d\n", stat_buf.st_size);
        //     if(ret == -1){
        //         printf("sendfile: errno: %d\n", errno);
        //     }

        //     flag |= O_NONBLOCK;
        //     fcntl(fd, F_SETFL, flag);

        //     close(filefd);
        // }

        // send(fd, "\r\n", 2, 0);
        // printf("send[fd=%d], [%d]%s\n", fd, len, ev->wbuffer);

        nty_event_del(reactor->epfd, ev);
        nty_event_set(ev, fd, recv_cb, reactor);
        nty_event_add(reactor->epfd, EPOLLIN, ev);

    }else{
        
        nty_event_del(reactor->epfd, ev);
        close(ev->fd);
    }

    return len;
}

int curfds = 0;

#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

struct timeval tv_begin;

int accept_cb(int fd, int events, void *arg){

    struct ntyreactor *reactor = (struct ntyreactor*)arg;
    if(reactor == NULL) return -1;

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

    int clientfd;

    if((clientfd = accept(fd, (struct sockaddr*)&client_addr, &len)) == -1){
        if(errno != EAGAIN && errno != EINTR){

        }
        printf("accept: %s\n", strerror(errno));
        return -1;
    }

    int flag = 0;
    if((flag = fcntl(clientfd, F_SETFL, O_NONBLOCK)) < 0){
        printf("%s: fcntl nonblocking failed, %d\n", __func__, MAX_EPOLL_EVENTS);
        return -1;
    }

    struct ntyevent *event = ntyreactor_idx(reactor, clientfd);

    if(event == NULL) return -1;

    nty_event_set(event, clientfd, recv_cb, reactor);
    nty_event_add(reactor->epfd, EPOLLIN, event);

    if(curfds++ % 1000 == 999){
        struct timeval tv_cur;
        memcpy(&tv_cur, &tv_begin, sizeof(struct timeval));

        gettimeofday(&tv_begin, NULL);

        int time_used = TIME_SUB_MS(tv_begin, tv_cur);
        printf("connections: %d, sockfd: %d, time_used:%d\n", curfds, clientfd, time_used);
    }

    return 0;
}

int init_sock(short port){

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    if(listen(fd, 20) < 0){
        printf("listen failed : %s\n", strerror(errno));
        return -1;
    }

    printf("listen server port : %d\n", port);
    gettimeofday(&tv_begin, NULL);

    return fd;
}

int ntyreactor_alloc(struct ntyreactor *reactor){
    
    if(reactor == NULL) return -1;
    if(reactor->evblks == NULL) return -1;

    struct eventblock *blk = reactor->evblks;

    while(blk->next != NULL){
        blk = blk->next;
    }

    struct ntyevent *evs =(struct ntyevent*)malloc((MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
    if(evs == NULL){
        printf("ntyreactor_alloc ntyevent failed\n");
        return -2;
    }
    memset(evs, 0, (MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));

    struct eventblock *block = malloc(sizeof(struct eventblock));
    if(block == NULL){
        printf("ntyreactor_alloc eventblock failed\n");
        return -3;
    }
    block->events = evs;
    block->next = NULL;

    blk->next = block;
    reactor->blkcnt ++;

    return 0;
}

struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd){

    if(reactor == NULL) return NULL;
    if(reactor->evblks == NULL) return NULL;

    int blkidx = sockfd / MAX_EPOLL_EVENTS;
    while(blkidx >= reactor->blkcnt){
        ntyreactor_alloc(reactor);
    }

    int i = 0;
    struct eventblock *blk = reactor->evblks;
    while(i ++ != blkidx && blk != NULL){
        blk = blk->next;
    }
    
    return &blk->events[sockfd % MAX_EPOLL_EVENTS];
}

int ntyreactor_init(struct ntyreactor *reactor){

    if(reactor == NULL) return -1;
    memset(reactor, 0, sizeof(struct ntyreactor));

    reactor->epfd = epoll_create(1);
    if(reactor->epfd <= 0){
        printf("create epfd in %s err %s\n", __func__, strerror(errno));
        return -2;
    }

    struct ntyevent* evs = (struct ntyevent*)malloc((MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
    if(evs == NULL){
        printf("create epfd in %s err %s\n", __func__, strerror(errno));
        close(reactor->epfd);
        return -3;
    }
    memset(evs, 0, (MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));

    struct eventblock *block = malloc(sizeof(struct eventblock));
    if(block == NULL){
        free(evs);
        close(reactor->epfd);
        return -3;
    }
    block->events = evs;
    block->next = NULL;

    reactor->evblks = block;
    reactor->blkcnt = 1;

    return 0;
}

int ntyreactor_destroy(struct ntyreactor *reactor){
    
    close(reactor->epfd);

    struct eventblock *blk = reactor->evblks;
    struct eventblock *blk_next;
    while(blk != NULL){
        blk_next = blk->next;

        free(blk->events);
        free(blk);

        blk = blk_next;
    }

    return 0;
}

int ntyreactor_addlistener(struct ntyreactor *reactor, int sockfd, NCALLBACK *acceptor){
    
    if(reactor == NULL) return -1;
    if(reactor->evblks == NULL) return -1;

    struct ntyevent *event = ntyreactor_idx(reactor, sockfd);
    if(event == NULL) return -1;

    nty_event_set(event, sockfd, acceptor, reactor);
    nty_event_add(reactor->epfd, EPOLLIN, event);

    return 0;
}

int ntyreactor_run(struct ntyreactor *reactor){
    if(reactor == NULL) return -1;
    if(reactor->epfd < 0) return -1;
    if(reactor->evblks == NULL) return -1;

    struct epoll_event events[MAX_EPOLL_EVENTS + 1];

    int checkpos = 0, i;

    while(1){

        int nready = epoll_wait(reactor->epfd, events, MAX_EPOLL_EVENTS, 1000);
        if(nready < 0){
            printf("epoll_wait error, exit\n");
            continue;
        }

        for(i = 0; i < nready; i ++){

            struct ntyevent *ev = (struct ntyevent*)events[i].data.ptr;

            if((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)){
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
            if((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT)){
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
        }
    }
}

int main(int argc, char *argv[]){

    struct ntyreactor *reactor = (struct ntyreactor*)malloc(sizeof(struct ntyreactor));
    ntyreactor_init(reactor);

    unsigned short port = SERVER_PORT;
    if(argc == 2){
        port = atoi(argv[1]);
    }

    int i = 0;
    int sockfds[PORT_COUNT] = {0};

    for(i = 0; i < PORT_COUNT; i ++){
        sockfds[i] = init_sock(port + i);
        ntyreactor_addlistener(reactor, sockfds[i], accept_cb);
    }

    ntyreactor_run(reactor);

    ntyreactor_destroy(reactor);

    for(i = 0; i < PORT_COUNT; i ++){
        close(sockfds[i]);
    }
    free(reactor);

    return 0;
}