#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "stdio.h"
#include "math.h"


int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Input : %s port number\n", argv[0]);
        return 1;
    }

    //포트 번호 변수
    int SERVER_PORT = atoi(argv[1]);
    //포트 번호 뒤에 인자
    int PORT_NUM = atoi(argv[2]);

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    //데이터 배열 선언 및 내부 값 채우기
    int data[10000];
    for (int i = 0; (i < 10000 ); i++) {
        data[i] = i%2; 
    }




    //////////////////////////
    int sock;
    int sock2;

    int  maxlen = 1024;
    int n = 0;
    int connectFD;
    char readBuff[maxlen];
    char sendBuff[maxlen];
    pid_t pid;



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
  }

    int wait_size = 16;
    if (listen(listen_sock, wait_size) < 0) {
        printf("Could not open socket for listening\n");
        return 1;
    }
    struct sockaddr_in client_address;
    int client_address_len = 0;







    while (1) {

        if ((sock = accept(listen_sock, (struct sockaddr*)&client_address, &client_address_len)) < 0) {
            printf("Could not open a socket to accept data\n");
            return 1;
        }

        struct sockaddr_in connectSocket, peerSocket;
        socklen_t connectSocketLength = sizeof(connectSocket);


        while ((connectFD = accept(listen_sock, (struct sockaddr*)&connectSocket, (socklen_t*)&connectSocketLength)) >= 0) {

            printf("Client connected with ip address: %s\n", inet_ntoa(client_address.sin_addr));
            getpeername(connectFD, (struct sockaddr*)&peerSocket, &connectSocketLength);
            char peerName[sizeof(peerSocket.sin_addr) + 1] = { 0 };
            sprintf(peerName, "%s", inet_ntoa(peerSocket.sin_addr));
            // 접속이 안되었을 때는 출력 x
            if (strcmp(peerName, "0.0.0.0") != 0)
                printf("Client : %s\n", peerName);


            if (connectFD < 0)
            {
                printf("Server: accept failed\n");
                exit(0);
            }
            pid = fork();

            int number[3];
            int q = 0;
            char commands[maxlen + 1]; //명령을 받는 배열
            char X[10000]; //x값을 받는 배열
            char N[maxlen + 1]; //N값을 받는 배열

            while ((n = recv(sock, readBuff, maxlen, 0)) > 0) { //데이터가 입력 받음
                printf("\nmessage 받았음!\n");
                //명령어 따로 담기
                int i = 0;

                for (int c_num = 0; readBuff[i] != 32; c_num++, i++) {
                    commands[c_num] = readBuff[i];
                }
                commands[3] = '\0';
                i++; //공백 건너뛰기 위해서


                int x_num = 0;
                for (; (readBuff[i] != 10) && (readBuff[i] != 32) && (readBuff[i] != '\n'); x_num++, i++) {
                    X[x_num] = readBuff[i];
                }

                X[++x_num] = '\0';
                int index = atoi(X); //데이터 배열 인덱스 번호
                i++; //공백 건너뛰기 위해서

                if (strcmp(commands, "get") == 0) { //스위치가 수신한 메세지가 get X
                    if (data[index] == 0) {
                        sprintf(sendBuff, "%s", readBuff);
                        printf("%s \n", readBuff);
                        send(sock2, sendBuff, maxlen, 0);

                    }
                    else {
                        sprintf(sendBuff, "%d", data[index]);
                        printf("the number is %d \n", *readBuff);
                        send(sock, sendBuff, maxlen, 0);

                    }
                }
                else if (strcmp(commands, "put") == 0) { //스위치가 수신한 메세지가 put X N;
                    sprintf(sendBuff, "%s", readBuff);
                    printf("%s \n", readBuff);
                    send(sock2, sendBuff, maxlen, 0);


                }
                else if (strcmp(commands, "rgt") == 0) { //서버로부터 받은 get에 대한 reply
                    sprintf(sendBuff, "%s", readBuff);
                    printf("%s \n", readBuff);
                    send(sock, sendBuff, maxlen, 0);

                }
                else if (strcmp(commands, "rpt") == 0) { //서버로부터 받은 put에 대한 reply
                    sprintf(sendBuff, "%s", readBuff);
                    printf("%s \n", readBuff);
                    send(sock, sendBuff, maxlen, 0);

                }

                close(sock2);

            }
        }
    close(sock);
    close(listen_sock);
    return 0;
}