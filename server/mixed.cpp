#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include <poll.h>
#include "../common/errors.h"
#include "../common/texts.h"
#include <tuple>

std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool> get_arguments(int argc, char *argv[])
{
    uint32_t server_port = 2021;
    uint32_t turning_speed = 6;
    uint32_t rounds_per_sec = 50;
    uint32_t width = 640;
    uint32_t height = 480;
    uint32_t seed = 0;
    bool seed_specified = false;

    int opt;

    while ((opt = getopt(argc, argv, "p:s:t:v:w:h:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            printf("Game server port: %s\n", optarg);
            ensure_valid_number(optarg);
            server_port = std::stoul(optarg);

            if (server_port > 65536)
                fatal("invalid port.");

            break;

        case 's':
            printf("Seed: %s\n", optarg);
            ensure_valid_number(optarg);
            seed = std::stoul(optarg);
            seed_specified = true;
            break;

        case 't':
            printf("Turning speed: %s\n", optarg);
            ensure_valid_number(optarg);
            turning_speed = std::stoul(optarg);
            if (turning_speed < 1 || turning_speed > 90)
                fatal("invalid height.");
            break;

        case 'v':
            printf("Rounds per seconds: %s\n", optarg);
            ensure_valid_number(optarg);
            rounds_per_sec = std::stoul(optarg);
            if (rounds_per_sec < 1 || rounds_per_sec > 1000)
                fatal("invalid rounds_per_sec.");
            break;

        case 'w':
            printf("Gui server port: %s\n", optarg);
            ensure_valid_number(optarg);
            width = std::stoul(optarg);
            if (width < 16 || width > 4096)
                fatal("invalid width.");
            break;

        case 'h':
            printf("Gui server port: %s\n", optarg);
            ensure_valid_number(optarg);
            height = std::stoul(optarg);
            if (height < 16 || height > 4096)
                fatal("invalid height.");
            break;

        default:
            fatal("Usage: %s [-t nsecs] [-n] name\n", argv[0]);
        }
    }

    if (argc - optind != 0)
    {
        fatal("Bad args.");
    }

    return std::make_tuple(server_port, turning_speed, rounds_per_sec, width, height, seed, seed_specified);
}
