#ifndef __UTILS_h__
#define __UTILS_h__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

struct udp_message {
  char topic[50];
  uint8_t type;
  char contents[1500];
};

struct tcp_message {
  char data[1500];
};

struct subscribe_message {
  char sf; // 0 or 1
  char type; // s or u
  char topic[50];
};

struct tcp_packet {
  struct tcp_message tcp_msg;
  struct subscribe_message sub_msg;
};

struct udp_packet {
  struct in_addr udp_ip;
  uint16_t udp_port;
  struct udp_message udp_msg;
};

#define MAX_CLIENTS 10000
#define BUFLEN    1500
  
#endif // __UTILS_h__