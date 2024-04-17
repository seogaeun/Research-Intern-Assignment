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

#define MAX_WORKERS 16 // 최대 워커 수. CPU 코어수에 따라 필요시 수정하면 된다.
#define SERVER_PORT 1000 // 서버쪽 Base port번호. 서버도 멀티스레드이므로, Port번호가 여러개임.
// Tx스레드의 while(1) 루프 내 srv_addr.sin_port 수정하는 부분 코드 참고.

int local_pkt_counter[MAX_WORKERS] = {0,}; // lock을 피하기 위해서 스레드별로 로 컬카운터를 사용함
int global_pkt_counter = 0; // 프로그램 종료시 통계치 산출을 위해 사용하는 수합용 글로벌 카운터

/* tx와 rx ID를 생성한 순서대로 순차적으로 부여하기 위한 lock과 전역 변수. */
pthread_mutex_t lock_txid = PTHREAD_MUTEX_INITIALIZER;
int tx_id = 0;
pthread_mutex_t lock_rxid = PTHREAD_MUTEX_INITIALIZER;
int rx_id = 0;
pthread_mutex_t lock_create = PTHREAD_MUTEX_INITIALIZER;

/*IP주소를 바탕으로 서버 ID를 반환하는 함수. 자세한건 main함수 주석 참고*/
int get_server_id(char *interface)
{
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;
    /* I want IP address attached to interface */
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);
		/* display only the last number of the IP address */
		char* ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    return (ip[strlen(ip) - 1] - '0');
}

/* 스레드를 특정 CPU 코어에 고정시켜서 CPU affinity를 맞춰준다. pint_to_cpu(0)이면 0번 코어에 고정한다는 뜻*/
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

/* 현재 시간을 nanosecond 수준으로 측정한다 */
uint64_t get_cur_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  uint64_t t = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  return t;
}

/* 응용 메시지 구조체. 사용하는 응용에 따라 이런 구조체를 새로 만들거나 수정하면 됨 */
struct netclone_t{
  uint8_t op; // Operation type
  uint32_t id; // Object ID
  uint64_t latency; // Latency measurement
  char* data; // Object data
} ;

/* 스레드에 인자를 전달하기 위한 구조체 */
struct sock_t {
  int sock; // thread에 main 함수 argument를 넘기기 위함.
  struct sockaddr_in srv_addr; // thread에 main 함수 argument를 넘기기 위함.
  uint64_t TARGET_QPS; // thread에 main 함수 argument를 넘기기 위함.
  uint64_t NUM_REQUESTS; // thread에 main 함수 argument를 넘기기 위함.
  uint64_t NUM_WORKERS; // thread에 main 함수 argument를 넘기기 위함.
  uint64_t NUM_WORKERS_SRV; // thread에 main 함수 argument를 넘기기 위함.
	int PROTOCOL_ID; // thread에 main 함수 argument를 넘기기 위함.
	int SERVER_ID; // thread에 main 함수 argument를 넘기기 위함.
} ;

void *tx_t(void *arg){
  pthread_mutex_lock(&lock_txid);
	int i = tx_id++; // 스레드 ID 할당. 전역 변수 이용해서 부여해야만 하므로 lock을 사용한다.
	pthread_mutex_unlock(&lock_txid);
	pin_to_cpu(i);  // CPU Affinity
  struct sock_t *sockfd = (struct sock_t *)arg; // 인자로 넘겨받은 소켓을 추출함
  struct netclone_t SendBuffer; // 보내고자 하는 메시지가 해당 구조체이고 소켓을 사용하므로 Sendbuffer라 이름 지음
  struct sockaddr_in srv_addr = sockfd->srv_addr;

	/*보내고자 하는 데이터 초기화. 실제 응용 작성시에는 while 루프 안으로 들어가야됨 */
  SendBuffer.op = 0;
  SendBuffer.id = 0;
  //SendBuffer.data = malloc(1);
  SendBuffer.data = "";
  srand(time(NULL)); // rand함수용 시드 초기화
	int cli_addr_len = 0;
  int n = 0;
  uint64_t elapsed_time = get_cur_ns();
	uint64_t temp_time = get_cur_ns();
  uint64_t inter_arrival_time = 0;
	int sock = sockfd->sock;
	printf("Tx Worker %d is running with Socket %d \n",i,sock);
	uint64_t NUM_WORKERS = sockfd->NUM_WORKERS;
	uint64_t NUM_WORKERS_SRV = sockfd->NUM_WORKERS_SRV;
	/* Thread별로 나눠서 처리하기 때문에 나눗셈 등의 연산이 들어간다. QPS같은 경우는 Exponentional distribution을 위해서 식이 복잡함 */
	uint64_t NUM_REQUESTS = sockfd->NUM_REQUESTS/NUM_WORKERS;
  uint64_t TARGET_QPS = RAND_MAX*sockfd->TARGET_QPS/1000/2/NUM_WORKERS;
  uint64_t counter = 0; // Tx 통계를 위한 카운터

	/* Tx의 Main 루프에 해당한다 */
	while(1){
		srv_addr.sin_port = htons(SERVER_PORT+ rand()%NUM_WORKERS_SRV); // 서버쪽 여러 스레드에 분산적으로 보내기 위해서 rand돌림
    SendBuffer.id = counter; // 현재는 특정 응용이 없어서 그냥 ID를 counter로 넣음

		/*Packet inter-arrival time 을 Exponentional하게 보내기 위한 연산 과정들이다 */
    inter_arrival_time = (uint64_t)(-log(1.0 - ((double)rand() / TARGET_QPS)) * 1000000) ;
    temp_time+=inter_arrival_time;
    while (get_cur_ns() < temp_time)
      ; // Inter-inter_arrival_time만큼 시간이 지나지 않았다면 무한루프를 돌며 대기한다.
    SendBuffer.latency = get_cur_ns(); // latency 측정용 timestamp
		sendto(sock, &SendBuffer, sizeof(SendBuffer),  0, (struct sockaddr*)&(srv_addr), sizeof(srv_addr));
    counter++;
    if(counter >= NUM_REQUESTS) break; // 보내야하는 pkt만큼 보냈으면 while루프 탈출함
	}
	/* Tx 통계치 를출력하며 종료함 */
	double tot_time = (get_cur_ns() - elapsed_time )/1e9;
	double throughput = counter  / tot_time ;
	printf("Tx Worker %d is finished by sending %lu requests, Tx throughput: %d RPS \n",i, counter,(int)throughput);

}


void *rx_t(void *arg){
	/* Rx ID를 할당 */
  pthread_mutex_lock(&lock_rxid);
	int i = rx_id++;
	pthread_mutex_unlock(&lock_rxid);

	/* 전반적인 구조는 tx와 거의 유사하므로 자세한 주석은 생략함 */
  struct sock_t *sockfd = (struct sock_t *)arg;
	uint64_t NUM_WORKERS = sockfd->NUM_WORKERS;
	pin_to_cpu(i+NUM_WORKERS);  // CPU Affinity
  struct netclone_t RecvBuffer;
  struct sockaddr_in cli_addr;
  int cli_addr_len = sizeof(cli_addr);
  int counter=0;
	int sock = sockfd->sock;
	printf("Rx Worker %d is running with Socket %d\n",i,sock);
	uint64_t NUM_REQUESTS = sockfd->NUM_REQUESTS;
	uint64_t TARGET_QPS = sockfd->TARGET_QPS;
	uint64_t PROTOCOL_ID = sockfd->PROTOCOL_ID;
	uint64_t SERVER_ID = sockfd->SERVER_ID;

	/* Rx는 패킷 수신 후 latency를 기록하는게 주 목적임. 아래가 그 내용 */
	char log_file_name[40];
	sprintf(log_file_name,"./log-%lu-%d-%lu-%lu-%lu.txt",SERVER_ID,i,PROTOCOL_ID,NUM_REQUESTS,TARGET_QPS);  // log-ServerID-ThreadID-Protocol-REQUESTS-QPS
	FILE* fd;
	if ((fd = fopen(log_file_name, "w")) == NULL) {
		exit(1);
	}

  uint64_t elapsed_time = get_cur_ns();

	/*Rx 스레드는 1초 이상 패킷을 수신하지 않으면 실험이 끝난 걸로 봄. 이를 위한 타이머 세팅 */
	uint64_t timer = get_cur_ns();


	int n = 0;
  while(1){
		if((get_cur_ns() - timer  ) > 1e9 ) break; // 현재 시간 - 타이머 > 1초 초과라면 while루프 탈출

		/*소켓 버퍼에서 패킷을 읽고, 수신한 내용이 있으면 처리를 함 */
		n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), MSG_DONTWAIT, (struct sockaddr*)&(cli_addr), &cli_addr_len);
		if (n>0){
			printf("%lu \n",(get_cur_ns() - RecvBuffer.latency)/1000);
			fprintf(fd,"%lu\n",(get_cur_ns() - RecvBuffer.latency)); // latency를 기록한다
			local_pkt_counter[i]++; // 패킷 카운터 증가시킴
			timer = get_cur_ns(); // 패킷을 수신했으므로 다시 타이머를 초기화 시킴
    }

  }
	/* 통계치 출력해주고 스레드 종료 */
	double tot_time = ((get_cur_ns() - elapsed_time )/1e9)-1;
	fprintf(fd,"%f\n",tot_time); // log파일 마지막에 실험 시간이 들어간다. log파일 처리시 throughput 계산에 활용되며, 당연히 latency 구할 때는 제외하는 처리 해줘야됨
	printf("Rx Worker %d is finished \n",i);
	close(sockfd->sock); // 소켓 닫음
	fclose(fd); // 파일 닫음
}

int main(int argc, char *argv[]) {
	//pin_to_cpu(0); // main 스레드는 딱히 하는게 없어서 따로 고정 안함
	if ( argc < 7 ){
	 printf("%s Usage: ./client DstIP protocolID  workers_srv num_workers num_requests target_ops \n", argv[0]);
	 exit(1);
	}
	int PROTOCOL_ID = atoi(argv[2]); // 여기서는 의미 없음
	int NUM_WORKERS_SRV = atoi(argv[3]); // 서버 스레드 수
  int NUM_WORKERS = atoi(argv[4]); // 클라이언트 스레드 수
	int NUM_REQUESTS = atoi(argv[5]); // 생성할 리퀘스트의 수
  uint64_t TARGET_QPS = atoi(argv[6]); // QPS는 서버 단위의 QPS이고, thread 갯수만큼 나눠서 보냄

	/* 서버의 ID를 자동으로 구함. 실험에 사용하는 인터페이스의 IP주소를 읽은 후 가장 마지막 숫자를 ID로 사용
	달리 말하면, IP주소는 반드시 끝이 1 2 3 4 5 6 7 8 9 .. 이런식이어야함. 서버가 10개 넘어가면 함수 변경해야함.
	왜냐하면, 현재 함수로는 마지막 IP가 110이면, 10이 아니라 0을 출력할 것이기 때문.
	*/
	char *interface = "lo"; // WSL에서는 의미 없음
	int SERVER_ID = get_server_id(interface) - 1 ;
  if(SERVER_ID <0) SERVER_ID = 0;
	printf("Server %d is running \n",SERVER_ID); // WSL에서는 의미 없음

	/* 소켓에 전체 공통적으로 쓰이는 부분들 세팅 */
  struct sockaddr_in srv_addr;
  memset(&srv_addr, 0, sizeof(srv_addr)); // Initialize memory space with zeros
  char* server_name = argv[1];; // 127.0.0.1
  srv_addr.sin_family = AF_INET; // IPv4
	srv_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, server_name, &srv_addr.sin_addr);  // Convert IP addr. to binary and save Addr.

  //소켓 구조체 생성. 주의: 소켓 구조체는 각 스레드에 소켓 뿐만 아니라 여러 인자를 넘기는데 사용됨.
  struct sock_t sockfd[MAX_WORKERS]; // 소켓 구조체를 최대 워커 수만큼 생성한다.
	int sock[MAX_WORKERS]; // 소켓 디스크립터를 최대 워커 수만큼 생성함.
  for(int i=0;i<NUM_WORKERS;i++){
		if ((sock[i] = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // UDP 소켓을 실제 워커수만큼 생성함.
			printf("Could not create socket\n");
			exit(1);
		}
		/* Main 함수에서 인자로 받아온 것들 + 소켓 관련 내용을 스레드에 전달할 인자용 구조체에 넣어줌.*/
	  sockfd[i].sock = sock[i];
	  sockfd[i].srv_addr = srv_addr;
	  sockfd[i].TARGET_QPS = TARGET_QPS;
	  sockfd[i].NUM_REQUESTS = NUM_REQUESTS;
	  sockfd[i].NUM_WORKERS = NUM_WORKERS;
	  sockfd[i].NUM_WORKERS_SRV = NUM_WORKERS_SRV;
	  sockfd[i].PROTOCOL_ID = PROTOCOL_ID;
	  sockfd[i].SERVER_ID = SERVER_ID;
	}
	/* Tx와 Rx를 분리하고 별도의 Core를 사용시킴으로써 성능을 극대화함 */
  pthread_t tx[MAX_WORKERS],rx[MAX_WORKERS]; // tx와 rx 스레드 디스크립터(?)를 생성함

	uint64_t elapsed_time = get_cur_ns(); // 최종 Rx Throughput (=Goodput) 측정용 Start 시간
  for(int i=0;i<NUM_WORKERS;i++){
		pthread_mutex_lock(&lock_create); // Tx와 Rx가 사용하는 Socket을 동기화하기 위해서 lock을 상요함
    pthread_create(&rx[i],NULL, rx_t ,(void *)&sockfd[i]); // Rx 스레드 생성
		pthread_create(&tx[i],NULL, tx_t ,(void *)&sockfd[i]); // Tx 스레드 생성
		pthread_mutex_unlock(&lock_create); // lock을 헤재함
  	}

	for(int i=0;i<NUM_WORKERS;i++){
		pthread_join(rx[i], NULL); // Rx 스레드가 종료되기 까지 main함수를 진행시키지 않음
		pthread_join(tx[i], NULL); // Tx 스레드가 종료되기 까지 main함수를 진행시키지 않음
	}

	/* Rx와 Tx 스레드가 종료된 후, 최종 통계치를 출력함 */
	for(int i=0;i<NUM_WORKERS;i++) global_pkt_counter += local_pkt_counter[i]; //스레드별 패킷 수 수합
	double tot_time = ((get_cur_ns() - elapsed_time )/1e9)-1; // 현재 시간 - Start 시간으로 총 실험 시간 구함
	double throughput = global_pkt_counter  / tot_time ; // 나눠서 throughput 구함
	printf("Total time: %f seconds \n", tot_time);
	printf("Total received pkts: %d \n", global_pkt_counter);
	printf("Rx Throughput: %d RPS \n", (int)throughput);

	return 0;
}
