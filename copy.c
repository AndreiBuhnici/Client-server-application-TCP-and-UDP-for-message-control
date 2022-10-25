#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int udp_socket, tcp_socket;
    struct sockaddr_in udp_addr, tcp_addr;
    int enable_nagle = 1;
    fd_set read_fds, tmp_fds;
    int fdmax;
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
        fprintf(stderr, "Usage: ./%s <port>\n", argv[0]);
        fprintf(stderr, "Eg: ./%s 10001\n", argv[0]);
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

    int bind_udp = bind(udp_socket, (const struct sockaddr *)&udp_addr, sizeof(struct sockaddr));
    DIE(bind_udp < 0, "Bind for UDP failed");

    int bind_tcp = bind(tcp_socket, (const struct sockaddr *)&tcp_addr, sizeof(struct sockaddr));
    DIE(bind_tcp < 0, "Bind for TCP failed");

    int nagle = setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &enable_nagle, sizeof(int));
    DIE(nagle < 0, "Disable for Nagle's Algorithm failed");

    int listen_tcp = listen(tcp_socket, MAX_CLIENTS);
    DIE(listen_tcp < 0, "Listen for TCP socket failed");

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    FD_SET(udp_socket, &read_fds);
    FD_SET(tcp_socket, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    if(udp_socket > tcp_socket)
        fdmax = udp_socket;
    else
        fdmax = tcp_socket;

    int loop = 1;

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    while(loop) {
        tmp_fds = read_fds;

        int sel = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(sel < 0, "Select failed");

        for(int i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &tmp_fds)) {
                if(i == udp_socket) {

                }
            }
        }
    }
    
    close(udp_socket);
    close(tcp_socket);

    return 0;
}
