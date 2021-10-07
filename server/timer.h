#ifndef _TIMER_
#define _TIMER_
#include <iostream>

int setup_timer();
void change_time(int timer_socket, uint32_t nanoseconds_per_round);

#endif
