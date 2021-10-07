
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include "../common/errors.h"
#include "udp.h"
#include <poll.h>
#include <sys/timerfd.h>

int setup_timer()
{
    int sckt = timerfd_create(CLOCK_REALTIME, 0);

    if (sckt == -1)
    {
        fatal("timerfd_create");
    }

    return sckt;
}

void change_time(int timer_socket, uint32_t nanoseconds_per_round) {
    struct itimerspec ts;
   
    ts.it_interval.tv_sec = nanoseconds_per_round / 1000000000;
    ts.it_interval.tv_nsec = nanoseconds_per_round - nanoseconds_per_round / 1000000000 * 1000000000;
    ts.it_value.tv_sec = nanoseconds_per_round / 1000000000;
    ts.it_value.tv_nsec = nanoseconds_per_round - nanoseconds_per_round / 1000000000 * 1000000000;

    std::cout << "ts.it_interval.tv_sec " << ts.it_interval.tv_sec << std::endl;
    std::cout << "ts.it_interval.tv_nsec " << ts.it_interval.tv_nsec << std::endl;
    std::cout << "ts.it_value.tv_sec " << ts.it_value.tv_sec << std::endl;
    std::cout << "ts.it_value.tv_nsec " << ts.it_value.tv_nsec << std::endl;

    if (timerfd_settime(timer_socket, 0, &ts, NULL) < 0)
    {
        fatal("timerfd_settime");
    }
}