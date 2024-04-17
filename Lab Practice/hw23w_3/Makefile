server.out : server.o 
	gcc -o server.out server.o 

client.out : client.o 
	gcc -o client.out client.o 

server.o : server.c
	gcc -c -o server.o server.c
	
client.o : client.c
	gcc -c -o client.o client.c


clean:
	rm *.o server.out
	rm *.o client.out