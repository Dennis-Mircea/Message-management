#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"


int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	
	// verifica daca numarul de argumente este corect
	if (argc < 4) {
		std::cerr << "Usage: " << argv[0] << " ID_CLIENT SERVER_ADDRESS SERVER_PORT" << std::endl;
		return -1;
	}

	// deschidere socket TCP
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "Error Open TCP Socket");

	// completare informatii despre adresa serverului
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "Error inet_aton");

	// dezactivarea algoritmului Neagle
	int one = 1;
	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(int));
	DIE(ret < 0, "TCP Socket Options Error");

	// stabilirea conexiunii la server
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "Error Connect");
	
	// punem in multimea de descriptori de citire socketul curent si Standard Inputul
	FD_SET(sockfd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    // numarul maxim de socketi
	fdmax = sockfd;

	// bagam in buffer id-ul clientului pentru a-l trimite la server
	memset(buffer, 0, BUFLEN);
	memcpy(buffer, argv[1], strlen(argv[1]));
	ret = send(sockfd, buffer, strlen(buffer), 0);
	DIE(ret < 0, "Error Send");

	while (1) {
  		// setam multimea de descriptori temporari
  		tmp_fds = read_fds;
  		// se selecteaza multimea de descriptori de citire
  		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
  		DIE(ret < 0, "Error Select");

  		// resetam bufferul la 0
  		memset(buffer, 0, BUFLEN);

  		for (int i = 0; i <= fdmax; i++) {
  			// daca socket-ul curent se afla in multimea temporara de descriptori
  			if (FD_ISSET(i, &tmp_fds)) {
  				// verifica sa nu fie mesaj de la tastatura
  				if (i != STDIN_FILENO) {
  					int received = recv(i, buffer, sizeof(buffer), 0);
  					DIE(recv < 0, "Error recv");
  					if (received == 0) {
                        std::cout<< "Closed connection" << std::endl;
  						close(i);
  						FD_CLR(i, &read_fds);
  						return 0;
  					} else {
  						std::cout << "Recieve message: " << std::string(buffer) << std::endl;
  					}
  				}  else {
					fgets(buffer, BUFLEN - 1, stdin);
					if (strncmp(buffer, "exit", 4) == 0) {
						close(sockfd);
						return 0;
					}
					// retine fiecare argument in parte
					char* token = (char*) malloc(BUFLEN * sizeof(char));
					// buffer auxiliar pentru parsarea comenzii de la tastatura
					char* aux = (char*) malloc(BUFLEN * sizeof(char));
					strcpy(aux, buffer);

					token = strtok(aux, " ");
					int nr = 1; // retine numarul de argumente date de la tastatura
					while ((token = strtok(NULL, " ")) != NULL) {
						nr++;   	
					}
					// verifica daca comanda este una valida inainte de a o trimite
					if ((strstr(buffer, "unsubscribe") != nullptr && nr == 2)
							|| (strstr(buffer, "subscribe") != nullptr && strstr(buffer, "unsubscribe") == nullptr && nr == 3)) {
						n = send(sockfd, buffer, strlen(buffer) - 1, 0);
						DIE(n < 0, "Error Send");
					} else {
						std::cout << "Wrong command" << std::endl;
					}
  				}
  			}
  		}
		
	}
	// inchide socketul
	close(sockfd);

	return 0;
}
