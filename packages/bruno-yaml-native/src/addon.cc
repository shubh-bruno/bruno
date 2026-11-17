#define RYML_SINGLE_HDR_DEFINE_NOW
#include "ryml.hpp"
#include <node_api.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

[[noreturn]] static void on_error_basic(ryml::csubstr msg, ryml::ErrorDataBasic const&, void*) {
  throw std::runtime_error(std::string(msg.str, msg.len));
}

[[noreturn]] static void on_error_parse(ryml::csubstr msg, ryml::ErrorDataParse const&, void*) {
  throw std::runtime_error(std::string(msg.str, msg.len));
}

[[noreturn]] static void on_error_visit(ryml::csubstr msg, ryml::ErrorDataVisit const&, void*) {
  throw std::runtime_error(std::string(msg.str, msg.len));
}

struct State {
  ryml::EventHandlerTree handler;
  ryml::Parser parser;
  ryml::Tree tree;
  std::string buf;
  State() : handler(ryml::get_callbacks()), parser(&handler), tree(ryml::get_callbacks()) {}
};

static thread_local std::unique_ptr<State> g_state;
static std::once_flag g_callbacks_once;

static State& state() {
  if (!g_state) g_state.reset(new State());
  return *g_state;
}

static inline bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

static bool equals(const char* s, size_t n, const char* lit) {
  size_t m = std::strlen(lit);
  return n == m && std::memcmp(s, lit, m) == 0;
}

static bool resolve_number(const char* s, size_t n, double* out) {
  if (n == 0) return false;
  size_t i = 0;
  bool neg = false;
  if (s[0] == '-' || s[0] == '+') {
    neg = s[0] == '-';
    i = 1;
    if (n == 1) return false;
  }
  if (s[i] == '.' && n - i == 4) {
    if (equals(s + i, 4, ".inf") || equals(s + i, 4, ".Inf") || equals(s + i, 4, ".INF")) {
      *out = neg ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
      return true;
    }
    if (i == 0 && (equals(s, 4, ".nan") || equals(s, 4, ".NaN") || equals(s, 4, ".NAN"))) {
      *out = std::numeric_limits<double>::quiet_NaN();
      return true;
    }
  }
  if (i == 0 && n > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'o')) {
    bool hex = s[1] == 'x';
    double v = 0;
    for (size_t k = 2; k < n; k++) {
      char c = s[k];
      int d;
      if (c >= '0' && c <= '9') d = c - '0';
      else if (hex && c >= 'a' && c <= 'f') d = c - 'a' + 10;
      else if (hex && c >= 'A' && c <= 'F') d = c - 'A' + 10;
      else return false;
      if (!hex && d > 7) return false;
      v = v * (hex ? 16 : 8) + d;
    }
    *out = v;
    return true;
  }
  size_t j = i;
  while (j < n && is_digit(s[j])) j++;
  size_t int_digits = j - i;
  size_t frac_digits = 0;
  if (j < n && s[j] == '.') {
    j++;
    size_t f = j;
    while (j < n && is_digit(s[j])) j++;
    frac_digits = j - f;
  }
  if (int_digits == 0 && frac_digits == 0) return false;
  if (j < n && (s[j] == 'e' || s[j] == 'E')) {
    j++;
    if (j < n && (s[j] == '+' || s[j] == '-')) j++;
    size_t e = j;
    while (j < n && is_digit(s[j])) j++;
    if (j == e) return false;
  }
  if (j != n) return false;
  if (n < 64) {
    char tmp[64];
    std::memcpy(tmp, s, n);
    tmp[n] = '\0';
    *out = std::strtod(tmp, nullptr);
  } else {
    std::string tmp(s, n);
    *out = std::strtod(tmp.c_str(), nullptr);
  }
  return true;
}

static napi_value make_string(napi_env env, ryml::csubstr s) {
  napi_value v;
  napi_create_string_utf8(env, s.str, s.len, &v);
  return v;
}

static napi_value make_plain(napi_env env, ryml::csubstr v) {
  const char* s = v.str;
  size_t n = v.len;
  napi_value out;
  if (n == 0 || (n == 1 && s[0] == '~') || equals(s, n, "null") || equals(s, n, "Null") || equals(s, n, "NULL")) {
    napi_get_null(env, &out);
    return out;
  }
  if (equals(s, n, "true") || equals(s, n, "True") || equals(s, n, "TRUE")) {
    napi_get_boolean(env, true, &out);
    return out;
  }
  if (equals(s, n, "false") || equals(s, n, "False") || equals(s, n, "FALSE")) {
    napi_get_boolean(env, false, &out);
    return out;
  }
  char c = s[0];
  if (is_digit(c) || c == '-' || c == '+' || c == '.') {
    double d;
    if (resolve_number(s, n, &d)) {
      napi_create_double(env, d, &out);
      return out;
    }
  }
  return make_string(env, v);
}

static napi_value convert(napi_env env, ryml::Tree const& t, ryml::id_type id) {
  napi_value out;
  if (t.is_map(id)) {
    napi_create_object(env, &out);
    for (ryml::id_type c = t.first_child(id); c != ryml::NONE; c = t.next_sibling(c)) {
      napi_set_property(env, out, make_string(env, t.key(c)), convert(env, t, c));
    }
    return out;
  }
  if (t.is_seq(id)) {
    napi_create_array_with_length(env, t.num_children(id), &out);
    uint32_t i = 0;
    for (ryml::id_type c = t.first_child(id); c != ryml::NONE; c = t.next_sibling(c)) {
      napi_set_element(env, out, i++, convert(env, t, c));
    }
    return out;
  }
  if (t.has_val(id)) {
    if (t.is_val_plain(id)) return make_plain(env, t.val(id));
    return make_string(env, t.val(id));
  }
  napi_get_null(env, &out);
  return out;
}

static napi_value Parse(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  napi_valuetype type;
  if (argc < 1 || napi_typeof(env, argv[0], &type) != napi_ok || type != napi_string) {
    napi_throw_type_error(env, nullptr, "parse: expected a string");
    return nullptr;
  }
  size_t len = 0;
  napi_get_value_string_utf8(env, argv[0], nullptr, 0, &len);
  State& s = state();
  s.buf.resize(len + 1);
  size_t written = 0;
  napi_get_value_string_utf8(env, argv[0], &s.buf[0], len + 1, &written);
  try {
    s.tree.clear();
    s.tree.clear_arena();
    ryml::parse_in_place(&s.parser, ryml::substr(&s.buf[0], written), &s.tree);
    if (s.tree.empty()) {
      napi_value n;
      napi_get_null(env, &n);
      return n;
    }
    ryml::id_type root = s.tree.root_id();
    if (s.tree.is_stream(root)) {
      root = s.tree.first_child(root);
      if (root == ryml::NONE) {
        napi_value n;
        napi_get_null(env, &n);
        return n;
      }
    }
    s.tree.resolve();
    return convert(env, s.tree, root);
  } catch (const std::exception& e) {
    napi_throw_error(env, nullptr, (std::string("YAML parse error: ") + e.what()).c_str());
    return nullptr;
  }
}

static napi_value Init(napi_env env, napi_value exports) {
  std::call_once(g_callbacks_once, []() {
    ryml::Callbacks cb;
    cb.set_error_basic(on_error_basic);
    cb.set_error_parse(on_error_parse);
    cb.set_error_visit(on_error_visit);
    ryml::set_callbacks(cb);
  });
  napi_value fn;
  napi_create_function(env, "parse", NAPI_AUTO_LENGTH, Parse, nullptr, &fn);
  napi_set_named_property(env, exports, "parse", fn);
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
