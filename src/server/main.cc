#include <iostream>

#include <crow.h>

#include "core/session.h"
#include "util/common.h"

Arc<User> handler_cookie(crow::App<crow::CookieParser> &app, const crow::request &req) {
    auto& ctx = app.get_context<crow::CookieParser>(req);
    auto uid = ctx.get_cookie("uid");
    if (uid.empty()) {
        ctx.set_cookie("uid", gen_random() + ";Path=/");
    }
    return Arc<User>(new User{uid, ""});
}

crow::query_string get_parsed_param(const std::string &body) {
    return crow::query_string(std::string{"http://example.org?"} + body);
}

int main(int argc, char** argv)
{
    crow::App<crow::CookieParser> app;
    load_text_resource("index.html", "index");
    load_text_resource("session.html", "session");
    init_card_db();

    CROW_ROUTE(app, "/")([&](const crow::request& req){
        handler_cookie(app, req);
        return get_text_resource("index");
    });

    CROW_ROUTE(app, "/create-session/")
    .methods("POST"_method)
    ([&](const crow::request& req){
        auto user = handler_cookie(app, req);
        auto params = get_parsed_param(req.body);
        try {
            int player_num = std::stoi(params.get("playernum"));
            auto cardlist = parse_cardlist(params.get("cardlist"));
            auto session = Session::create_session(user, cardlist, player_num);
            CROW_LOG_INFO << "create session, "
                    << "playnum: "<< player_num
                    << ", cards: " << cardlist.size()
                    << ", sessionid: " << session->get_session_id();
            crow::response resp{302};
            resp.set_header("Location", std::string{"/session/"} + session->get_session_id());
            return resp;
        } catch (std::exception &e) {
            return crow::response{400, e.what()};
        }
        return crow::response{500};
    });

    CROW_ROUTE(app, "/session/<string>")([&](const crow::request& req, std::string session_id){
        auto user = handler_cookie(app, req);
        try {
            auto session = Session::get_session(session_id);
            auto view = session->get_user_session_view(user);
            auto tmplt = get_text_resource("session");
            return crow::response(mstch::render(tmplt, view));
        } catch (std::exception &e) {
            return crow::response{400, e.what()};
        }
        return crow::response{500};
    });

    CROW_ROUTE(app, "/start-session/<string>")
    .methods("POST"_method)
    ([&](const crow::request& req, std::string session_id){
        auto user = handler_cookie(app, req);
        try {
            auto session = Session::get_session(session_id);
            session->start(user);
            CROW_LOG_INFO << "start session, "
                    << "sessionid: " << session_id;
            crow::response resp{302};
            resp.set_header("Location", std::string{"/session/"} + session->get_session_id());
            return resp;
        } catch (std::exception &e) {
            return crow::response{400, e.what()};
        }
        return crow::response{500};
    });

    CROW_ROUTE(app, "/join/<string>")
    .methods("POST"_method)
    ([&](const crow::request& req, std::string session_id){
        auto user = handler_cookie(app, req);
        auto params = get_parsed_param(req.body);
        try {
            std::string nick = params.get("nickname");
            user->nick = nick;
            auto session = Session::get_session(session_id);
            session->add_user(user);
            CROW_LOG_INFO << "user join session"
                    << "|sessionid:" << session_id
                    << "|uid:" << user->user_id
                    << "|nick:" << user->nick;
            crow::response resp{302};
            resp.set_header("Location", std::string{"/session/"} + session->get_session_id());
            return resp;
        } catch (std::exception &e) {
            return crow::response{400, e.what()};
        }
        return crow::response{500};
    });

    CROW_ROUTE(app, "/select/<string>/<string>")
    .methods("POST"_method)
    ([&](const crow::request& req, std::string session_id, std::string card_name){
        auto user = handler_cookie(app, req);
        try {
            auto session = Session::get_session(session_id);
            card_name = url_decode(card_name);
            CROW_LOG_INFO << "select card"
                    << "|sessionid:" << session_id
                    << "|uid:" << user->user_id
                    << "|card:" << card_name;
            auto card = get_card(card_name);
            session->select_card(user, card);
            crow::response resp{302};
            resp.set_header("Location", std::string{"/session/"} + session->get_session_id());
            return resp;
        } catch (std::exception &e) {
            return crow::response{400, e.what()};
        }
        return crow::response{500};
    });

    CROW_ROUTE(app, "/download/<string>")
    ([&](const crow::request& req, std::string session_id){
        auto user = handler_cookie(app, req);
        try {
            auto session = Session::get_session(session_id);
            return crow::response{200, session->get_card_str(user)};
        } catch (std::exception &e) {
            return crow::response{400, e.what()};
        }
        return crow::response{500};
    });

    CROW_ROUTE(app, "/timestamp/<string>")
    ([&](const crow::request& req, std::string session_id){
        auto user = handler_cookie(app, req);
        try {
            auto session = Session::get_session(session_id);
            return crow::response{200, std::to_string(session->get_user_timestamp(user))};
        } catch (std::exception &e) {
            return crow::response{400, e.what()};
        }
        return crow::response{500};
    });

    int port = 18080;
    if (argc > 1) {
        port = std::stoi(std::string{argv[1]});
    }
    app.port(port).multithreaded().run();
}