/*
NEXT:
- test
- call code in a seq and push the result - rather than retaining?
  - no, that doesn't make sense for static ifs for example
- parens
- varargs
- overloading
- comments
*/

#include <cctype>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <istream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

using char_t = std::string::value_type;

namespace log {

static int fatal_level = 0;
static int error_level = 10;
static int warn_level = 20;
static int info_level = 30;
static int debug_level = 40;

static int output_level = error_level;
std::ostream &out = std::cout;

void setlevel(int l) {
  output_level = l;
}

struct levelout {
  int lev;
  std::function<void()> f;
};

struct end_t {} end;

levelout fatal{fatal_level, []() { std::exit(2); } };
levelout error{error_level, []() { std::exit(1); } };
levelout warn{warn_level, {}};
levelout info{info_level, {}};
levelout debug{debug_level, {}};

template <typename T>
levelout &
operator<<(levelout &lo, const T &x) {
  if (output_level >= lo.lev) {
    out << x;
  }
  return lo;
}

template <>
levelout &
operator<<(levelout &lo, const end_t &) {
  if (output_level >= lo.lev) {
    out << std::endl;
    if (lo.f) lo.f();
  }
  return lo;
}

}

template <typename K, typename V>
std::ostream &
operator<<(std::ostream &os, const std::map<K, V> &M) {
  os << "{ ";
  for (const auto &i : M) {
    os << i.first << ": " << i.second << ", ";
  }
  return os << "}";
}

template <typename T>
std::ostream &
operator<<(std::ostream &os, const std::vector<T> &V) {
  os << "[ ";
  for (const auto &i : V) {
    os << i << ", ";
  }
  return os << "]";
}


struct unit;

enum class literal {
  number,
  string,
  name
};

struct func {
  std::string name;
  int left_power;
  int right_power;
  std::vector<std::func<void(unit)>> 
  int prefix;
  int suffix;
  std::function<void(const unit *)> impl;
};

struct call {
  const func *f;
  std::vector<unit> args;

  call() = default;
  call(const call &) = default;
  call(call &&) = default;
  call &operator=(const call &) = default;
  call &operator=(call &&) = default;

  call(const func *f_, std::vector<unit> &&args_)
    : f{f_}
    , args{std::forward<std::vector<unit>>(args_)}
  { }

  void invoke() const {
    f->impl(args.data());
  }
};

struct code {
  std::map<std::string, unit> scope;
  std::vector<call> calls;

  void invoke() const {
    for (const auto &c : calls) {
      c.invoke();
    }
  }
};

struct name {
  std::string n;
};

using group = std::vector<unit>;

struct unit {
  using unit_type = std::variant<
    nullptr_t,
    int64_t,
    std::string,
    name,
    group,
    func,
    code
  >;
  unit_type u;

  unit() = default;
  unit(unit &&) = default;
  unit(const unit &) = default;
  unit &operator=(unit &&) = default;
  unit &operator=(const unit &) = default;

  unit(unit_type &&t)
    : u{std::forward<unit_type>(t)}
  { }

  bool is_nil() const { return u.index() == 0; }
};

std::ostream &
operator<<(std::ostream &os, literal u) {
  switch (u) {
  case literal::number: os << "number";
  case literal::string: os << "string";
  case literal::name: os << "name";
  }
  return os;
}

std::ostream &
operator<<(std::ostream &os, const func &f) {
  return os << f.prefix << "/" << f.name << "/" << f.suffix << "(" << f.left_power << "/" << f.right_power << ")";
}

std::ostream &
operator<<(std::ostream &os, const call &c) {
  return os << c.f->name << c.args;
}

std::ostream &
operator<<(std::ostream &os, const code &c) {
  return os << "{ scope: " << c.scope << ", seq: " << c.calls << " }";
}

std::ostream &
operator<<(std::ostream &os, const name &n) {
  return os << n.n;
}

std::ostream &
operator<<(std::ostream &os, const unit &u) {
  std::visit([&os](const auto &x) { os << x; }, u.u); 
  return os;
}

std::vector<unit> units;
std::vector<code *> scopes;

std::istream &in = std::cin;

static code &
curscope() { return *scopes.back(); }

static const unit *
getunit(const std::string &name) {
  for (auto i = scopes.rbegin(); i != scopes.rend(); ++i) {
    auto j = (*i)->scope.find(name);
    if (j != (*i)->scope.end()) {
      return &j->second;
    }
  }
  return nullptr;
}

static unit
read_string() {
  std::string s;
  bool in_escape = false;
  for (auto c = in.get(); in; c = in.get()) {
    if (in_escape) {
      in_escape = false;
      if (c == 'n') s += '\n';
      if (c == 't') s += '\t';
      if (c == '\\') s += '\\';
      if (c == '"') s += '"';
    }
    else if (c == '\\') {
      in_escape = true;
    }
    else if (c == '\"') {
      return unit{s};
    }
    else {
      s += c;
    }
  }
  if (in.eof()) {
    log::error << "EOF inside string" << log::end;
  }
  log::error << "I/O error inside string" << log::end;
  return unit{""}; // unreachable
}

static std::string
read_token(std::function<bool(char)> is_valid_char) {
  std::string s;
  for (char_t c = in.peek(); in; c = in.peek()) {
    if (!is_valid_char(c)) {
      if (s.empty()) {
        log::error << "expecting token" << log::end;
      }
      return s;
    }
    in.get();
    s += c;
  }
  if (!in) {
    log::error << "I/O error inside token" << log::end;
  }
  return s;
}

static bool
isnamechar(char_t c) {
  return std::isalnum(c) || c == '_';
}

static void seq(code &);

static unit
read_unit() {
  char_t c;
  while (in && std::isspace(c = in.peek())) {
    in.get();
  }
  if (c == std::istream::traits_type::eof()) {
    return unit{};
  }
  if (!in) {
    log::error << "I/O error" << log::end;
  }
  if (c == '"') {
    in.get();
    return read_string();
  }
  else if (std::isdigit(c)) {
    return unit{std::stoll(read_token(&isdigit))};
  }
  else if (isnamechar(c)) {
    return unit{name{read_token(&isnamechar)}};
  }
  else if (std::ispunct(c)) {
    return unit{name{read_token(&ispunct)}};
  }
  else {
    log::error << "invalid symbol" << log::end;
  }
  return unit{}; // unreachable
}

static void
builtin_stack(const unit *) {
  log::debug << units << log::end;
}

static void
builtin_scope(const unit *) {
  log::debug << curscope().scope << log::end;
}

static void
builtin_dump(const unit *args) {
  log::debug << args[0] << log::end;
}

static void
builtin_def(const unit *args) {
  const name *n = std::get_if<name>(&args->u);
  if (!n) {
    log::error << "def: arg 0 is not a name" << log::end;
  }
  if (getunit(n->n)) {
    log::error << "def: name '" << n->n << "' is already defined in this scope" << log::end;
  }
  curscope().scope[n->n] = args[1];
}

static void
builtin_invoke(const unit *args) {
  const code *c = std::get_if<code>(&args->u);
  if (!c) {
    log::error << "invoke: arg 0 is not code" << log::end;
  }
  c->invoke();
}

static void
builtin_fn(const unit *args) {
  const group *g = std::get_if<group>(&args[0].u);
  if (!g) {
    log::error << "invoke: arg 0 is not a group" << log::end;
  }
  const code *c = std::get_if<code>(&args[1].u);
  if (!c) {
    log::error << "invoke: arg 1 is not code" << log::end;
  }
   
}

static void getunit(int prec);

static void
callf(const func *f, std::vector<unit> &args, size_t at) {
  if (args.size() - at < f->suffix) {
    log::error << f->name << " requires " << f->suffix << " suffix args but " << (args.size() - at) << " available" << log::end;
  }
  auto i = args.begin() + at;
  log::debug << "trycall " << f->name << log::end;
  if (f->impl) {
    if (scopes.size() == 1) {
      log::debug << "call " << f->name << " " << std::vector<unit>{i - f->prefix, i + f->suffix} << log::end;
      f->impl(args.data() + at - f->prefix);
    }
    else {
      log::debug << "retain " << f->name << " " << std::vector<unit>{i - f->prefix, i + f->suffix} << log::end;
      curscope().calls.emplace_back(f, std::vector<unit>{i - f->prefix, i + f->suffix});
    }
  }
  args.erase(i - f->prefix, i + f->suffix);
}

static const unit *
resolve(const unit &in) {
  const unit *out = &in;
  const name *n = std::get_if<name>(&in.u);
  if (n) {
    out = getunit(n->n);
    if (!out) {
      out = &in;
    }
  }
  return out;
}

const static int min_power = std::numeric_limits<int>::min();
const static int max_power = std::numeric_limits<int>::max();

static const func basefunc = { "", min_power, min_power, 0, 0, nullptr };

static const func *
expr(const func *f, std::vector<unit> &args, size_t at) {
  log::debug << f->name << " " << args << " " << at << log::end;
  unit u = read_unit();
  if (u.is_nil()) {
    callf(f, args, at);
    return nullptr;
  }
  const unit *ru = resolve(u);
  const func *next = std::get_if<func>(&ru->u);
  if (next) {
    while (next && next->left_power > f->right_power) {
      log::debug << "resolving next " << next->name << log::end;
      if (args.size() - at < next->prefix) {
        log::error << next->name << " requires " << next->prefix << " prefix args but " << args.size() - at << " available" << log::end;
      }
      if (next->suffix == 0) {
        callf(next, args, args.size());
        return expr(f, args, at);
      }
      next = expr(next, args, args.size());
    }
    callf(f, args, at);
    log::debug << "ret " << next->name << log::end;
    return next;
  }
  else {
    log::debug << "push " << *ru << log::end;
    args.push_back(*ru);
    return expr(f, args, at);
  }
}

static void
seq(code &C) {
  scopes.emplace_back(&C);
  auto at = units.size();
  for (const func *f = &basefunc; f; f = expr(f, units, units.size()))
    std::cout << "loop";
    ;
  if (units.size() != at) {
    log::error << "unconsumed arguments " << std::vector<unit>{units.begin() + at, units.end()} << log::end;
  }
  scopes.pop_back();
}

static void builtin_start(const unit *);
static void builtin_paren(const unit *);

  code base = { {
    { "__stack",
      unit{func{"__stack", 0, 0, 0, 0, &builtin_stack}}
    },
    { "__scope",
      unit{func{"__scope", 0, 0, 0, 0, &builtin_scope}}
    },
    { "__dump",
      unit{func{"__dump", 0, 0, 0, 1, &builtin_dump}},
    },
    { "__def",
      unit{func{"__def", 0, 0, 0, 2, &builtin_def}}
    },
    { "__invoke",
      unit{func{"__invoke", 0, 0, 1, 0, &builtin_invoke}}
    },
    { ";",
      unit{func{";", min_power, max_power, 0, 0, nullptr}}
    },
    { ",",
      unit{func{",", min_power, max_power, 0, 0, nullptr}}
    },
    { "{",
      unit{func{"{", min_power + 1, max_power, 0, 0, &builtin_start}}
    },
    { "}",
      unit{func{"}", min_power, max_power, 0, 0, nullptr}}
    },
    { "(",
      unit{func{"(", max_power, max_power, 0, 0, &builtin_paren}}
    },
    { ")",
      unit{func{")", min_power, max_power, 0, 0, nullptr}}
    },
    { "fn",
      unit{func{"fn", 0, 0, 0, 2, &builtin_fn}}
    }
  }, {} };

static void
builtin_start(const unit *) {
  code C;
  scopes.emplace_back(&C);
  const func *end = std::get_if<func>(&base.scope["}"].u);
  auto at = units.size();
  const func *f;
  for (f = &basefunc; f && f != end; f = expr(f, units, units.size()))
    ;
  if (f != end) {
    log::error << "mismatched brace" << log::end;
  }
  if (units.size() != at) {
    log::error << "unconsumed arguments " << units << log::end;
  }
  scopes.pop_back();
  units.emplace_back(C);
}

static void
builtin_paren(const unit *) {
  const func *sep = std::get_if<func>(&base.scope[","].u);
  const func *end = std::get_if<func>(&base.scope[")"].u);
  size_t at = units.size();
  const func *f;
  bool is_group = false;
  for (f = &basefunc; f && f != end; f = expr(f, units, units.size())) {
    if (f == sep) {
      is_group = true;
    }
  }
  if (f != end) {
    log::error << "mismatched parens" << log::end;
  }
  if (is_group) {
    std::vector<unit> G{units.begin() + at, units.end()};
    units.erase(units.begin() + at, units.end());
    units.emplace_back(group{std::move(G)});
  }
}

int
main(int, char *[]) {
  log::setlevel(log::debug_level);
  seq(base);
  return 0;
}
