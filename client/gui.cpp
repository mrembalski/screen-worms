#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <uv.h>
#include "gui.h"
#include "../common/errors.h"

int setup_client_gui_connection(const std::string &gui_server, const std::string &gui_server_port)
{
    struct addrinfo gui_addr_hints;
    struct addrinfo *gui_addr_result;
    int err;

    memset(&gui_addr_hints, 0, sizeof(struct addrinfo));
    gui_addr_hints.ai_family = AF_UNSPEC;
    gui_addr_hints.ai_socktype = SOCK_STREAM;
    gui_addr_hints.ai_protocol = IPPROTO_TCP;

    err = getaddrinfo(gui_server.c_str(), gui_server_port.c_str(), &gui_addr_hints, &gui_addr_result);

    if (err != 0)
    {
        fatal("getaddrinfo");
    }

    int client_gui_socket = socket(gui_addr_result->ai_family, gui_addr_result->ai_socktype, gui_addr_result->ai_protocol);

    if (client_gui_socket < 0)
    {
        fatal("Socket failure.");
    }

    int const flag{1};

    if (setsockopt(client_gui_socket, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(int)) < 0)
    {
        fatal("setsockopt.");
    }


    if (connect(client_gui_socket, gui_addr_result->ai_addr, gui_addr_result->ai_addrlen) < 0)
    {
        fatal("Could not connect.");
    }

    freeaddrinfo(gui_addr_result);

    return client_gui_socket;
}


void *receive_gui_messages(void *arg)
{
    struct gui_thread_data *data = (gui_thread_data *) arg;

    int len;
    char *buffer;
    size_t ll;

    auto read_socket = fdopen(data->gui_socket, "r");

    while (true)
    {
        len = getline(&buffer, &ll, read_socket);
        
        if (len == -1)
            fatal("Dead GUI.");

        if (len == 0) 
            continue;

        sem_wait(data->pressed_key_mutex);
        
        if (len == 12 && strncmp(buffer, "LEFT_KEY_UP\n", len) == 0)
        {
            if ((*(data->pressed_key_ptr)) == 2) {
                (*(data->pressed_key_ptr)) = 0;
                puts("0");
            }
        }
        else if (len == 13 && strncmp(buffer, "RIGHT_KEY_UP\n", len) == 0)
        {
            if ((*(data->pressed_key_ptr)) == 1) {
                (*(data->pressed_key_ptr)) = 0;
                puts("0");
            }
        }
        else if (len == 14 && strncmp(buffer, "LEFT_KEY_DOWN\n", len) == 0)
        {  
            puts("2");
            (*(data->pressed_key_ptr)) = 2;
        }
        else if (len == 15 && strncmp(buffer, "RIGHT_KEY_DOWN\n", len) == 0)
        {
            puts("1");
            (*(data->pressed_key_ptr)) = 1;
        }
        else
        {
            //message ignored
        }

        sem_post(data->pressed_key_mutex);
    }
}
