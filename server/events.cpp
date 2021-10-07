#include "events.h"

#define NEW_GAME_EVENT_TYPE 0
#define PIXEL_EVENT_TYPE 1
#define PLAYER_ELIMINATED_EVENT_TYPE 2
#define GAME_OVER_EVENT_TYPE 3
#define NAME_DELIMETER 0

std::vector <uint8_t> new_game_event(int width, int height, std::vector <std::string> names) {
    std::vector <uint8_t> to_save;

    to_save.push_back((uint8_t)(NEW_GAME_EVENT_TYPE));

    to_save.push_back((uint8_t)(width >> 24));
    to_save.push_back((uint8_t)(width >> 16));
    to_save.push_back((uint8_t)(width >> 8));
    to_save.push_back((uint8_t)(width));

    to_save.push_back((uint8_t)(height >> 24));
    to_save.push_back((uint8_t)(height >> 16));
    to_save.push_back((uint8_t)(height >> 8));
    to_save.push_back((uint8_t)(height));

    for (size_t i = 0; i != names.size(); i++)
    {
        for (const char &c : names[i])
        {
            to_save.push_back((uint8_t) c);
        }

        to_save.push_back(NAME_DELIMETER);
    }

    return to_save;
}

std::vector <uint8_t> pixel_event(int x, int y, uint8_t player_number) {
    std::vector <uint8_t> to_save;

    to_save.push_back((uint8_t)(PIXEL_EVENT_TYPE));
    to_save.push_back((uint8_t)(player_number));

    to_save.push_back((uint8_t)(x >> 24));
    to_save.push_back((uint8_t)(x >> 16));
    to_save.push_back((uint8_t)(x >> 8));
    to_save.push_back((uint8_t)(x));

    to_save.push_back((uint8_t)(y >> 24));
    to_save.push_back((uint8_t)(y >> 16));
    to_save.push_back((uint8_t)(y >> 8));
    to_save.push_back((uint8_t)(y));
    return to_save;
}

std::vector <uint8_t> player_eliminated_event(uint8_t player_number) {
    std::vector <uint8_t> to_save;

    to_save.push_back((uint8_t)(PLAYER_ELIMINATED_EVENT_TYPE));
    to_save.push_back((uint8_t)(player_number));

    return to_save;
}

std::vector <uint8_t> game_over_event() {
    std::vector <uint8_t> to_save;

    to_save.push_back((uint8_t)(GAME_OVER_EVENT_TYPE));
    return to_save;
}
