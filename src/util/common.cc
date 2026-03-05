#include "util/common.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstdio>
#include <cstdlib>

std::string gen_random()
{
    std::string::size_type length = 20;
    static auto& chrs = "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    thread_local static std::mt19937 rg{std::random_device{}()};
    thread_local static std::uniform_int_distribution<std::string::size_type> pick(0, sizeof(chrs) - 2);

    std::string s;

    s.reserve(length);

    while(length--)
        s += chrs[pick(rg)];

    return s;
}

void hexchar(unsigned char c, unsigned char &hex1, unsigned char &hex2)
{
    hex1 = c / 16;
    hex2 = c % 16;
    hex1 += hex1 <= 9 ? '0' : 'a' - 10;
    hex2 += hex2 <= 9 ? '0' : 'a' - 10;
}

std::string url_encode(const std::string &s) {
    const char *str = s.c_str();
    std::vector<char> v(s.size());
    v.clear();
    for (size_t i = 0, l = s.size(); i < l; i++)
    {
        char c = str[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
            c == '*' || c == '\'' || c == '(' || c == ')')
        {
            v.push_back(c);
        }
        else
        {
            v.push_back('%');
            unsigned char d1, d2;
            hexchar(c, d1, d2);
            v.push_back(d1);
            v.push_back(d2);
        }
    }
    return std::string(v.cbegin(), v.cend());
}

std::string url_decode(const std::string &SRC) {
    std::string ret;
    char ch;
    int ii;
    for (size_t i = 0; i < SRC.length(); i++) {
        if (int(SRC[i])==37) {
            std::sscanf(SRC.substr(i+1,2).c_str(), "%x", &ii);
            ch=static_cast<char>(ii);
            ret+=ch;
            i=i+2;
        } else {
            ret+=SRC[i];
        }
    }
    return (ret);
}

std::string html_encode(const std::string& data) {
    std::string buffer;
    buffer.reserve(data.size());
    for(size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    return buffer;
}

static std::map<std::string, std::string> text_resource;
static std::string empty{};

std::string resolve_resource_path(const std::string& filename) {
    const std::string default_prefix = "./resource/";
    const char* env_dir = std::getenv("HIVEMIND_RESOURCE");
    if (env_dir != nullptr) {
        std::string candidate = std::string(env_dir);
        if (!candidate.empty() && candidate.back() != '/') {
            candidate += '/';
        }
        candidate += filename;

        std::ifstream f(candidate);
        if (f.good()) {
            return candidate;
        }
    }
    return default_prefix + filename;
}

void load_text_resource(std::string filename, std::string key) {
    std::ifstream t(resolve_resource_path(filename));
    std::stringstream buffer;
    buffer << t.rdbuf();
    text_resource[key] = buffer.str();
}

const std::string &get_text_resource(std::string key) {
    if (text_resource.find(key) == text_resource.end()) {
        return empty;
    }
    return text_resource[key];
}

void str_replace(std::string& str, const std::string& from, const std::string& to) {
    while (true) {
        size_t start_pos = str.find(from);
        if(start_pos == std::string::npos) {
            return;
        }
        str.replace(start_pos, from.length(), to);
    }
}
