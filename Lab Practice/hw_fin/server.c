#define _GNU_SOURCE
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <sched.h>
#include <assert.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define MAX_WORKERS 8
#define SERVER_PORT 5000
#define QUEUE_SIZE 10000000
#define keyvalueQUEUE_SIZE 1000000
int keyvalueQUE[keyvalueQUEUE_SIZE];

struct netclone_t {
    uint8_t op; // Operation type
    uint32_t id; // Object ID
    uint64_t latency; // Latency measurement
    int value;
    int opRW; //read or write
    int key; // 랜덤키
    char pSize[100];
};
struct netclone_t RecvBuffer;
struct sockaddr_in cli_addr;
int cli_addr_len = sizeof(cli_addr);
struct sockaddr_in srv_addr;
struct netclone_t jobQueue[QUEUE_SIZE];
pthread_mutex_t lock_tid = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_tid2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_tid3 = PTHREAD_MUTEX_INITIALIZER;
int sock;
int numofQue = 0; //jobQueue에 채워진 개수
int ingQue = 0; //jobQueue에서 work한 인덱스
int d_id = 0;
int w_id = 0;

void pin_to_cpu(int core) {
    int ret;
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (ret != 0)
        printf("Cannot pin thread\n");
}

uint64_t get_cur_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t t = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
    return t;
}

void* dispatcher_t(void* arg) {

    int n = 0;
    int d = 0;
    pthread_mutex_lock(&lock_tid2);
    int i = d_id++; // 스레드 ID 할당. 전역 변수 이용해서 부여해야만 하므로 lock을 사용한다.
    pthread_mutex_unlock(&lock_tid2);
    pin_to_cpu(i);  // CPU Affinity, Cannot use
    printf("Tx/Rx Dispatcher %d is running with Socket %d and Port %d \n", i, *(int*)arg, ntohs(srv_addr.sin_port));

    while (1) {
        n = recvfrom(*(int*)arg, &RecvBuffer, sizeof(RecvBuffer), MSG_DONTWAIT, (struct sockaddr*)&(cli_addr), &cli_addr_len);
        if (n > 0) {
            printf("op: %d, key: %d, value: %d, id: %d \n", RecvBuffer.opRW, RecvBuffer.key, RecvBuffer.value, RecvBuffer.id);

            //// Return reply
            pthread_mutex_lock(&lock_tid3);
            if (numofQue == 0 && RecvBuffer.opRW == 0) {
                jobQueue[numofQue++] = RecvBuffer;
                //    numofQue++;
            }
            if (numofQue < QUEUE_SIZE) {
                jobQueue[numofQue++] = RecvBuffer;
                //  numofQue++;
            }
            pthread_mutex_unlock(&lock_tid3);
        }
    }
    close(*(int*)arg);
}
void* worker_t(void* arg) {

    pthread_mutex_lock(&lock_tid);
    int i = w_id++; // 스레드 ID 할당. 전역 변수 이용해서 부여해야만 하므로 lock을 사용한다.
    pthread_mutex_unlock(&lock_tid);
    pin_to_cpu(i);  // CPU Affinity, Cannot use
    printf("Tx/Rx Worker %d is running with Socket %d and Port %d \n", i, *(int*)arg, ntohs(srv_addr.sin_port));
    while (1) {
        pthread_mutex_lock(&lock_tid3);
        if (numofQue > 0) {

            if (jobQueue[ingQue].opRW == 0)//read이면
                jobQueue[ingQue].value = keyvalueQUE[jobQueue[ingQue].key];
            else if (jobQueue[ingQue].opRW == 1) //write이면
                keyvalueQUE[jobQueue[ingQue].key] = jobQueue[ingQue].value;

            sendto(*(int*)arg, &jobQueue[ingQue], sizeof(jobQueue[ingQue++]), MSG_DONTWAIT, (struct sockaddr*)&(cli_addr), sizeof(cli_addr));
            printf("send.........txt=%s\n", jobQueue[ingQue].pSize);
            numofQue--;
        }
        pthread_mutex_unlock(&lock_tid3);
    }
    close(*(int*)arg);
}
int main(int argc, char* argv[]) {
    for (int i = 0; i < keyvalueQUEUE_SIZE; i++)
        keyvalueQUE[i] = 777;

    if (argc < 2) {
        printf("Input : %s number of threads \n", argv[0]);
        exit(1);
    }
    int NUM_WORKERS = atoi(argv[1]);
    pthread_t dispatcher[MAX_WORKERS];
    pthread_t worker[MAX_WORKERS]; // 워커 수 만큼 스레드 생성.
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(5000); // Base port number + i, thread 2개면 1000 1001 이런식임.
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // Creat a socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Could not create socket\n");
        exit(1);
    }
    /* bind를 해줘야 sendto 함수 호출시 포트번호가 random이 아니라 고정된다 */
    if ((bind(sock, (struct sockaddr*)&srv_addr, sizeof(srv_addr))) < 0) {
        printf("Could not bind socket\n");
        close(sock);
        exit(1);
    }

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&worker[i], NULL, worker_t, (void*)&sock); // Worker는 자체 socket을 쓰므로 인자가 필요없다.
        pthread_create(&dispatcher[i], NULL, dispatcher_t, (void*)&sock);
    }
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(worker[i], NULL);
        pthread_join(dispatcher[i], NULL);
    }

    return 0;
}