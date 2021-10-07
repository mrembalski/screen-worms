#include <iostream>
#include "../common/errors.h"

#define MIN_ASCII_VALUE 33
#define MAX_ASCII_VALUE 126


void ensure_valid_player_name(const std::string &name)
{
    if (!(name.length() >= 1 && name.length() <= 20))
    {
        fatal("Invalid player name length.");
    }

    for(const char& c : name) {
        if (!(((int) c) >= MIN_ASCII_VALUE && ((int) c) <= MAX_ASCII_VALUE))
            fatal("Invalid characters in name.");
    }
}

void ensure_valid_number(const std::string& s) {
    std::string::const_iterator it = s.begin();
    
    while (it != s.end() && std::isdigit(*it))
        ++it;

    if (!(!s.empty() && it == s.end() && s[0] != '0')) {
        fatal("Not a valid number.");
    }
}

bool is_valid_character(const char &c) {
    return ((((int) c) >= MIN_ASCII_VALUE && ((int) c) <= MAX_ASCII_VALUE));
}