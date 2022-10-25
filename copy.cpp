#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <bits/stdc++.h>

using namespace std;

int main(int argc, char *argv[]) {
    int ret;
    int udp_socket, tcp_socket, newsockfd;
    struct sockaddr_in udp_addr, tcp_addr, cli_addr;
    int disable_nagle = 1;
    int enable_reuse = 1;
    socklen_t clilen;
    fd_set read_fds, tmp_fds;
    int fdmax;
    char buffer[BUFLEN];
    unordered_map<string, int> clients_sock;
    unordered_map<int, string> sock_clients;
    unordered_map<string, set<string>> topic_cl;
    unordered_map<string, unordered_map<string, char>> clients_sf;
    unordered_map<string, queue<struct udp_packet>> message_q;

    // Creating socket file descriptor for UDP
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("UDP socket creation failed");
        exit(EXIT_FAILURE);
    } 
    
    // Creating socket file descriptor for TCP
    if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        fprintf(stderr, "Eg: %s 10001\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memset(&udp_addr, 0, sizeof(struct sockaddr_in));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(atoi(argv[1]));

    memset(&tcp_addr, 0, sizeof(struct sockaddr_in));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(atoi(argv[1]));

    int reuse;
    reuse = setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(int));
    DIE(reuse < 0, "Reuse port failed");

    reuse = setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(int));
    DIE(reuse < 0, "Reuse port failed");

    int nagle = setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &disable_nagle, sizeof(int));
    DIE(nagle < 0, "Disable for Nagle's Algorithm failed");

    int bind_udp = bind(udp_socket, (const struct sockaddr *)&udp_addr, sizeof(struct sockaddr));
    DIE(bind_udp < 0, "Bind for UDP failed");

    int bind_tcp = bind(tcp_socket, (const struct sockaddr *)&tcp_addr, sizeof(struct sockaddr));
    DIE(bind_tcp < 0, "Bind for TCP failed");

    int listen_tcp = listen(tcp_socket, MAX_CLIENTS);
    DIE(listen_tcp < 0, "Listen for TCP socket failed");

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    FD_SET(udp_socket, &read_fds);
    FD_SET(tcp_socket, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    fdmax = max(udp_socket, tcp_socket);

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int loop = 1;

    while(loop) {
        tmp_fds = read_fds;

        int sel = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(sel < 0, "Select failed");
        
        for(int i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &tmp_fds)) {
                if(i == udp_socket) {
                    struct sockaddr_in cliaddr;
                    socklen_t cliaddr_len = sizeof(cliaddr);
                    
                    struct udp_message udp_msg;
                    memset(&udp_msg, 0, sizeof(struct udp_message));
                    ret = recvfrom(udp_socket, &udp_msg, sizeof(struct udp_message), 0, (struct sockaddr *)&cliaddr, &cliaddr_len);
                    DIE(ret <= 0, "Receive from UDP client failed");

                    string aux = udp_msg.topic;
                    if(topic_cl.find(aux) == topic_cl.end()) {
                        continue;
                    }

                    /*
                    struct udp_packet udp_pkt;
                    memset(&udp_pkt, 0, sizeof(struct udp_packet));
                    memcpy(&udp_pkt.udp_msg, &udp_msg, sizeof(struct udp_message));
                    memcpy(&udp_pkt.udp_ip, &cliaddr.sin_addr, sizeof(struct in_addr));
                    memcpy(&udp_pkt.udp_port, &cliaddr.sin_port, sizeof(short));*/


                    for(const string &client : topic_cl[aux]) {
                        if(clients_sock.find(client) != clients_sock.end()){
                            int client_socket = clients_sock.at(client);
                            ret = send(client_socket, &udp_pkt, sizeof(struct udp_packet), 0);
                            DIE(ret < 0, "Send UDP packet to TCP client failed");
                        } else {
                            if (clients_sf[client][aux] == 1) {
                                message_q[client].push(udp_pkt);
                            }
                        }
                    }
                } else if (i == tcp_socket) {
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(tcp_socket, (struct sockaddr *)&cli_addr, &clilen);
                    DIE(newsockfd < 0, "Accept failed");

                    reuse = setsockopt(newsockfd, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(int));
                    DIE(reuse < 0, "Reuse failed");

                    nagle = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &disable_nagle, sizeof(int));
                    DIE(nagle < 0, "Disable nagle failed");

                    struct tcp_message tcp_msg;
                    memset(&tcp_msg, 0, sizeof(struct tcp_message));
                    int rcv = recv(newsockfd, &tcp_msg, sizeof(struct tcp_message), 0);
                    DIE(rcv < 0, "Receive failed");

                    if(clients_sock.find(tcp_msg.data) != clients_sock.end()){
                        close(newsockfd);
                        printf("Client %s already connected.\n", tcp_msg.data);
                    } else {
                        clients_sock.insert(make_pair(tcp_msg.data, newsockfd));
                        sock_clients.insert(make_pair(newsockfd, tcp_msg.data));
                        printf("New client %s connected from %s:%d.\n", tcp_msg.data, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                        FD_SET(newsockfd, &read_fds);
                        fdmax = max(fdmax, newsockfd);

                        if (clients_sf.find(tcp_msg.data) != clients_sf.end()) {
                            while (!message_q[tcp_msg.data].empty()) {
                                struct udp_packet to_send = message_q[tcp_msg.data].front();
                                message_q[tcp_msg.data].pop();

                                ret = send(newsockfd, &to_send, sizeof(struct udp_packet), 0);
                                DIE(ret < 0, "Send unreceived messages failed");
                            }
                        }
                    }
                } else if (i == STDIN_FILENO) {
                    memset(buffer, 0, BUFLEN);
                    fgets(buffer, BUFLEN-1, stdin);

                    if(strncmp(buffer, "exit", 4) == 0) {
                        loop = 0;
                        for(int k = 0; k <= fdmax; k++) {
                            if(FD_ISSET(k, &read_fds) && k != STDIN_FILENO && k != tcp_socket && k != udp_socket) {
                                close(k);   
                                FD_CLR(k, &read_fds);
                            }
                        }
                        break;
                    } else {
                        printf("Unknown command");
                    }
                } else {
                    struct tcp_packet client_pkt;
                    memset(&client_pkt, 0, sizeof(struct tcp_packet));
                
                    ret = recv(i, &client_pkt, sizeof(struct tcp_packet), 0);
                    DIE(ret < 0, "Receive from client failed");

                    if(ret == 0) {
                        string id_client = sock_clients.at(i);
                        clients_sock.erase(id_client);
                        sock_clients.erase(i);
                        close(i);
                        FD_CLR(i, &read_fds);
                        printf("Client %s disconnected.\n", id_client.c_str());
                    } else {
                        struct subscribe_message sub_op;
                        memcpy(&sub_op, &client_pkt.sub_msg, sizeof(struct subscribe_message));
                        
                        if (sub_op.type == 's') {
                            if (topic_cl.find(sub_op.topic) != topic_cl.end()) {
                                topic_cl[sub_op.topic].insert(sock_clients.at(i));
                            } else {  
                                set<string> new_set;
                                new_set.insert(sock_clients.at(i));
                                topic_cl.insert(make_pair(sub_op.topic, new_set));
                            }

                            unordered_map<string, char> new_map;

                            if (clients_sf.find(sock_clients.at(i)) != clients_sf.end()) {
                                clients_sf[sock_clients.at(i)].insert(make_pair(sub_op.topic, sub_op.sf));
                            } else {
                                new_map.insert(make_pair(sub_op.topic, sub_op.sf));
                                clients_sf.insert(make_pair(sock_clients.at(i), new_map));
                            }
                            if(sub_op.sf == 1) {
                                queue<struct udp_packet> sf_q;
                                message_q.insert(make_pair(sock_clients.at(i), sf_q));
                            }

                            struct tcp_message ack_msg;
                            memset(&ack_msg, 0, sizeof(struct tcp_message));
                            strcpy(ack_msg.data, "ack");

                            ret = send(i, &ack_msg, sizeof(struct tcp_message), 0);
                            DIE(ret < 0, "Ack sub failed");

                        } else if (sub_op.type == 'u') {
                            if(topic_cl.find(sub_op.topic) != topic_cl.end()) {
                                topic_cl[sub_op.topic].erase(sock_clients.at(i));
                                if(clients_sf[sock_clients.at(i)][sub_op.topic] == 1) {
                                    message_q.erase(sock_clients.at(i));
                                }
                                clients_sf.erase(sock_clients.at(i));
                            }

                            struct tcp_message ack_msg;
                            memset(&ack_msg, 0, sizeof(struct tcp_message));
                            strcpy(ack_msg.data, "ack");

                            ret = send(i, &ack_msg, sizeof(struct tcp_message), 0);
                            DIE(ret < 0, "Ack unsub failed");
                        }
                    }
                }
            }
        }
    }
    
    close(udp_socket);
    close(tcp_socket);

    return 0;
}