#ifndef _EVENTS_
#define _EVENTS_

#include <vector>
#include <string>
#include <stdint.h>

std::vector <uint8_t> new_game_event(int width, int height, std::vector <std::string> names);

std::vector <uint8_t> pixel_event(int x, int y, uint8_t player_number);

std::vector <uint8_t> player_eliminated_event(uint8_t player_number);

std::vector <uint8_t> game_over_event();

#endif
