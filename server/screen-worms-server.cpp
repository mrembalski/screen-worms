#include <limits.h>
#include <poll.h>
#include <sys/epoll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <math.h>
#include <errno.h>
#include <sys/time.h>
#include "../common/errors.h"
#include "../common/texts.h"
#include "udp.h"
#include "mixed.h"
#include "timer.h"
#include "events.h"
#include "structures.h"

#define MAX_EVENTS 10
#define MAX_MESSAGE_SIZE 33
#define MIN_MESSAGE_SIZE 13

void save_event(std::vector<uint8_t> _event);
void send_events_to_someone(uint32_t _from, uint32_t _to, comp_addr receiver);

uint32_t crc32(const uint8_t *buf, size_t size)
{
    const uint8_t *p = buf;
    uint32_t crc;

    crc = ~0U;

    while (size--)
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

    return crc ^ ~0U;
}

std::vector<std::vector<uint8_t>> events;
uint32_t server_port;
uint32_t turning_speed;
uint32_t rounds_per_sec;
uint32_t nanoseconds_per_round;
uint32_t width;
uint32_t height;
uint32_t seed;
uint64_t last_generated;
uint32_t game_id;
uint8_t number_of_players;
bool **board;
bool seed_specified;
bool game_is_running;
bool has_been_generated;
std::map<comp_addr, client_t> clients;
std::map<std::string, player_t> players;
int UDP_SCKT, TIMER_SCKT;
uint64_t ROUND;

uint32_t my_rand()
{
    uint64_t ans;

    if (!has_been_generated)
    {
        if (seed_specified)
            ans = seed;
        else
            ans = time(NULL);

        has_been_generated = true;
    }
    else
        ans = ((last_generated * 279410273) % 4294967291);

    last_generated = ans;

    return (uint32_t)(ans);
}

std::vector<std::string> get_players_names()
{
    std::vector<std::string> names;

    for (auto it = players.begin(); it != players.end(); it++)
        names.push_back(it->first);

    return names;
}

uint64_t next_bytes(uint8_t how_many, uint8_t from, char *buffer)
{
    uint64_t score = 0;

    for (int i = 0; i < how_many; i++)
    {
        score *= 256;
        score += (uint8_t)buffer[from + i];
    }

    return score;
}

bool check_if_free_player_name(const std::string &name) {
    int no_of_names = 0;

    for (auto it = clients.begin(); it != clients.end(); it++)
    {
        if (!it->second.player_name.empty()) {
            no_of_names++;

            if (it->second.player_name == name) 
                return false;
        }
    }

    if (no_of_names < 25)
        return true;

    return false;
}

void update_server();
bool can_game_get_started();
void receive_message()
{
    // puts("message received");
    char buf[MAX_MESSAGE_SIZE + 1];
    memset(buf, 0, MAX_MESSAGE_SIZE + 1);

    struct sockaddr_in6 client_address;
    socklen_t rcva_len = sizeof(struct sockaddr_in6);

    auto len = recvfrom(UDP_SCKT, buf, MAX_MESSAGE_SIZE + 1, 0, (struct sockaddr *)&client_address, &rcva_len);

    if (len < MIN_MESSAGE_SIZE || len > MAX_MESSAGE_SIZE)
    {
        // puts("invalid size");

        // for (int i = 0; i < len; i++)
        // {
        //     //std::cout << buf[len] << std::endl;
        // }
        return;
    }

    uint64_t session_id = next_bytes(8, 0, buf);
    uint8_t turn_direction = next_bytes(1, 8, buf);
    uint32_t next_expected_event_no = next_bytes(4, 9, buf);

    std::string player_name;

    for (int i = 0; i < len - MIN_MESSAGE_SIZE; i++)
    {
        if (!is_valid_character(buf[MIN_MESSAGE_SIZE + i]))
        {
            // puts("invalid name");
            return;
        }
        player_name += buf[MIN_MESSAGE_SIZE + i];
    }

    if (player_name.size() > 20)
    {
        // puts("name over 20");
        return;
    }

    auto sender = comp_addr(client_address);

    if (clients.find(sender) == clients.end())
    {
        if (!player_name.empty() && !check_if_free_player_name(player_name)) {
            //std::cout << player_name << " not empty and not free" << std::endl;
            return;
        }

        //std::cout << "new client:" << std::endl;
        //std::cout << "session_id " << session_id << ", turn_direction " << turn_direction << ", next_expected_event_no " << next_expected_event_no << ", player_name " << player_name << std::endl;

        clients[sender] = client_t{
            .last_round_message_sent = ROUND,
            .session_id = session_id,
            .turn_direction = turn_direction,
            .player_name = player_name,
            .player_ready = (turn_direction == 1 || turn_direction == 2)};
    }
    else
    {
        if (session_id == clients[sender].session_id && player_name == clients[sender].player_name)
        {
            // puts("update");
            clients[sender].last_round_message_sent = ROUND;
            clients[sender].turn_direction = turn_direction;
            clients[sender].player_ready = clients[sender].player_ready || (turn_direction == 1 || turn_direction == 2);

            if (players.find(clients[sender].player_name) != players.end() && players.find(clients[sender].player_name)->second.owner == sender)
            {
                //std::cout << clients[sender].player_name << " updating last_direction to " << turn_direction << std::endl;
                players.find(clients[sender].player_name)->second.last_direction = turn_direction;
            }
        }
        //new connection
        else if (session_id > clients[sender].session_id)
        {
            // puts("new sess id");

            clients[sender] = client_t{
                .last_round_message_sent = ROUND,
                .session_id = session_id,
                .turn_direction = turn_direction,
                .player_name = player_name,
                .player_ready = (turn_direction == 1 || turn_direction == 2)};
        }
        //bad datagram
        else
        {
            // puts("bad datagram");
            return;
        }
    }

    send_events_to_someone(next_expected_event_no, events.size() - 1, sender);
}

void send_events(size_t _from, size_t _to)
{
    std::vector<uint8_t> message;

    message.push_back((uint8_t)((game_id) >> 24));
    message.push_back((uint8_t)((game_id) >> 16));
    message.push_back((uint8_t)((game_id) >> 8));
    message.push_back((uint8_t)((game_id)));

    for (size_t i = _from; i <= _to; ++i)
    {
        if (message.size() + events[i].size() + 12 <= 550)
        {
            uint32_t sum_of_event = 4 + events[i].size();

            std::vector<uint8_t> new_event;

            new_event.push_back((uint8_t)((sum_of_event) >> 24));
            new_event.push_back((uint8_t)((sum_of_event) >> 16));
            new_event.push_back((uint8_t)((sum_of_event) >> 8));
            new_event.push_back((uint8_t)((sum_of_event)));

            new_event.push_back((uint8_t)((i) >> 24));
            new_event.push_back((uint8_t)((i) >> 16));
            new_event.push_back((uint8_t)((i) >> 8));
            new_event.push_back((uint8_t)((i)));

            for (size_t j = 0; j < events[i].size(); j++)
                new_event.push_back(events[i][j]);

            uint32_t event_crc = crc32(new_event.data(), new_event.size());

            for (size_t j = 0; j < new_event.size(); j++)
                message.push_back(new_event[j]);

            message.push_back((uint8_t)((event_crc) >> 24));
            message.push_back((uint8_t)((event_crc) >> 16));
            message.push_back((uint8_t)((event_crc) >> 8));
            message.push_back((uint8_t)((event_crc)));
        }
        else
        {
            for (auto it = clients.begin(); it != clients.end(); ++it)
            {
                int64_t len = sendto(UDP_SCKT, message.data(), message.size(), 0, (const struct sockaddr *)&it->first.addr, (socklen_t)sizeof(it->first.addr));
                len--; //unused
            }

            message.clear();

            message.push_back((uint8_t)((game_id) >> 24));
            message.push_back((uint8_t)((game_id) >> 16));
            message.push_back((uint8_t)((game_id) >> 8));
            message.push_back((uint8_t)((game_id)));
        }
    }

    if (message.size() > 4)
    {
        for (auto it = clients.begin(); it != clients.end(); ++it)
        {
            int len = sendto(UDP_SCKT, message.data(), message.size(), 0, (const struct sockaddr *)&it->first.addr, (socklen_t)sizeof(it->first.addr));
            len--; //unused
        }
    }
}

void send_events_to_someone(uint32_t _from, uint32_t _to, comp_addr receiver)
{
    // //std::cout << "send events from " << _from << " to " << _to << " out of " << events.size() << std::endl;

    // if (_to < _from || _from >= events.size())
    // {
    //     fatal("Send events invalid from to.");
    // }
        
    std::vector<uint8_t> message;

    message.push_back((uint8_t)((game_id) >> 24));
    message.push_back((uint8_t)((game_id) >> 16));
    message.push_back((uint8_t)((game_id) >> 8));
    message.push_back((uint8_t)((game_id)));

    for (auto i = _from; i <= _to && i < events.size(); ++i)
    {
        if (message.size() + events[i].size() + 12 <= 550)
        {
            uint32_t sum_of_event = 4 + events[i].size();

            std::vector<uint8_t> new_event;

            new_event.push_back((uint8_t)((sum_of_event) >> 24));
            new_event.push_back((uint8_t)((sum_of_event) >> 16));
            new_event.push_back((uint8_t)((sum_of_event) >> 8));
            new_event.push_back((uint8_t)((sum_of_event)));

            new_event.push_back((uint8_t)((i) >> 24));
            new_event.push_back((uint8_t)((i) >> 16));
            new_event.push_back((uint8_t)((i) >> 8));
            new_event.push_back((uint8_t)((i)));

            for (size_t j = 0; j < events[i].size(); j++)
                new_event.push_back(events[i][j]);

            uint32_t event_crc = crc32(new_event.data(), new_event.size());

            for (size_t j = 0; j < new_event.size(); j++)
                message.push_back(new_event[j]);

            message.push_back((uint8_t)((event_crc) >> 24));
            message.push_back((uint8_t)((event_crc) >> 16));
            message.push_back((uint8_t)((event_crc) >> 8));
            message.push_back((uint8_t)((event_crc)));
        }
        else
        {
            int64_t len = sendto(UDP_SCKT, message.data(), message.size(), 0, (const struct sockaddr *)&receiver.addr, (socklen_t)sizeof(receiver.addr));
            len--; //unused

            message.clear();

            message.push_back((uint8_t)((game_id) >> 24));
            message.push_back((uint8_t)((game_id) >> 16));
            message.push_back((uint8_t)((game_id) >> 8));
            message.push_back((uint8_t)((game_id)));
        }
    }

    if (message.size() > 4) {
        int64_t len = sendto(UDP_SCKT, message.data(), message.size(), 0, (const struct sockaddr *)&receiver.addr, (socklen_t)sizeof(receiver.addr));
        len--; //unused
    }
}

#define TIMEOUT_IN_MS 2000
#define NS_TO_MS 1000000

void disconnect_timeouted_clients()
{
    std::vector<comp_addr> to_disconnect;

    for (auto it = clients.begin(); it != clients.end(); it++)
    {
        if ((ROUND - it->second.last_round_message_sent) * nanoseconds_per_round / NS_TO_MS > TIMEOUT_IN_MS)
        {
            to_disconnect.push_back(it->first);
        }
    }

    for (auto const &disc : to_disconnect)
    {
        clients.erase(disc);
    }
}

bool can_game_get_started()
{
    bool everyone_ready = true;
    uint64_t number_of_players_ready = 0;

    for (auto it = clients.begin(); it != clients.end(); it++)
    {
        if (!it->second.player_name.empty())
        {
            if (!it->second.player_ready)
            {
                everyone_ready = false;
            }
            else
            {
                number_of_players_ready += 1;
            }
        }
    }

    return (everyone_ready && number_of_players_ready >= 2);
}

void end_game()
{
    save_event(game_over_event());

    number_of_players = 0;
    game_is_running = false;
    players.clear();

    for (auto it = clients.begin(); it != clients.end(); it++)
    {
        it->second.player_ready = false;
        it->second.turn_direction = 0;
    }

    for (size_t i = 0; i < width; i++)
    {
        for (size_t j = 0; j < height; j++)
        {
            board[i][j] = false;
        }
    }
}

void create_players()
{
    ////std::cout << "Starting game." << std::endl;

    std::vector<std::string> to_play;

    for (auto it = clients.begin(); it != clients.end(); it++)
    {
        if (!it->second.player_name.empty())
        {
            to_play.push_back(it->second.player_name);
        }
    }

    std::sort(to_play.begin(), to_play.end());
    number_of_players = 0;
    game_is_running = true;
    game_id = my_rand();

    for (size_t i = 0; i < to_play.size(); i++)
    {
        for (auto it = clients.begin(); it != clients.end(); it++)
        {
            if (it->second.player_name == to_play[i])
            {
                players[to_play[i]] = player_t{
                    .pos_x = (double)(my_rand() % width) + 0.5,
                    .pos_y = (double)(my_rand() % height) + 0.5,
                    .direction = (my_rand() % 360),
                    .last_direction = it->second.turn_direction,
                    .owner = it->first,
                    .is_alive = true,
                    .player_number = number_of_players};

                number_of_players++;
            }
        }
    }
}

bool is_on_board(double x, double y)
{
    return (0 <= x && x < width && 0 <= y && y < height);
}

bool is_eaten(double x, double y)
{
    if (!(0 <= x && x < width && 0 <= y && y < height))
    {
        fatal("is_eaten");
    }

    return board[(int)(x)][(int)(y)];
}

void set_eaten(double x, double y)
{
    if (board[(int)(x)][(int)(y)] == true)
    {
        fatal("set_eaten");
    }

    board[(int)(x)][(int)(y)] = true;
}

bool has_not_moved(double x, double y, double x_2, double y_2)
{
    return ((int)x == (int)x_2 && (int)y == (int)y_2);
}

void update_position(std::map<std::string, player_t>::iterator it)
{
    if (it->second.last_direction == 1)
    {
        it->second.direction = (it->second.direction + turning_speed + 360) % 360;
    }

    if (it->second.last_direction == 2)
    {
        it->second.direction = (it->second.direction - turning_speed + 360) % 360;
    }

    it->second.pos_x += std::cos((double)it->second.direction * M_PI/180);
    it->second.pos_y += std::sin((double)it->second.direction * M_PI/180);
}

void update_player_status(std::map<std::string, player_t>::iterator it, double old_x, double old_y)
{
    if (has_not_moved(it->second.pos_x, it->second.pos_y, old_x, old_y))
        return;

    if (!is_on_board(it->second.pos_x, it->second.pos_y))
    {
        it->second.is_alive = false;
        number_of_players -= 1;
        save_event(player_eliminated_event(it->second.player_number));
    }
    else if (is_on_board(it->second.pos_x, it->second.pos_y) && is_eaten(it->second.pos_x, it->second.pos_y))
    {
        it->second.is_alive = false;
        number_of_players -= 1;
        save_event(player_eliminated_event(it->second.player_number));
    }
    else
    {
        set_eaten(it->second.pos_x, it->second.pos_y);
        save_event(pixel_event((int)(it->second.pos_x), (int)(it->second.pos_y), it->second.player_number));
    }
}

void update_server()
{
    ROUND += 1;

    disconnect_timeouted_clients();

    if (game_is_running)
    {
        for (auto it = players.begin(); it != players.end(); it++)
        {
            if (!it->second.is_alive || number_of_players == 1)
                continue;

            auto last_x = it->second.pos_x;
            auto last_y = it->second.pos_y;

            update_position(it);
            update_player_status(it, last_x, last_y);

            if (number_of_players == 1)
            {
                end_game();
                break;
            }
        }
    }
    else
    {
        if (can_game_get_started())
        {
            create_players();
            events.clear();
            save_event(new_game_event(width, height, get_players_names()));

            for (auto it = players.begin(); it != players.end(); it++)
            {
                if (is_on_board(it->second.pos_x, it->second.pos_y) && !is_eaten(it->second.pos_x, it->second.pos_y))
                {
                    set_eaten(it->second.pos_x, it->second.pos_y);
                    save_event(pixel_event((int)it->second.pos_x, (int)it->second.pos_y, it->second.player_number));
                }
                else
                {
                    it->second.is_alive = false;
                    number_of_players -= 1;
                    save_event(player_eliminated_event(it->second.player_number));
                }

                if (number_of_players == 1)
                {
                    end_game();
                    break;
                }
            }
        }
    }
}

void do_epoll(int epollfd)
{
    std::array<struct epoll_event, MAX_EVENTS> events;

    while (true)
    {
        auto n = epoll_wait(epollfd, events.data(), MAX_EVENTS, -1);

        for (int i = 0; i < n; ++i)
        {
            if (events[i].events & EPOLLERR ||
                events[i].events & EPOLLHUP ||
                !(events[i].events & EPOLLIN))
            {
                close(events[i].data.fd);
            }
            else if (UDP_SCKT == events[i].data.fd)
            {
                receive_message();
            }
            else if (TIMER_SCKT == events[i].data.fd)
            {
                char buf[10];
                auto len = read(TIMER_SCKT, buf, 10);
                len++; //no unused warning;
                update_server();
            }
        }
    }
}

void save_event(std::vector<uint8_t> _event)
{
    events.push_back(_event);
    send_events(events.size() - 1, events.size() - 1);
}
struct epoll_event udp_event;
struct epoll_event timer_event;

int main(int argc, char *argv[])
{
    std::tie(server_port, turning_speed, rounds_per_sec, width, height, seed, seed_specified) = get_arguments(argc, argv);

    nanoseconds_per_round = 1000000000 / rounds_per_sec;

    int epollfd = epoll_create1(0);
    UDP_SCKT = setup_connection(server_port);
    TIMER_SCKT = setup_timer();
    change_time(TIMER_SCKT, nanoseconds_per_round);

    udp_event.data.fd = UDP_SCKT;
    udp_event.events = EPOLLIN;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, udp_event.data.fd, &udp_event) == -1)
        if (epollfd == -1)
            fatal("epoll_ctl failed.");

    timer_event.data.fd = TIMER_SCKT;
    timer_event.events = EPOLLIN;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, timer_event.data.fd, &timer_event) == -1)
        if (epollfd == -1)
            fatal("epoll_ctl failed.");

    board = new bool *[width];

    for (size_t i = 0; i < width; ++i)
        board[i] = new bool[height];

    for (size_t i = 0; i < width; ++i)
    {
        for (size_t j = 0; j < height; ++j)
        {
            board[i][j] = 0;
        }
    }

    do_epoll(epollfd);
}
