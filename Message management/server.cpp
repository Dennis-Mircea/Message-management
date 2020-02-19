#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <iomanip>
#include <math.h>
#include <string>
#include <vector>
#include <iterator>
#include <algorithm>
#include "helpers.h"

class Topic {
public:
	std::string topic;
	int SF;
};

class Client {
public:
	int sockfd;	// socket-ul de pe care se conecteaza clientul
	bool connected;	// specifica daca clientul este conectat sau nu
	std::string name;	// id-ul clientului
	std::vector<Topic> topics; // topicurile la care se aboneaza clientul
	std::vector<std::string> inbox;	// mesajele pe care le primeste clientul cat timp este deconectat

	Client (std::string name, int sockfd) { // constructor pentru un client care se conecteaza
		this->name = name;
		this->sockfd = sockfd;
		this->connected = true;
	}
};

// functie pentru a nu concatena mesajele primite(pastreaza o distanta de o secunda)
void delay() {
	int count = 7000000;
	while (count) {
		count--;
	}
}

// functie care cauta un client in multimea de clienti, iar daca il gaseste, ii actualizeaza datele
bool findClient(int sock, std::vector<Client>& clients, std::string name) {
	for (int i = 0; i < clients.size(); i++) {
		if (clients[i].name == name) { // daca l-a gasit
			clients[i].sockfd = sock;
			clients[i].connected = true;
			// trimite toate mesajele catre client din cutia postala
			for (int j = 0; j < clients[i].inbox.size(); j++) {
				send(clients[i].sockfd, clients[i].inbox[j].c_str(), clients[i].inbox[j].length(), 0);
				delay();
			}
			// goleste cutia postala
			clients[i].inbox.clear();
			return true;
		}
	}
	return false;
}

void udpMessage(std::string& messageToSend, char* buffer, struct sockaddr_in cli_addr) {
	// numele topicului
	std::string topicName(buffer);

	// retine tipul datelor (INT, SHORT_REAL ...)
	uint8_t type;
	memcpy(&type, buffer + 50, sizeof(uint8_t));

	// compunerea mesajului(mai intai din ip-ul si portul serverului)
	messageToSend = std::string(inet_ntoa(cli_addr.sin_addr)) + ":" + 
				std::to_string(ntohs(cli_addr.sin_port));


	switch (type) {
		case 0: { // INT
			uint8_t sign; // semnul numarului
			uint32_t int_payload; // numarul in sine
			memcpy(&sign, buffer + 50 + sizeof(uint8_t), sizeof(uint8_t));
			memcpy(&int_payload, buffer + 50 + 2 * sizeof(uint8_t), sizeof(uint32_t));
			// schimbare in little endian din big endian
			uint32_t value = ntohl(int_payload);
			if (sign == 1) {
				messageToSend += " - " + topicName + " - " + "INT" + " - " + "-" + std::to_string(value); 
			} else {
				messageToSend += " - " + topicName + " - " + "INT" + " - " + std::to_string(value);
			}
			break;
	 	} case 1: { // SHORT_REAL
			uint16_t short_payload; // numarul
			memcpy(&short_payload, buffer + 50 + sizeof(uint8_t), sizeof(uint16_t));
			// schimbare in little endian
			uint16_t value = ntohs(short_payload);
			// construirea numarului te tip short
			double number = value / 100.0;

			messageToSend += " - " + topicName + " - " + "SHORT_REAL" + " - " + std::to_string(number);
			break;
		} case 2: { // FLOAT
			uint8_t sign; // semnul
			uint32_t float_payload; // prima parte a numarului
			uint8_t power; // a doua parte

			memcpy(&sign, buffer + 50 + sizeof(uint8_t), sizeof(uint8_t));
			memcpy(&float_payload, buffer + 50 + 2 * sizeof(uint8_t), sizeof(uint32_t));
			memcpy(&power, buffer + 50 + 2 * sizeof(uint8_t) + sizeof(uint32_t), sizeof(uint8_t));

			// schimbare in little endian
			uint32_t x = ntohl(float_payload);
			// alcatuirea numarului propriu-zis
			double value = (double) x;

			while (power) {
				value /= 10.0;
				power--;
			}
			if (sign == 1) {
				messageToSend += " - " + topicName + " - " + "FLOAT" + " - " + "-" + std::to_string(value);
			} else {
				messageToSend += " - " + topicName + " - " + "FLOAT" + " - " + std::to_string(value);
			}

			break;
		} case 3: { // STRING
			char* contains; // continutul mesajului de tip string
			contains = (char*) malloc(1500 * sizeof(char)); // memorie de 1500 de bytes
			memcpy(contains, buffer + 50 + sizeof(uint8_t), 1500);

			std::string text(contains);
			messageToSend += " - " + topicName + " - " + "STRING" + " - " + text;
			break;
		} default:
			break;
	}
}
// trimite mesajul catre client
void sendMessage(std::string messageToSend, std::vector<Client>& clients, std::string topicName) {
	for (int j = 0; j < clients.size(); ++j) {
		bool isInList = false;
		Topic auxiliar;
		// verifica daca topicul este in lista clientului
		for (auto it : clients[j].topics) {
			if (it.topic == topicName) {
				isInList = true;
				auxiliar = it;
				break;
			}
		}
		// daca este in lista
		if (isInList) {
			if (clients[j].connected) { // verifica daca e conectat si atunci ii trimite mesajul
				send(clients[j].sockfd, messageToSend.c_str(), messageToSend.length(), 0);
				delay();
			} else { // altfel baga in cutia postala
				if (auxiliar.SF == 1) {
					clients[j].inbox.push_back(messageToSend);
				}
			}
		}
	}
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, udpSockfd;
	struct sockaddr_in serv_addr, cli_addr;
	char buffer[BUFLEN];
	int n, i, ret;
	socklen_t clilen;

	clilen = sizeof(cli_addr);

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	// verifica daca numarul de argumente este corect
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " SERVER_PORT" << std::endl;
		return -1;
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// deschidere socket TCP
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "Error Open TCP Socket");

	// deschidere socket UDP
	udpSockfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udpSockfd < 0, "Error Open UDP Socket");

	// preluarea portului
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");
	
	// dezactivarea algoritmului Neagle
	int one = 1;
	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(int));
	DIE(ret < 0, "TCP Socket Options Error");
	
	// completare informatii despre adresa serverului
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// se face legatura intre server si socket atat pentru TCP, cat si pentru UDP
	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "Error Bind");

	ret = bind(udpSockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "Error Bind");

	// ascultam pe socketul de TCP un numar de MAX_CLIENTS clienti
	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "Error Listen");

	// punem in multimea de descriptori de citire socketul curent de TCP si UDP si Standard Inputul
	FD_SET(sockfd, &read_fds);
	FD_SET(udpSockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = std::max(sockfd, udpSockfd);

	// multimea clientilor
	std::vector<Client> clients;

	while (1) {
		// setam multimea de descriptori temporari
  		tmp_fds = read_fds;
  		// se selecteaza multimea de descriptori de citire
  		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
  		DIE(ret < 0, "Error Select");

  		// resetam bufferul la 0
  		memset(buffer, 0, BUFLEN);

		for (i = 0; i <= fdmax; ++i) {
			// daca socket-ul curent se afla in multimea temporara de descriptori
			if (FD_ISSET(i, &tmp_fds)) {
				// verificam daca a venit o cerere pe socketul inactiv
				if (i == STDIN_FILENO) {
					// citeste de la tastatura
					fgets(buffer, BUFLEN, stdin);
					
					// verifica daca a primit exit(singura comanda acceptata de la tastatura in server)
					if (strncmp(buffer, "exit", 4) == 0) {		
						close(sockfd);
						return 0;
					} else {
						std::cout << "Wrong command" << std::endl;
					}
				} else if (i == sockfd) {
					// serverul accepta conexiunea cu socketul primit
					newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "Error Accept");

					// dezactivarea algoritmului Neagle
					ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(int));
					DIE(ret < 0, "TCP Socket Options Error");
					
					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);

					// se seteaza fdmax-ul
					fdmax = std::max(fdmax, newsockfd);

					// se primeste id-ul clientului
					n = recv(newsockfd, buffer, BUFLEN, 0);
					std::string name(buffer);
					
					// daca nu a gasit clientul in multime, il adauga
					if (!findClient(newsockfd, clients, name)) {
						Client client1(name, newsockfd);
						clients.push_back(client1);
					}
					
					std::cout << "New Client " << std::string(buffer) << " connected from "
						<< std::string(inet_ntoa(cli_addr.sin_addr)) << ":" << ntohs(cli_addr.sin_port)
						<< std::endl;
					
				} else if (i == udpSockfd) {
					// primeste topicurile de la clientul udp
					recvfrom(udpSockfd, buffer, BUFLEN, 0, (struct sockaddr *) &cli_addr, &clilen);
					// numele topicului
					std::string topicName(buffer);

					std::string messageToSend;
					// creaza mesajul
					udpMessage(messageToSend, buffer, cli_addr);
					// trimite mesajul
					sendMessage(messageToSend, clients, topicName);

				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "Error Recv");

					// daca nu primeste nimic, inseamna ca conexiunea s-a inchis
					if (n == 0) {
						for (int j = 0; j < clients.size(); ++j) {
							if (clients[j].sockfd == i) { // ajungem la clientul de la care s-au primit date
								
								// il marcam ca deconectat
								clients[j].connected = false;

								// afisam mesaj corespunzator
								std::cout << "Client " << clients[j].name << " disconnected\n";
								break;
							}
						}
						// inchidem socketul
						close(i);
						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
					} else {

						// retine fiecare argument in parte
						char* token = (char*) malloc(BUFLEN * sizeof(char));
						// buffer auxiliar pentru parsarea comenzii de la tastatura
						char* aux = (char*) malloc(BUFLEN * sizeof(char));
						strcpy(aux, buffer);

						token = strtok(aux, " ");

						if (strstr(token, "unsubscribe") != nullptr ) { // daca a primit comanda unsubscribe
							token = strtok(NULL, " "); // ia topicul de la care se dezaboneaza clientul

							std::string topicUnsubscribed(token);

							for (int j = 0; j < clients.size(); ++j) {
								if (clients[j].sockfd == i) { // cautam clientul respectiv
									for (auto it = clients[j].topics.begin(); it != clients[j].topics.end(); ++it) { // cautam topicul
										if (topicUnsubscribed == it->topic) {
											clients[j].topics.erase(it);
											break;
										}
									}
								}

							}
						} else { // daca a primit comanda subscribe
							token = strtok(NULL, " ");
							std::string topicSubscribed(token); // topicul la care se aboneaza

							Topic newtopic;
							newtopic.topic = topicSubscribed;

							token = strtok(NULL, " "); // sf-ul
							int sf = atoi(token);
							newtopic.SF = sf;
							// bagam topicul in lista de topicuri la care este abonat clientul respectiv
							for (int j = 0; j < clients.size(); ++j) {
								if (clients[j].sockfd == i) {
									clients[j].topics.push_back(newtopic);
								}
							}
						}
					}
				}
			}
		}
	}
	close(sockfd);
	close(udpSockfd);
	return 0;
}
