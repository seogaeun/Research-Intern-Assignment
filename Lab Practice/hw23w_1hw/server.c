#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("Input : %s port number\n", argv[0]);
		return 1;
	}

	int SERVER_PORT = atoi(argv[1]);

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(SERVER_PORT);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	int listen_sock;
	if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Could not create listen socket\n");
		return 1;
	}

	if ((bind(listen_sock, (struct sockaddr*)&server_address,
		sizeof(server_address))) < 0) {
		printf("Could not bind socket\n");
		return 1;
	}

	int wait_size = 16;
	if (listen(listen_sock, wait_size) < 0) {
		printf("Could not open socket for listening\n");
		return 1;
	}
	struct sockaddr_in client_address;
	int client_address_len = 0;

	int sock;
	int  maxlen = 1024;
	int n = 0;
	char buffer[maxlen];
	float result = 0;

	while (1) {
		if ((sock = accept(listen_sock, (struct sockaddr*)&client_address, &client_address_len)) < 0) {
			printf("Could not open a socket to accept data\n");
			return 1;
		}

		printf("Client connected with ip address: %s\n", inet_ntoa(client_address.sin_addr));

		float cal(char* buffer) {
			int len = 0;

			char w[100] = {};
			char a[100] = {};
			char b[100] = {};
			int i,j = 0;
			while (buffer[len] != '\0') {
				if (buffer[len] != 32) {
					switch (i) {
					case 0:
						w[j++] = buffer[len]; break;
					case 1:
						a[j++] = buffer[len]; break;
					case 2:
						b[j++] = buffer[len]; break;
					}

				}
				else {
					i++;
					j = 0;
				}
				len++;
			}
//			i = 0;
			if (strcmp(w,"add")==0) { result = (atoi(a) + atoi(b)); }
			else if (strcmp(w, "sub") == 0) { result = (atoi(a) - atoi(b)); }
			else if (strcmp(w, "mul")==0) { result = (atoi(a) * atoi(b)); }
			else if (strcmp(w, "div")==0) { result = ((float)atoi(a) / (float)atoi(b)); }
			//else printf("옳지 않은 연산이 입력되었습니다.\n");
			return result;
		}


		while ((n = recv(sock, buffer, maxlen, 0)) > 0) {
			buffer[n] = '\0';
			cal(buffer);
			sprintf(buffer, "%d", result);
			printf("result is: %f\n", buffer);
			send(sock, (char*)buffer, strlen((char*)buffer), 0);
		}

	}
	close(sock);
	close(listen_sock);
	return 0;
}
