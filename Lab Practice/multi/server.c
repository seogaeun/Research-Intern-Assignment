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

#define MAX_WORKERS 16
#define SERVER_PORT 1000

pthread_mutex_t lock_tid = PTHREAD_MUTEX_INITIALIZER;
int t_id = 0;

void pin_to_cpu(int core){
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

struct netclone_t{
	uint8_t op; // Operation type
  uint32_t id; // Object ID
  uint64_t latency;
  char* data; // Object data
} ;

void *worker_t(void *arg){

	pthread_mutex_lock(&lock_tid);
	int i = t_id++; // 스레드 ID 할당. 전역 변수 이용해서 부여해야만 하므로 lock을 사용한다.
	pthread_mutex_unlock(&lock_tid);
	pin_to_cpu(i);  // CPU Affinity, Cannot use
	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(1000+i); // Base port number + i, thread 2개면 1000 1001 이런식임.
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	// Creat a socket
	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Could not create socket\n");
		exit(1);
	}
	/* bind를 해줘야 sendto 함수 호출시 포트번호가 random이 아니라 고정된다 */
	if ((bind(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) < 0) {
		printf("Could not bind socket\n");
		close(sock);
		exit(1);
	}

	printf("Tx/Rx Worker %d is running with Socket %d and Port %d \n",i,sock,ntohs(srv_addr.sin_port));
	struct netclone_t RecvBuffer;
	struct sockaddr_in cli_addr;
	int cli_addr_len = sizeof(cli_addr);
	int counter = 0; // local counter
	int n = 0;

	int result = 0;
	while(1){
		n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer),  MSG_DONTWAIT, (struct sockaddr *)&(cli_addr), &cli_addr_len);
		if (n > 0) {
			// Actual work happens here
			printf("Packet received\n");

			//// Return reply
			sendto(sock, &RecvBuffer, sizeof(RecvBuffer),  MSG_DONTWAIT, (struct sockaddr *)&(cli_addr), sizeof(cli_addr));
		}
	}
	close(sock);
}

int main(int argc, char *argv[]) {
	if ( argc < 2 ){
	 printf("Input : %s number of threads \n", argv[0]);
	 exit(1);
	}
	int NUM_WORKERS = atoi(argv[1]);
	pthread_t worker[MAX_WORKERS]; // 워커 수 만큼 스레드 생성.
	for(int i=0;i<NUM_WORKERS;i++){
		pthread_create(&worker[i],NULL, worker_t ,0); // Worker는 자체 socket을 쓰므로 인자가 필요없다.
	}
	for(int i=0;i<NUM_WORKERS;i++){
		pthread_join(worker[i], NULL);
	}
	return 0;
}
