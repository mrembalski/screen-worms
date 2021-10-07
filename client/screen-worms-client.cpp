#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <set>
#include <unistd.h>
#include <vector>
#include <algorithm>

#include "../common/errors.h"
#include "gui.h"
#include "server.h"
#include "constants.h"
#include "../common/texts.h"

#define MESSAGE_INTERVAL 30
#define MAX_UDP_RECEIVE_SIZE 550

uint64_t microseconds_of_start;

int client_server_socket;
int client_gui_socket;

uint8_t pressed_key;
sem_t pressed_key_mutex;

uint32_t next_expected_event_no;
uint32_t my_next_expected_event_no;
sem_t next_expected_event_no_mutex;

std::string player_name = "";
std::string game_server; //will be provided
std::string game_server_port = "2021";
std::string gui_server = "localhost";
std::string gui_server_port = "20210";

pthread_t client_server_communication_thread;
pthread_t client_gui_communication_thread;
pthread_t server_communication_thread;
pthread_attr_t thread_attr;
std::set <uint32_t> game_ids;

char buffer[551];
int len;
uint32_t reader_place;

uint32_t get_next_expected_event_no()
{
    sem_wait(&next_expected_event_no_mutex);
    uint32_t tmp = next_expected_event_no;
    sem_post(&next_expected_event_no_mutex);

    return tmp;
}

void set_next_expected_event_no(uint32_t to_set)
{
    sem_wait(&next_expected_event_no_mutex);
    next_expected_event_no = to_set;
    sem_post(&next_expected_event_no_mutex);
}

uint32_t next_bytes(uint8_t how_many)
{
    uint32_t score = 0;

    for (int i = 0; i < how_many; i++)
    {
        score *= 256;
        score += (uint8_t)buffer[reader_place];
        reader_place += 1;
    }

    return score;
}

uint32_t game_id;
uint32_t event_len;
uint32_t event_no;
uint32_t maxx;
uint32_t maxy;
uint8_t event_type;

uint32_t x_to_send;
uint32_t y_to_send;
std::string string_to_send;

std::vector<std::string> received_names;
std::vector<std::string> players_names;

#define NEW_GAME_CONST 13

void ensure_correct_names()
{
    int number_of_names = 0;
    int length_of_current_name = 0;

    for (int i = 0; i < (int) event_len - NEW_GAME_CONST; i++)
    {
        if (buffer[reader_place] == 0)
        {
            if (length_of_current_name == 0)
                fatal("Too many null characters");

            received_names.push_back(std::string(buffer + reader_place - length_of_current_name, buffer + reader_place - length_of_current_name + length_of_current_name));
            length_of_current_name = 0;
            number_of_names++;
        }
        else
        {
            if (!((int)buffer[reader_place] >= 33 && (int)buffer[reader_place] <= 126))
                fatal("Invalid characters.");

            length_of_current_name += 1;

            if (length_of_current_name > 20)
                fatal("Name too long.");
        }

        if (i + 1 == (int) event_len - NEW_GAME_CONST && buffer[reader_place] != 0)
            fatal("Not null character at end.");

        reader_place++;
    }

    if (number_of_names > 25)
        fatal("Too many names");
}

std::string message_type[3] = {
    "NEW_GAME",
    "PIXEL",
    "PLAYER_ELIMINATED"};

void send_to_gui(uint8_t event_type)
{
    std::string message;

    message.append(message_type[event_type] + " ");

    if (event_type == 0 || event_type == 1)
    {
        message.append(std::to_string(x_to_send) + " " + std::to_string(y_to_send) + " ");
    }

    message.append(string_to_send);
    message.append("\n");

    int64_t x = write(client_gui_socket,
                   message.c_str(),
                   message.length());

    if (x != (int64_t) message.length())
        fatal("invalid send to gui");

    string_to_send.clear();

    std::cout << "sent to gui " << x << " bytes. message: " << message << std::endl;
}

uint32_t crc32(const char *buf, size_t size)
{
    const uint8_t *p = (uint8_t *)buf;
    uint32_t crc;

    crc = ~0U;

    while (size--)
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

    return crc ^ ~0U;
}

void ensure_player_exists(uint8_t player_number)
{
    if (player_number < received_names.size())
        string_to_send = received_names[player_number];
    else
        fatal("Index out of bounds (1).");
}
uint32_t my_x;
uint32_t my_y;

void ensure_valid_x_y()
{
    if (my_x <= x_to_send || my_y <= y_to_send) 
        fatal("invalid x y received");
}

uint32_t my_game_id;

void *receive_server_messages(void *arg)
{
    (void) arg;

    while ((len = read(client_server_socket, buffer, 551)) >= 0)
    {
        std::cout << "Received message " << len << std::endl;
        reader_place = 0;

        if (len > MAX_UDP_RECEIVE_SIZE || len < 4)
            fatal("Invalid message size.");
        std::cout << "len:" << len << std::endl;

        if (reader_place + 4 > (uint32_t) len)
            fatal("Invalid message size, game_id");
        game_id = next_bytes(4);
        std::cout << "game_id:" << game_id << std::endl;

        while (true)
        {
            auto begin_of_message = reader_place;

            if (reader_place == (uint32_t) len)
                break;

            if (reader_place + 4 > (uint32_t) len)
                fatal("Invalid message size, event_len");
            event_len = next_bytes(4);
            std::cout << "event_len:" << event_len << std::endl;

            if (reader_place + 4 > (uint32_t) len)
                fatal("Invalid message size, event_no");
            event_no = next_bytes(4);
            std::cout << "event_no:" << event_no << std::endl;

            if (reader_place + 1 > (uint32_t) len)
                fatal("Invalid message size, event_type");
            event_type = next_bytes(1);
            std::cout << "event_type:" << (int)event_type << std::endl;

            if (event_type == 0)
            {
                if (event_no != 0)
                {
                    fatal("New game without event_no 0");
                }

                if (game_id != my_game_id && game_ids.find(game_id) == game_ids.end())
                {
                    game_ids.insert(game_id);
                    set_next_expected_event_no(1);
                    my_game_id = game_id;

                    if (reader_place + 4 > (uint32_t) len)
                        fatal("Invalid message size, x_to_send");
                    x_to_send = next_bytes(4); my_x = x_to_send;
                    std::cout << "x_to_send:" << x_to_send << std::endl;

                    if (reader_place + 4 > (uint32_t) len)
                        fatal("Invalid message size, y_to_send");
                    y_to_send = next_bytes(4); my_y = y_to_send;
                    std::cout << "y_to_send:" << y_to_send << std::endl;

                    if (reader_place + (event_len - (4 + 1 + 4 + 4)) > (uint32_t) len)
                        fatal("Invalid message size, player_names");

                    received_names.clear();
                    ensure_correct_names();

                    std::sort(received_names.begin(), received_names.end());

                    for (size_t i = 0; i < received_names.size(); ++i)
                    {
                        string_to_send += received_names[i];

                        if (i + 1 != received_names.size())
                            string_to_send += " ";
                    }
                }
                else
                {
                    if (reader_place + (event_len - (4 + 1)) > (uint32_t) len)
                        fatal("Invalid message size, not my_game_id event_type 0");
                    reader_place += event_len - (4 + 1);
                }
            }
            else if (event_type == 1)
            {
                if (game_id == my_game_id && get_next_expected_event_no() == event_no)
                {
                    set_next_expected_event_no(event_no + 1);
                    if (reader_place + 1 > (uint32_t) len)
                        fatal("Invalid message size, player_number");
                    auto player_number = next_bytes(1);

                    if (reader_place + 4 > (uint32_t) len)
                        fatal("Invalid message size, x_to_send");
                    x_to_send = next_bytes(4);

                    if (reader_place + 4 > (uint32_t) len)
                        fatal("Invalid message size, y_to_send");
                    y_to_send = next_bytes(4);

                    ensure_valid_x_y();

                    ensure_player_exists(player_number);
                }
                else
                {
                    if (reader_place + 9 > (uint32_t) len)
                        fatal("Invalid message size, not my_game_id event_type 1");
                    reader_place += 9;
                }
            }
            else if (event_type == 2)
            {
                if (game_id == my_game_id && get_next_expected_event_no() == event_no)
                {
                    set_next_expected_event_no(event_no + 1);
                    if (reader_place + 1 > (uint32_t) len)
                        fatal("Invalid message size, player_number");
                    auto player_number = next_bytes(1);

                    ensure_player_exists(player_number);
                }
                else
                {
                    if (reader_place + 1 > (uint32_t) len)
                        fatal("Invalid message size, not my_game_id event_type 2");
                    reader_place += 1;
                }
            }
            else if (event_type == 3 && game_id == my_game_id && get_next_expected_event_no() == event_no)
            {
                set_next_expected_event_no(event_no + 1);
                received_names.clear();
            }
            else if (event_type > 3)
            {
                /* unknown type  */
                if (reader_place + (event_len - (4 + 1)) > (uint32_t) len)
                    fatal("Invalid message size, not my_game_id event_type 0");
                reader_place += event_len - (4 + 1);
            }

            if (reader_place != begin_of_message + event_len + 4)
                fatal("Invalid message size, event_len too short");

            auto my_crc32 = crc32(&buffer[begin_of_message], reader_place - begin_of_message);

            if (reader_place + 4 > (uint32_t) len)
                fatal("Invalid message size, received_crc");
            auto received_crc = next_bytes(4);

            if (my_crc32 != received_crc)
                break;

            if (game_id == my_game_id && event_type < 3)
                send_to_gui(event_type);
        }
        memset(buffer, 0, len);
    }
    return 0;
}

int opt;

int main(int argc, char *argv[])
{
    struct timeval timer;
    gettimeofday(&timer, NULL);
    microseconds_of_start = timer.tv_sec * 1000 + timer.tv_usec;

    game_server = argv[1];

    argc--;
    for (int i = 0; i < argc; ++i)
        argv[i] = argv[i + 1];

    while ((opt = getopt(argc, argv, "n:p:i:r:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            printf("Player name: %s\n", optarg);
            ensure_valid_player_name(optarg);
            player_name = optarg;
            break;

        case 'p':
            printf("Game server port: %s\n", optarg);
            ensure_valid_number(optarg);
            game_server_port = optarg;

            if (std::stoul(optarg) > 65536)
                fatal("invalid port.");
            break;

        case 'i':
            printf("Gui address: %s\n", optarg);
            gui_server = optarg;
            break;

        case 'r':
            printf("Gui server port: %s\n", optarg);
            ensure_valid_number(optarg);
            gui_server_port = optarg;

            if (std::stoul(optarg) > 65536)
                fatal("invalid port.");
            break;

        default:
            fatal("Usage: %s [-t nsecs] [-n] name\n", argv[0]);
        }
    }

    sem_init(&pressed_key_mutex, 0 /* shared between threads */, 1 /* open at start */);
    sem_init(&next_expected_event_no_mutex, 0 /* shared between threads */, 1 /* open at start */);

    if (optind != argc)
    {
        fatal("Expected argument after options\n");
    }

    if (pthread_attr_init(&thread_attr) != 0)
    {
        fatal("Could not init attr.");
    }

    if (pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE) != 0)
    {
        fatal("Could not set detach state.");
    }

    client_server_socket = setup_client_server_connection(game_server, game_server_port);
    client_gui_socket = setup_client_gui_connection(gui_server, gui_server_port);

    struct server_thread_data server_data = {
        .player_name = player_name,
        .server_socket = client_server_socket,
        .microseconds_of_start = microseconds_of_start,
        .pressed_key_mutex = &pressed_key_mutex,
        .pressed_key_ptr = &pressed_key,
        .next_event_no_mutex = &next_expected_event_no_mutex,
        .next_event_no_ptr = &next_expected_event_no};

    if (pthread_create(&client_server_communication_thread, &thread_attr, send_client_server_messages, &server_data) != 0)
    {
        fatal("Could not create thread for client server communication.");
    }

    struct gui_thread_data gui_data = {
        .player_name = player_name,
        .gui_socket = client_gui_socket,
        .microseconds_of_start = microseconds_of_start,
        .pressed_key_mutex = &pressed_key_mutex,
        .pressed_key_ptr = &pressed_key};

    if (pthread_create(&client_gui_communication_thread, &thread_attr, receive_gui_messages, &gui_data) != 0)
    {
        fatal("Could not create thread for client server communication.");
    }
    if (pthread_create(&server_communication_thread, &thread_attr, receive_server_messages, &server_data) != 0)
    {
        fatal("Could not create thread for server communication.");
    }

    if (pthread_join(server_communication_thread, NULL) != 0)
    {
        fatal("Could not create thread for server communication.");
    }

    exit(EXIT_SUCCESS);
}
