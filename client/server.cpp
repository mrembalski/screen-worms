#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

#include "server.h"
#include "../common/errors.h"

#define MESSAGE_INTERVAL 30
#define BASE_MESSAGE_SIZE 13

int setup_client_server_connection(const std::string &game_server, const std::string &game_server_port)
{
    struct addrinfo server_addr_hints;
    struct addrinfo *server_addr_result;

    (void)memset(&server_addr_hints, 0, sizeof(struct addrinfo));

    server_addr_hints.ai_family = AF_UNSPEC;
    server_addr_hints.ai_socktype = SOCK_DGRAM;
    server_addr_hints.ai_protocol = IPPROTO_UDP;
    server_addr_hints.ai_flags = 0;
    server_addr_hints.ai_addrlen = 0;
    server_addr_hints.ai_addr = NULL;
    server_addr_hints.ai_canonname = NULL;
    server_addr_hints.ai_next = NULL;

    if (getaddrinfo(game_server.c_str(), game_server_port.c_str(), &server_addr_hints, &server_addr_result) != 0)
    {
        fatal("Could not get address info.");
    }

    int client_server_socket = socket(server_addr_result->ai_family, SOCK_DGRAM, server_addr_result->ai_protocol); //or 0

    if (client_server_socket < 0)
    {
        fatal("Could not establish connection.");
    }

    if (connect(client_server_socket, server_addr_result->ai_addr, server_addr_result->ai_addrlen) < 0)
    {
        fatal("UDP connect.");
    }

    freeaddrinfo(server_addr_result);

    return client_server_socket;
}

void send_client_server_message(struct server_thread_data *data)
{
    std::vector<uint8_t> message;

    //session_id
    message.push_back((uint8_t)(data->microseconds_of_start >> 56));
    message.push_back((uint8_t)(data->microseconds_of_start >> 48));
    message.push_back((uint8_t)(data->microseconds_of_start >> 40));
    message.push_back((uint8_t)(data->microseconds_of_start >> 32));
    message.push_back((uint8_t)(data->microseconds_of_start >> 24));
    message.push_back((uint8_t)(data->microseconds_of_start >> 16));
    message.push_back((uint8_t)(data->microseconds_of_start >> 8));
    message.push_back((uint8_t)(data->microseconds_of_start));

    //turn_direction
    sem_wait(data->pressed_key_mutex);
    uint8_t pressed_key_value = *(data->pressed_key_ptr);
    sem_post(data->pressed_key_mutex);

    message.push_back((uint8_t)(pressed_key_value));

    //next_expected_event_no
    sem_wait(data->next_event_no_mutex);
    uint32_t next_expected_event_no = *(data->next_event_no_ptr);
    sem_post(data->next_event_no_mutex);

    message.push_back((uint8_t)(next_expected_event_no >> 24));
    message.push_back((uint8_t)(next_expected_event_no >> 16));
    message.push_back((uint8_t)(next_expected_event_no >> 8));
    message.push_back((uint8_t)(next_expected_event_no));

    //player_name
    for (const char &c : data->player_name)
        message.push_back((uint8_t)(c));

    int64_t snd_len = write(
        data->server_socket,
        message.data(),
        message.size());

    if (snd_len != (int64_t) message.size())
    {
        fatal("Partial or failed write.");
    }
}

void *send_client_server_messages(void *arg)
{
    struct server_thread_data *data = (server_thread_data *)arg;

    struct timeval time_struct;
    gettimeofday(&time_struct, NULL);

    uint64_t current_value, last_value = (time_struct.tv_sec * 1000000 + time_struct.tv_usec) / 1000;

    while (true)
    {
        gettimeofday(&time_struct, NULL);

        current_value = (time_struct.tv_sec * 1000000 + time_struct.tv_usec) / 1000;

        if (current_value - last_value < MESSAGE_INTERVAL)
        {
            usleep((MESSAGE_INTERVAL - (current_value - last_value)) * 1000);
        }
        else
        {
            last_value = current_value;
            send_client_server_message(data);
        }
    }
}
