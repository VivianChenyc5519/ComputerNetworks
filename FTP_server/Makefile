all: ftp_client ftp_server

ftp_client: code/ftp_client.o 
	gcc code/ftp_client.o -o ftp_client

ftp_server: code/ftp_server.o
	gcc code/ftp_server.o -o ftp_server

ftp_client.o: code/ftp_client.c
	gcc -c code/ftp_client.c -o code/ftp_client.o

ftp_server.o: code/ftp_server.c
	gcc -c code/ftp_server.c -o code/ftp_server.o

clean:
	rm code/*.o ftp_client ftp_server 
