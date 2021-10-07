
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include "../common/errors.h"
#include "udp.h"

int setup_connection(uint32_t server_port)
{
    struct sockaddr_in6 server {};

    int sckt = socket(AF_INET6, SOCK_DGRAM, 0);

    if (sckt == -1)
    {
        fatal("Opening stream socket");
    }

    server.sin6_family = AF_INET6;
    server.sin6_addr = in6addr_any;
    server.sin6_port = htons(server_port);

    if (bind(sckt, (struct sockaddr *)&server, (socklen_t)sizeof(server)) < 0)
        fatal("bind");

    return sckt;
}
