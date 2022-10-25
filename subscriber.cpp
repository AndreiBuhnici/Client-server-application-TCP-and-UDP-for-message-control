#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <bits/stdc++.h>
#include "utils.h"

using namespace std;

void usage(char *file){
	fprintf(stderr, "Usage: %s client_id server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[]){
	int sockfd, n, ret;
  	int disable_nagle = 1;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	int fdmax;
	fd_set read_fds, tmp_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	if (argc < 4) {
		usage(argv[0]);
	}

	// Open TCP socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket creation failed");

	FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = sockfd;

	// Set ip address and port
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	// Connect to server
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// Disable Nagle
  	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&disable_nagle, sizeof(int));
  	DIE(ret < 0, "nagle");

	// Send id to server
	struct tcp_message tcp_msg;
	memset(&tcp_msg, 0, sizeof(struct tcp_message));
	memcpy(tcp_msg.data, argv[1], sizeof(tcp_msg.data));
  	ret = send(sockfd, &tcp_msg, sizeof(struct tcp_message), 0);
  	DIE(ret < 0, "send");

  	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	while (1) {
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if(FD_ISSET(STDIN_FILENO, &tmp_fds)) {	
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN-1, stdin);

			// Close client
			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			} else if (strncmp(buffer, "subscribe", 9) == 0) {
				// Subscribe to topic from stdin
				if(*(buffer + 10) != '\0') {
					struct subscribe_message sub_msg;
					memset(&sub_msg, 0, sizeof(struct subscribe_message));

					// parse input
					string s = (buffer + 10);
					istringstream iss(s);
					vector<string> tokens{istream_iterator<string>{iss}, istream_iterator<string>{}};
					if(tokens.size() != 2)
						cout<< "Usage : subscribe <TOPIC> <SF>\n";
					else {
						memcpy(&sub_msg.topic, tokens[0].c_str(), strlen(tokens[0].c_str()));
						sub_msg.sf = tokens[1].at(0) - '0';
						sub_msg.type = 's';
						if(sub_msg.sf > 1) {
							printf("Invalid sf, choose 0 or 1\n");
							continue;
						}
						struct tcp_packet sub_pkt;
						memset(&sub_pkt, 0, sizeof(struct tcp_packet));
						memset(&sub_pkt.tcp_msg, 0, sizeof(struct tcp_message));
						memcpy(&sub_pkt.sub_msg, &sub_msg, sizeof(struct subscribe_message));

						// send sub message
						ret = send(sockfd, &sub_pkt, sizeof(struct tcp_packet), 0);
						DIE(ret < 0, "Send subscribe message failed");

						// receive ack
						struct tcp_message sub_ack;
						memset(&sub_ack, 0, sizeof(struct tcp_message));
						ret = recv(sockfd, &sub_ack, sizeof(struct tcp_message), 0);
						DIE(ret < 0, "Ack receive failed");

						if(strncmp(sub_ack.data, "acks", 4) == 0)
							printf("Subscribed to topic.\n");
						else if(strncmp(sub_ack.data, "ackf", 4) == 0)
							printf("Already subscribed to topic.\n");
					}
				} else {
					cout<< "Usage : subscribe <TOPIC> <SF>\n";
				}
			} else if (strncmp(buffer, "unsubscribe", 11) == 0) {
				// Unsubscribe from topic
				if(*(buffer + 12) != '\0') {
					struct subscribe_message unsub_msg;
					memset(&unsub_msg, 0, sizeof(struct subscribe_message));

					// parse input
					string s = buffer + 12;
					if(strncmp(s.c_str(), " ", 1) == 0) {
						cout<< "Usage : unsubscribe <TOPIC>\n";
						continue;
					}
					memcpy(&unsub_msg.topic, s.c_str(), sizeof(s.size()));
					unsub_msg.type = 'u';

					struct tcp_packet unsub_pkt;
					memset(&unsub_pkt, 0, sizeof(struct tcp_packet));
					memset(&unsub_pkt.tcp_msg, 0, sizeof(struct tcp_message));
					memcpy(&unsub_pkt.sub_msg, &unsub_msg, sizeof(struct subscribe_message));

					// Send unsub message
					ret = send(sockfd, &unsub_pkt, sizeof(struct tcp_packet), 0);
					DIE(ret < 0, "Send unsubscribe message failed");

					// Receive ack
					struct tcp_message unsub_ack;
					memset(&unsub_ack, 0, sizeof(struct tcp_message));
					ret = recv(sockfd, &unsub_ack, sizeof(struct tcp_message), 0);
					DIE(ret < 0, "Ack receive failed");

					if(strncmp(unsub_ack.data, "acks", 4) == 0)
						printf("Unsubscribed from topic.\n");
					else if(strncmp(unsub_ack.data, "ackf", 4) == 0)
						printf("Unknown topic.\n");
				} else {
					cout<< "Usage : unsubscribe <TOPIC>\n";
				}
			} else {
				printf("Unknown command.\n");
			}
		} else {
			struct udp_packet udp_pkt;
			memset(&udp_pkt, 0, sizeof(struct udp_packet));
			ret = recv(sockfd, &udp_pkt, sizeof(struct udp_packet), 0);
			DIE(ret < 0, "Receive failed");

			// Server is offline
			if(ret == 0)
				break;

			// INT
			if (udp_pkt.udp_msg.type == 0) {
				uint8_t sign;
				uint32_t number;
				memcpy(&sign, udp_pkt.udp_msg.contents, 1);
				memcpy(&number, udp_pkt.udp_msg.contents + 1, 4);
				number = ntohl(number);

				if(sign == 1)
					number = (-1)*number;
				
				printf("%s:%d - %s - INT - %d\n", inet_ntoa(udp_pkt.udp_ip), ntohs(udp_pkt.udp_port), udp_pkt.udp_msg.topic, number);

			// SHORT REAL
			} else if (udp_pkt.udp_msg.type == 1) {
				uint16_t number;
				memcpy(&number, udp_pkt.udp_msg.contents, 2);
				number = ntohs(number);

				printf("%s:%d - %s - SHORT_REAL - %.2f\n", inet_ntoa(udp_pkt.udp_ip), ntohs(udp_pkt.udp_port), udp_pkt.udp_msg.topic, (float)(number/100.00));

			// FLOAT
			} else if (udp_pkt.udp_msg.type == 2) {
				uint8_t sign;
				uint8_t rest;
				uint32_t number;
				memcpy(&sign, udp_pkt.udp_msg.contents, 1);
				memcpy(&number, udp_pkt.udp_msg.contents + 1, 4);
				memcpy(&rest, udp_pkt.udp_msg.contents + 5, 1);
				number = ntohl(number);
				float rez = number;
				uint8_t digits = rest;

				while (rest != 0) {
					rest--;
					rez /= 10;
				}

				if(sign == 1) {
					rez = (-1) * rez;
				}

				printf("%s:%d - %s - FLOAT - %.*f\n", inet_ntoa(udp_pkt.udp_ip), ntohs(udp_pkt.udp_port), udp_pkt.udp_msg.topic, digits, rez);

			// STRING
			} else if (udp_pkt.udp_msg.type == 3) {
				printf("%s:%d - %s - STRING - %s\n", inet_ntoa(udp_pkt.udp_ip), ntohs(udp_pkt.udp_port), udp_pkt.udp_msg.topic, udp_pkt.udp_msg.contents);
			}
			
		}
	}

	close(sockfd);

	return 0;
}
