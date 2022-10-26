# Client-server-application-TCP-and-UDP-for-message-control
- C++ client-server app that implements TCP and UDP functionalities
- The topology of the scheme is : 2 routers connected with each other, both with 2 stations connected;
- Both routers implement the same protocols;
- The idea was to simulate this type of network in C;
- ARP Protocol (Request / Reply) with associated ARP table for routers;
- Routing IPV4 Table;
- Routing process consists in verifying checksum, ttl, there is a route to destination and that the packet is not an ICMP packet;
- Longest prefix match(LPM) for getting the best route with binary search in Routing Table;
- ICMP Protocol for ping functionality and error management;
- Incremental Checksum;
