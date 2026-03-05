#ifndef HIVE_MIND_UTIL_COMMON_H_
#define HIVE_MIND_UTIL_COMMON_H_

#include <exception>
#include <memory>
#include <variant>
#include <vector>
#include <random>
#include <string>
#include <mutex>

template<typename T>
using Arc = std::shared_ptr<T>;

template<typename T>
using Box = std::unique_ptr<T>;

using ulock = std::unique_lock<std::mutex>;
using std::make_shared;
using std::make_unique;

struct Void{};

std::string gen_random();
std::string url_encode(const std::string &s);
std::string url_decode(const std::string &s);
std::string html_encode(const std::string& data);
std::string resolve_resource_path(const std::string& filename);

void load_text_resource(std::string filename, std::string key);
const std::string &get_text_resource(std::string key);
void str_replace(std::string& str, const std::string& from, const std::string& to);

#endif
