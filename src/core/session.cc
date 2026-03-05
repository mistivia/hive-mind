#include "core/session.h"

#include <algorithm>
#include <thread>

static std::map<std::string, Arc<Session>> session_map;
static std::mutex session_map_lock;
static std::once_flag session_cleanup_thread_once;

std::string to_string(SessionState state) {
    switch(state) {
        case SessionState::pending: return "未开始";
        case SessionState::ongoing: return "进行中";
        default: return "已经结束";
    }
    return "";
};

void Session::ensure_cleanup_thread_started() {
    std::call_once(session_cleanup_thread_once, []() {
        std::thread([]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::minutes(15));
                Session::cleanup_expired_sessions();
            }
        }).detach();
    });
}

void Session::cleanup_expired_sessions() {
    ulock _l{session_map_lock};
    auto now = std::chrono::system_clock::now();
    for (auto it = session_map.begin(); it != session_map.end();) {
        if (now - it->second->created_at > std::chrono::hours(24)) {
            it = session_map.erase(it);
        } else {
            ++it;
        }
    }
}

Arc<Session> Session::create_session(Arc<User> creator, std::vector<Arc<Card>> &card_list, int player_num) {
    ensure_cleanup_thread_started();
    ulock _l{session_map_lock};

    if(player_num < 1) {
        throw std::runtime_error{"play num must > 1"};
    }
    if (card_list.size() < static_cast<size_t>(PACK_NUM * CARD_NUM_PER_PACK * player_num)) {
        throw std::runtime_error{"insufficient cards"};
    }

    auto session = make_shared<Session>();
    session->player_num = player_num;
    session->session_id = gen_random();
    session->created_at = std::chrono::system_clock::now();
    
    auto user_state = make_shared<UserState>();
    user_state->is_creator = true;
    user_state->user = creator;
    user_state->user->nick = "房主";
    user_state->time_last_updated = std::chrono::system_clock::now();
    session->users.push_back(user_state);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(card_list.begin(), card_list.end(), g);
    for (auto &card : card_list){
        session->remained_cards.push(card);
    }

    session_map[session->session_id] = session;
    return session;
}

Arc<Session> Session::get_session(const std::string &session_id) {
    ensure_cleanup_thread_started();
    ulock _l{session_map_lock};

    if (session_map.find(session_id) == session_map.end()) {
        throw std::runtime_error{"cannot find session"};
    }
    return session_map[session_id];
}

void Session::validate_user(Arc<User> user) {
    for (auto &user_state : this->users) {
        if (user_state->user->user_id == user->user_id) {
            return;
        }
    }
    throw std::runtime_error{"user do not exist"};
}

void Session::validate_user_is_new(Arc<User> user) {
    for (auto &user_state : this->users) {
        if (user_state->user->user_id == user->user_id) {
            throw std::runtime_error{"user already exist"};
        }
    }
    return;
}

void Session::validate_creator(Arc<User> user) {
    for (auto &user_state : this->users) {
        if (user_state->user->user_id == user->user_id && user_state->is_creator) {
            return;
        }
    }
    throw std::runtime_error{"not creator"};
}

void Session::validate_session_state(SessionState state) {
    if (this->session_state != state) {
        throw std::runtime_error{"session is not valid"};
    }
    return;
}

void Session::add_user(Arc<User> user) {
    ulock _l{this->lock};
    validate_session_state(SessionState::pending);
    validate_user_is_new(user);
    if (user->nick.empty()) {
        throw std::runtime_error{"invalid nick"};
    }
    if (users.size() >= static_cast<size_t>(player_num)) {
        throw std::runtime_error{"room is full"};
    }

    auto user_state = make_shared<UserState>();
    user_state->user = user;
    user_state->time_last_updated = std::chrono::system_clock::now();
    users.push_back(user_state);

    get_creator()->time_last_updated = std::chrono::system_clock::now();
}

void Session::kick_user(Arc<User> kicking_user, Arc<User> kicked_user) {
    ulock _l{this->lock};
    validate_session_state(SessionState::pending);
    validate_creator(kicking_user);
    validate_user(kicked_user);

    for (auto it = users.begin(); it < users.end(); it++) {
        if ((*it)->user->user_id == kicked_user->user_id && !(*it)->is_creator) {
            users.erase(it);
        }
    }
}

void Session::start(Arc<User> starting_user) {
    ulock _l{this->lock};
    validate_session_state(SessionState::pending);
    validate_creator(starting_user);

    if (users.size() != static_cast<size_t>(player_num)) {
        throw std::runtime_error{"player num wrong"};
    }

    this->session_state = SessionState::ongoing;
    for (auto &user_state : users) {
        user_state->time_last_updated = std::chrono::system_clock::now();
    }
    sanitize_state();
}

void Session::select_card(Arc<User> user, Arc<Card> card) {
    ulock _l{this->lock};
    validate_session_state(SessionState::ongoing);
    validate_user(user);

    // find the user
    Arc<UserState> user_state;
    size_t user_idx;
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i]->user->user_id == user->user_id) {
            user_state = users[i];
            user_idx = i;
        }
    }
    if (user_state->card_pack_queue.empty()) {
        throw std::runtime_error{"no card to select"};
    }
    // find the card
    auto pack = user_state->card_pack_queue.front();
    for (auto it = pack->begin(); it < pack->end(); it++) {
        if ((*it)->name == card->name) {
            // get the card and transfer pack to next player
            user_state->selected_card.push_back(card);
            pack->erase(it);
            if (!pack->empty()) {
                int next_user_idx = -1;
                if (round_num % 2 == 0) {
                    next_user_idx = (user_idx + 1) % player_num;
                } else {
                    next_user_idx = (user_idx + player_num - 1) % player_num;
                }
                users[next_user_idx]->card_pack_queue.push(pack);
                if (users[next_user_idx]->card_pack_queue.size() == 1) {
                    users[next_user_idx]->time_last_updated = std::chrono::system_clock::now();
                }
            }
            user_state->card_pack_queue.pop();
            sanitize_state();
            return;
        }
    }
    throw std::runtime_error{"invalid card"};
}

int64_t Session::get_user_timestamp(Arc<User> user) {
    ulock _l{this->lock};
    validate_user(user);

    auto chrono_stamp = get_user_state(user)->time_last_updated;

    return std::chrono::duration_cast<std::chrono::milliseconds>(chrono_stamp.time_since_epoch()).count();
}

Arc<UserState> Session::get_user_state(Arc<User> user) {
    Arc<UserState> cur_user;
    for (auto &user_state : this->users) {
        if (user_state->user->user_id == user->user_id) {
            cur_user = user_state;
        }
    }
    return cur_user;
}

Arc<UserState> Session::get_creator() {
    Arc<UserState> cur_user;
    for (auto &user_state : this->users) {
        if (user_state->is_creator) {
            cur_user = user_state;
        }
    }
    return cur_user;
}


std::string Session::get_card_str(Arc<User> user) {
    ulock _l{this->lock};
    validate_user(user);

    auto cur_user = get_user_state(user);
    std::string card_str;
    for (auto &card : cur_user->selected_card) {
        card_str += card->name;
        card_str += "\n";
    }
    return card_str;
}

bool Session::need_new_pack() {
    for (auto &user_state : users) {
        if (!user_state->card_pack_queue.empty()) {
            return false;
        }
    }
    round_num++;
    if (round_num > PACK_NUM) {
        return false;
    }
    return true;
}

bool Session::should_finish() {
    for (auto &user_state : users) {
        if (!user_state->card_pack_queue.empty()) {
            return false;
        }
    }
    if (round_num >= PACK_NUM) {
        return true;
    }
    return false;
}

Arc<CardPack> create_pack(std::queue<Arc<Card>> &cards) {
    auto pack = make_shared<CardPack>();
    for (int i = 0; i < CARD_NUM_PER_PACK; i++) {
        pack->push_back(cards.front());
        cards.pop();
    }
    return pack;
} 

void Session::sanitize_state() {
    if (need_new_pack()) {
        for (auto &user_state : users) {
            user_state->card_pack_queue.push(create_pack(remained_cards));
            user_state->time_last_updated = std::chrono::system_clock::now();
        }
    }
    if (should_finish()) {
        for (auto &user_state : users) {
            user_state->time_last_updated = std::chrono::system_clock::now();
        }
        session_state = SessionState::finished;
    }
}

mstch::map card_view(Card &card, std::string &session_id) {
    auto card_info = mstch::map{
        {"sessionid", session_id},
        {"name", html_encode(card.name)},
        {"imgurl", card.image_url},
        {"nameurl", url_encode(card.name)}};
    std::string zhstext;
    if (!card.zhsname.empty()) {
        zhstext = "【" + card.zhsname + "】";
    }
    if (!card.zhstext.empty()) {
        zhstext += "\r";
        zhstext += card.zhstext;
    }
    zhstext = html_encode(zhstext);
    str_replace(zhstext, "\n", "<br>");
    str_replace(zhstext, "\r", "</p><p>");
    card_info["zhstext"] = zhstext;
    if (zhstext.empty()) {
        card_info["stab"] = std::string("<div></div>");
    }
    return card_info;
}

mstch::map Session::get_user_session_view(Arc<User> user) {
    ulock _l{this->lock};
    mstch::map view{
        {"download", mstch::array{
            mstch::map{{"sessionid", session_id}},
        }}
    };
    mstch::array players{};
    Arc<UserState> cur_user;
    for (auto &user_state : this->users) {
        if (user_state->user->user_id == user->user_id) {
            cur_user = user_state;
            if (user_state->is_creator && session_state == SessionState::pending) {
                view["start"] = mstch::array{mstch::map{{"sessionid", session_id}}};
            }
        }
        players.push_back(mstch::map{
            {"nick", user_state->user->nick},
            {"packnum", std::to_string(user_state->card_pack_queue.size())}});
    }
    view["players"] = players;
    view["state"] = to_string(session_state);
    if (cur_user == nullptr && session_state == SessionState::pending) {
        view["join"] = mstch::array{mstch::map{{"sessionid", session_id}}};
    }
    if (cur_user == nullptr) {
        return view;
    }

    mstch::array cardstopick{};
    mstch::array cardspicked{};
    if (!cur_user->card_pack_queue.empty()) {
        auto pack = cur_user->card_pack_queue.front();
        for (auto &card : *pack) {
            cardstopick.push_back(card_view(*card, session_id));
        }
    }
    if (!cur_user->selected_card.empty()) {
        for (auto &card : cur_user->selected_card) {
            cardspicked.push_back(card_view(*card, session_id));
        }
    }
    view["cardstopick"] = cardstopick;
    view["cardspicked"] = cardspicked;

    return view;
}