#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

int readline(int fd, char* ptr, int maxlen)
{
	int n, rc;
	char c;
	for (n = 1; n < maxlen; n++) {
		if ((rc = read(fd, &c, 1)) == 1) {
			*ptr++ = c;
			if (c == '\n') break;
		}
		else if (rc == 0) {
			if (n == 1) return 0;
			else break;
		}
	}

	*ptr = 0;
	return n;
}


int reads(char* ss, int sock) {
	send(sock, ss, strlen(ss), 0);
}
int writes(char* ss, int sock) {
	send(sock, ss, strlen(ss), 0);
}



int sock; 
int calConnect(int argc, char* argv[]) {
	if (argc < 2) {
		printf("Input : %s port number\n", argv[0]);
		return 1;
	}

	int SERVER_PORT = atoi(argv[1]);
	const char* server_name = "localhost"; // 127.0.0.1
	struct sockaddr_in server_address; // Create socket structure
	memset(&server_address, 0, sizeof(server_address)); // Initialize memory space with zeros
	server_address.sin_family = AF_INET; // IPv4
	inet_pton(AF_INET, server_name, &server_address.sin_addr);  // Convert IP addr. to binary
	server_address.sin_port = htons(SERVER_PORT);

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Could not create socket\n");
		return 1;
	}

	if (connect(sock, (struct sockaddr*)&server_address,
		sizeof(server_address)) < 0) {
		printf("Could not connect to server\n");
		return 1;
	}

}
int main(int argc, char* argv[]) {
	calConnect(argc, argv);
	
	int n = 0;
	int maxlen = 1024;
	char buffer[maxlen];
	char data_to_send[maxlen + 1];
	char commands[maxlen + 1];
	time_t seconds;

	int readBytes, writtenBytes;
	char sendBuffer[maxlen];
	char receiveBuffer[maxlen];


	while (1) {
		struct timespec specific_time;
		struct tm* now;
		int millsec_before;
		int millsec_after;
		if (readline(0, data_to_send, maxlen) > 0) {
			clock_gettime(CLOCK_REALTIME, &specific_time);
			now = localtime(&specific_time.tv_sec);
			millsec_before = specific_time.tv_nsec;
			//명령어 따로 담기
			for (int i = 0; i < 3; i++) {
				commands[i] = data_to_send[i];
			}
			commands[3] = '\0';

			if (strcmp(commands, "get") == 0) {
				printf("get! \n");
				reads(data_to_send, sock);

			}
			else if (strcmp(commands, "put") == 0) {
				printf("put! \n");
				writes(data_to_send, sock);

			}
			else{
				printf("\n---wrong---\n");
			}
		}

		if ((n = recv(sock, buffer, maxlen, 0)) > 0) {
			clock_gettime(CLOCK_REALTIME, &specific_time);
			now = localtime(&specific_time.tv_sec);
			millsec_after = specific_time.tv_nsec;
			buffer[n] = '\0'; // Null
			printf("%s %d\n\n", buffer, millsec_after - millsec_before);
		}
	}
	close(sock);
	return 0;
}
