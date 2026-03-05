#ifndef HIVE_MIND_CORE_SESSION_H_
#define HIVE_MIND_CORE_SESSION_H_

#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include <mstch/mstch.hpp>

#include "core/card.h"
#include "core/user.h"
#include "core/user_state.h"
#include "util/common.h"

enum class SessionState {
    pending, ongoing, finished
};

class Session {
  public:
    static Arc<Session> create_session(Arc<User> creator, std::vector<Arc<Card>> &card_list, int player_num);
    static Arc<Session> get_session(const std::string &session_id);

    void add_user(Arc<User> user);
    void kick_user(Arc<User> kicking_user, Arc<User> kicked_user);
    void select_card(Arc<User> user, Arc<Card> card);
    void start(Arc<User> starting_user);
    mstch::map get_user_session_view(Arc<User> user);
    std::string get_card_str(Arc<User> user);
    int64_t get_user_timestamp(Arc<User> user);
    const std::string &get_session_id() {return session_id;}

  private:
    Arc<UserState> get_user_state(Arc<User> user);
    Arc<UserState> get_creator();

    void sanitize_state();
    bool need_new_pack();
    bool should_finish();

    void validate_user(Arc<User> user);
    void validate_user_is_new(Arc<User> user);
    void validate_creator(Arc<User> user);
    void validate_session_state(SessionState state);

    static void ensure_cleanup_thread_started();
    static void cleanup_expired_sessions();

    std::string session_id;
    int player_num;
    SessionState session_state = SessionState::pending;
    std::vector<Arc<UserState>> users;
    std::queue<Arc<Card>> remained_cards;
    std::chrono::time_point<std::chrono::system_clock> created_at;
    int round_num = 0;
    std::mutex lock;
};

#endif
