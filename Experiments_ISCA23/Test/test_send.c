#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/aes.h>

#include <gem5/m5ops.h>

#define GEM5
#define CONNECT
//#define SENDTO

#define PORT 35001

#ifdef CONNECT
#define HOOK_PORT 30800
#define NUM_HOOK_SOCKETS 5
struct sockaddr_in hook_serv_addr[NUM_HOOK_SOCKETS];
struct sockaddr *hook_addr;
int hook_success;
#endif

#ifdef SENDTO
#define HOOK_MIN 33
#define HOOK_MAX 126
char *hook_buf;
char *hook_buf_ptr;
int hook_bytes_sent;
#endif



#ifdef CONNECT
int connect_hook(int sockfd, struct sockaddr *servaddr, int addrlen) {
    //hook_addr = (struct sockaddr *)((long)hook_serv_addr + addrlen * (rand() % NUM_HOOK_SOCKETS));
    hook_addr = (struct sockaddr *)((long)hook_serv_addr);
    memcpy(servaddr, hook_addr, addrlen);
    hook_success = connect(sockfd, servaddr, addrlen);
    //hook_success = 1;
    return hook_success;
}
#endif

#ifdef SENDTO
int sendto_hook(int sockfd, char *buf, size_t len, unsigned int flags) { //These are optional and not relevant to current hooking->struct sockaddr * servaddr, int addrlen) {
    //hook_buf_ptr = (char *)((long)hook_buf + rand() % (1024-len));
    hook_buf_ptr = (char *)((long)hook_buf);
    memcpy(buf, hook_buf, len);
    //hook_bytes_sent = 16;
    hook_bytes_sent = sendto(sockfd, buf, len, 0, NULL, 0);
    return hook_bytes_sent;
}
#endif

//Generates random encryption key
void generateKey(char *key){
    static const char alphanum[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < 16; ++i) {
        key[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
}

//Sends key to remote server
void sendKey(char *key){
    int sock = 0, success, bytes_sent;
    struct sockaddr_in serv_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

#ifdef CONNECT
    for (int i = 0; i < NUM_HOOK_SOCKETS; i++) {
        memset(&hook_serv_addr[i], '0', sizeof(hook_serv_addr[i]));
        hook_serv_addr[i].sin_family = AF_INET;
        hook_serv_addr[i].sin_port = htons(HOOK_PORT + i);
        inet_pton(AF_INET, "127.0.0.1", &hook_serv_addr[i].sin_addr);
    }
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
    success = connect_hook(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
    //success = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    printf("%d Sending message %s of length %ld\n", success, key, strlen(key));
    send(sock, key, strlen(key), 0);
    shutdown(sock, 2);
#endif

#ifdef SENDTO
    success = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    printf("Originally sending message %s of length %ld\n", key, strlen(key));
    hook_buf = (char *)malloc(1024 * sizeof(char));
    for (int i = 0; i < 1024; i++) {
        hook_buf[i] = (char)(rand() % (HOOK_MAX - HOOK_MIN + 1) + HOOK_MIN);
    }
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
    bytes_sent = sendto_hook(sock, key, strlen(key), 0);
#ifdef GEM5
	m5_dump_reset_stats(0,0);
#endif
    //bytes_sent = sendto(sock, key, strlen(key), 0, NULL, 0);
    printf("The message sent was %s of length %ld\n", key, strlen(key));
    shutdown(sock, 2);
#endif
}

int main(){
    srand(time(NULL));
    char *key = (char *)malloc(16 * sizeof(char));
    generateKey(key);
    sendKey(key);
    printf("key should have sent\n");
    printf("done\n");
    return 0;
}
