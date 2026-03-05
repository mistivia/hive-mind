#include "core/card.h"

#include <sstream>
#include <fstream>

#include "util/common.h"

static std::map<std::string, Card> card_db;

void init_card_db(){
    auto fs = std::ifstream{resolve_resource_path("mtgzhs")};
    std::string line;
    while(std::getline(fs, line, '\n')) {
        Card card;
        card.name = line;
        std::getline(fs, card.zhsname);
        std::getline(fs, card.zhstext);
        card_db[card.name] = card;
    }
}
 
Arc<Card> get_card(std::string name) {
    auto card = make_shared<Card>();
    card->name = name;
    card->image_url = "https://api.scryfall.com/cards/named?format=image&version=normal&exact=" + url_encode(name);
    if (card_db.find(name) != card_db.end()) {
        card->zhsname = card_db[name].zhsname;
        // fix scryfall
        str_replace(card->zhsname, "：", " ");
        card->zhstext = card_db[name].zhstext;
        card->image_url = "https://api.scryfall.com/cards/named?format=image&version=normal&fuzzy=" + url_encode(card->zhsname);
        str_replace(card->zhstext, "\001", "\n");
    }
    return card;
}

std::vector<Arc<Card>> parse_cardlist(const std::string &in_str) {
    std::string str;
    for (auto c : in_str) {
        if (c != '\r') {
            str += c;
        }
    }

    auto result = std::vector<Arc<Card>>{};
    auto ss = std::stringstream{str};

    for (std::string line; std::getline(ss, line, '\n');) {
        if (!line.empty()) {
            result.push_back(get_card(line));
        }
    }
    return result;
}