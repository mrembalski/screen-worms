#ifndef _SERVER_
#define _SERVER_
#include <iostream>
#include <semaphore.h>

int setup_client_server_connection(const std::string &game_server, const std::string &game_server_port);
void *send_client_server_messages(void *arg);

struct server_thread_data {
    std::string player_name;
    int server_socket;
    uint64_t microseconds_of_start;

    sem_t *pressed_key_mutex;
    uint8_t *pressed_key_ptr;

    sem_t *next_event_no_mutex;
    uint32_t *next_event_no_ptr;
};

#endif
