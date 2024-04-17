#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int readline ( int fd, char *ptr, int maxlen )
{
   int n, rc;
   char c;
   for(n=1; n<maxlen; n++){
      if ( (rc = read(fd, &c, 1)) == 1 ){
         *ptr ++ = c;
         if ( c == '\n' ) break;
      } else if ( rc == 0 ){
         if ( n == 1 ) return 0;
         else break;
      }
   }

   *ptr = 0;
   return n;
}

int main(int argc, char *argv[]) {
	if ( argc < 2 ){
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

	// open a stream socket
	int sock;
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Could not create socket\n");
		return 1;
	}

	if (connect(sock, (struct sockaddr*)&server_address,
	            sizeof(server_address)) < 0) {
		printf("Could not connect to server\n");
		return 1;
	}

	int n = 0;
	int maxlen = 1024;
	char buffer[maxlen];
	char cdata_to_send[maxlen+1];
  	char data_to_send[maxlen+1];
	void Add() { send(sock, data_to_send, strlen(data_to_send), 0); }
	void Sub() { send(sock, data_to_send, strlen(data_to_send), 0); }
	void Mul() { send(sock, data_to_send, strlen(data_to_send), 0); }
	void Div() { send(sock, data_to_send, strlen(data_to_send), 0); }

	while(1){
		if ( readline(0, data_to_send, maxlen) > 0 ){ //만약 클라이언트에 메세지가 있다면
			strncpy(cdata_to_send, data_to_send, 3);
			cdata_to_send[3] = '\0'; // Null

			if (strcmp(cdata_to_send, "add") == 0) { Add(); }
			else if (strcmp(cdata_to_send, "sub") == 0) { Sub(); }
			else if (strcmp(cdata_to_send, "mul") == 0) { Mul(); }
			else if (strcmp(cdata_to_send, "div") == 0) { Div(); }
			
			 //send 함수를 통해서 메세지를 보내라
		}

		if ((n = recv(sock, buffer, maxlen, 0)) > 0) { //소켓서 읽을 데이터가 있으면
			printf("RECIEVE!");
//			buffer[n] = '\0'; // Null
//			printf("%s", buffer); //출력해라
		}
	}
	close(sock);
	return 0;
}
