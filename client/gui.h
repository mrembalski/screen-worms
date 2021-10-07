#ifndef _GUI_
#define _GUI_
#include <iostream>
#include <semaphore.h>

int setup_client_gui_connection(const std::string &gui_server, const std::string &gui_server_port);

void *receive_gui_messages(void *arg);

struct gui_thread_data {
    std::string player_name;
    int gui_socket;
    uint64_t microseconds_of_start;

    sem_t *pressed_key_mutex;
    uint8_t *pressed_key_ptr;
};

#endif


