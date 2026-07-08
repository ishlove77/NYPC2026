#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

constexpr int MAX_TURN = 200;         // maximum turn (days)
constexpr int START_GOLD = 500;       // initial gold
constexpr int START_WARRIORS = 3;     // initial warriors
constexpr int MOVE_COST = 10;         // move cost
constexpr int TRAIN_COST = 120;       // train cost
constexpr int WORK_INCOME = 15;       // income per warrior
constexpr int UPKEEP_PER_WARRIOR = 2; // upkeep per warrior
constexpr int HQ_MAX_LEVEL = 5;       // HQ max level
constexpr int BASE_MAX_LEVEL = 3;     // base max level
constexpr int HQ_HEAL_COST = 1000;    // HQ fix cost
constexpr int BASE_HEAL_COST = 500;   // base fix cost

struct HqLevelEntry {
  int upgrade_cost;
  int warrior_hp;
  int hp;
  int turret;
  int train_cap;
  int work_cap;
};

struct BaseLevelEntry {
  int cost;
  int hp;
  int turret;
  int work_cap;
};

constexpr HqLevelEntry HQ_LEVELS[HQ_MAX_LEVEL + 1] = {
    {0, 0, 0, 0, 0, 0},     {0, 4, 10, 1, 1, 1},    {600, 5, 15, 2, 1, 2},
    {1200, 6, 20, 2, 2, 3}, {2400, 7, 25, 3, 2, 4}, {3600, 8, 30, 3, 3, 5},
};
constexpr BaseLevelEntry BASE_LEVELS[BASE_MAX_LEVEL + 1] = {
    {0, 0, 0, 0},
    {300, 6, 1, 1},
    {600, 12, 1, 2},
    {1000, 18, 2, 3},
};

enum class Side : int { LEFT = 0, RIGHT = 1 };
enum class BType : int { HQ, BASE };
enum class WState : int { STATIONARY, MOVING };

inline Side opposite(Side s) {
  return s == Side::LEFT ? Side::RIGHT : Side::LEFT;
}
inline char side_char(Side s) { return s == Side::LEFT ? 'A' : 'B'; }
inline Side parse_side_char(char c) {
  return c == 'A' ? Side::LEFT : Side::RIGHT;
}

struct WarriorId {
  Side side = Side::LEFT;
  int num = 0;
  bool operator==(const WarriorId &o) const {
    return side == o.side && num == o.num;
  }
};

struct Warrior {
  WarriorId id;
  int region = 0;
  int hp = 0;
  WState state = WState::STATIONARY;
  int target = 0;
};

struct Building {
  int region = 0;
  Side side = Side::LEFT;
  BType type = BType::HQ;
  int level = 1;
  int hp = 10;

  int current_hp() const {
    return type == BType::HQ ? HQ_LEVELS[level].hp : BASE_LEVELS[level].hp;
  }
  int work_cap() const {
    return type == BType::HQ ? HQ_LEVELS[level].work_cap
                             : BASE_LEVELS[level].work_cap;
  }
};

struct GameMap {
  int N = 0, K = 0;
  std::vector<long long> x, y;
  std::vector<int> strongholds;
  std::vector<std::vector<int>> adj;

  Side my_side = Side::LEFT;
  int my_hq = 0;
  int opp_hq = 0;
};

struct GameState {
  int gold = START_GOLD; // current gold
  int my_countdown = 5;  // my remaining countdowns
  int opp_countdown = 5; // opponent's remaining countdowns
  std::vector<Warrior> warriors;
  std::vector<Building> buildings;
};

struct Actions {
  int train_n = 0;
  std::vector<std::pair<WarriorId, int>> moves;
  std::vector<int> upgrades;
};

static std::string readln() {
  std::string s;
  if (!std::getline(std::cin, s))
    std::exit(0);
  return s;
}

static std::vector<std::string> tokens(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream is(s);
  for (std::string t; is >> t;)
    out.push_back(t);
  return out;
}

static WarriorId parse_warrior(const std::string &tok) {
  assert(!tok.empty() && (tok[0] == 'A' || tok[0] == 'B'));
  WarriorId id;
  id.side = parse_side_char(tok[0]);
  id.num = std::stoi(tok.substr(1));
  return id;
}

static std::string format_warrior(WarriorId id) {
  std::string s;
  s.push_back(side_char(id.side));
  s += std::to_string(id.num);
  return s;
}

static int hq_of(const GameMap &M, Side s) {
  return (s == Side::LEFT) ? 0 : M.N - 1;
}

static Building make_base(int region, Side s) {
  return Building{region, s, BType::BASE, 1, BASE_LEVELS[1].hp};
}

static void apply_upgrade(Building &b) {
  b.level += 1;
  b.hp = b.current_hp();
}

static int upgrade_cost(const Building &b) {
  if (b.type == BType::HQ)
    return HQ_LEVELS[b.level + 1].upgrade_cost;
  else
    return BASE_LEVELS[b.level + 1].cost;
}

static int max_level(const Building &b) {
  return b.type == BType::HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL;
}

static void parse_init(GameMap &M, GameState &S) {
  {
    auto t = tokens(readln());
    assert(t.size() >= 2 && t[0] == "READY");
    M.my_side = (t[1] == "LEFT") ? Side::LEFT : Side::RIGHT;
  }
  {
    auto t = tokens(readln());
    M.N = std::stoi(t.at(0));
    M.K = std::stoi(t.at(1));
  }
  M.x.assign(M.N, 0);
  M.y.assign(M.N, 0);
  {
    auto t = tokens(readln()); // x_0 x_1 ... x_{N-1}
    for (int i = 0; i < M.N; ++i)
      M.x[i] = std::stoll(t.at(i));
  }
  {
    auto t = tokens(readln()); // y_0 y_1 ... y_{N-1}
    for (int i = 0; i < M.N; ++i)
      M.y[i] = std::stoll(t.at(i));
  }
  {
    auto t = tokens(readln()); // K strongholds
    M.strongholds.clear();
    M.strongholds.reserve(t.size());
    for (const auto &s : t)
      M.strongholds.push_back(std::stoi(s));
    std::sort(M.strongholds.begin(), M.strongholds.end());
  }
  M.adj.assign(M.N, {});
  for (int r = 0; r < M.N; ++r) {
    auto t = tokens(readln()); // deg n_1 n_2 ...
    int deg = std::stoi(t.at(0));
    auto &nb = M.adj[r];
    nb.reserve(deg);
    for (int j = 0; j < deg; ++j)
      nb.push_back(std::stoi(t.at(1 + j)));
    std::sort(nb.begin(), nb.end());
  }

  M.my_hq = hq_of(M, M.my_side);
  M.opp_hq = hq_of(M, opposite(M.my_side));

  S = GameState{};
  S.gold = START_GOLD;
  Side opp = opposite(M.my_side);
  for (int sfx = 1; sfx <= START_WARRIORS; ++sfx) {
    S.warriors.push_back(Warrior{.id = WarriorId{M.my_side, sfx},
                                 .region = M.my_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
    S.warriors.push_back(Warrior{.id = WarriorId{opp, sfx},
                                 .region = M.opp_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
  }
  S.buildings.push_back(Building{hq_of(M, Side::LEFT), Side::LEFT, BType::HQ, 1,
                                 HQ_LEVELS[1].hp});
  S.buildings.push_back(Building{hq_of(M, Side::RIGHT), Side::RIGHT, BType::HQ,
                                 1, HQ_LEVELS[1].hp});

  std::cout << "OK" << std::endl;
}

static bool read_turn_start(int &turn_index) {
  std::string line = readln();
  if (line == "FINISH")
    return false;
  auto t = tokens(line);
  assert(!t.empty() && t[0] == "START");
  turn_index = std::stoi(t.at(2));
  return true;
}

static Building *find_building(GameState &S, int region) {
  for (auto &b : S.buildings)
    if (b.region == region)
      return &b;
  return nullptr;
}

static Warrior *find_warrior(GameState &S, WarriorId id) {
  for (auto &w : S.warriors)
    if (w.id == id)
      return &w;
  return nullptr;
}

static void read_turn_result(GameState &S, const GameMap &M,
                             const Actions &submitted) {
  for (int region : submitted.upgrades) {
    Building *b = find_building(S, region);
    if (b == nullptr) {
      S.gold -= BASE_LEVELS[1].cost;
      S.buildings.push_back(make_base(region, M.my_side));
    } else {
      if (b->level >= max_level(*b)) {
        int cost = (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
        S.gold -= cost;
        b->hp = b->current_hp();
      } else {
        S.gold -= upgrade_cost(*b);
        apply_upgrade(*b);
      }
    }
  }

  for (const auto &[id, target] : submitted.moves) {
    Building *b = find_building(S, target);
    int cost = (b != nullptr && b->side == M.my_side) ? 0 : MOVE_COST;
    S.gold -= cost;
    if (Warrior *w = find_warrior(S, id)) {
      w->state = WState::MOVING;
      w->target = target;
    }
  }

  S.gold -= TRAIN_COST * submitted.train_n;

  {
    std::string line = readln();
    if (line == "FINISH")
      std::exit(0);
    auto t = tokens(line);
    assert(!t.empty() && t[0] == "TURN");
  }
  {
    auto t = tokens(readln());
    S.my_countdown = std::stoi(t.at(2));
    S.opp_countdown = std::stoi(t.at(4));
  }
  // UPGRADE
  {
    auto t = tokens(readln()); // "UPGRADE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); // "<A|B> <region>"
      Side s = parse_side_char(r.at(0)[0]);
      int region = std::stoi(r.at(1));
      Building *b = find_building(S, region);
      if (b == nullptr) {
        S.buildings.push_back(make_base(region, s));
      } else if (b->side != M.my_side) {
        if (b->level >= max_level(*b)) {
          b->hp = b->current_hp();
        } else {
          apply_upgrade(*b);
        }
      }
    }
  }
  // TRAIN
  {
    auto t = tokens(readln()); // "TRAIN N"
    int n = std::stoi(t.at(1));
    if (n > 0) {
      auto ids = tokens(readln());
      for (int i = 0; i < n; ++i) {
        WarriorId id = parse_warrior(ids.at(i));
        int hq_region = hq_of(M, id.side);
        Building *hq_b = find_building(S, hq_region);
        int hq_level = (hq_b != nullptr) ? hq_b->level : 1;
        S.warriors.push_back(Warrior{.id = id,
                                     .region = hq_region,
                                     .hp = HQ_LEVELS[hq_level].warrior_hp});
      }
    }
  }
  // MOVE
  {
    auto t = tokens(readln()); // "MOVE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      WarriorId id = parse_warrior(r.at(0));
      int region = std::stoi(r.at(1));
      if (Warrior *w = find_warrior(S, id)) {
        w->region = region;
        if (id.side == M.my_side && w->state == WState::MOVING &&
            w->region == w->target) {
          w->state = WState::STATIONARY;
        }
      }
    }
  }
  // DAMAGE
  {
    auto t = tokens(readln()); // "DAMAGE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      WarriorId id = parse_warrior(r.at(1));
      int damage = std::stoi(r.at(2));
      if (Warrior *w = find_warrior(S, id))
        w->hp -= damage;
    }
    S.warriors.erase(std::remove_if(S.warriors.begin(), S.warriors.end(),
                                    [](const Warrior &w) { return w.hp <= 0; }),
                     S.warriors.end());
  }
  // SIEGE
  {
    auto t = tokens(readln()); // "SIEGE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      int region = std::stoi(r.at(1));
      int damage = std::stoi(r.at(2));
      if (Building *b = find_building(S, region))
        b->hp -= damage;
    }
    S.buildings.erase(
        std::remove_if(S.buildings.begin(), S.buildings.end(),
                       [](const Building &b) { return b.hp <= 0; }),
        S.buildings.end());
  }
  (void)readln(); // "END"

  int income = 0;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side)
      continue;
    int count = 0;
    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side && w.region == b.region)
        ++count;
    }
    income += WORK_INCOME * std::min(count, b.work_cap());
  }
  S.gold += income;

  int alive = 0;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side)
      ++alive;
  S.gold = std::max(0, S.gold - UPKEEP_PER_WARRIOR * alive);
}

struct Paths {
  std::vector<std::vector<double>> dist;
  std::vector<std::vector<int>> nxt;
};

static double euclid_ceil(const GameMap &M, int u, int v) {
  double dx = (double)(M.x[u] - M.x[v]);
  double dy = (double)(M.y[u] - M.y[v]);
  return std::ceil(std::sqrt(dx * dx + dy * dy));
}

static Paths calculate_paths(const GameMap &M) {
  const double INF = std::numeric_limits<double>::infinity();
  Paths P;
  P.dist.assign(M.N, std::vector<double>(M.N, INF));
  P.nxt.assign(M.N, std::vector<int>(M.N, -1));

  for (int i = 0; i < M.N; ++i) {
    P.dist[i][i] = 0.0;
    P.nxt[i][i] = i;
  }
  for (int u = 0; u < M.N; ++u) {
    for (int v : M.adj[u]) {
      double w = euclid_ceil(M, u, v);
      if (w < P.dist[u][v])
        P.dist[u][v] = w;
    }
  }

  for (int k = 0; k < M.N; ++k) {
    for (int u = 0; u < M.N; ++u) {
      if (P.dist[u][k] == INF)
        continue;
      for (int v = 0; v < M.N; ++v) {
        double cand = P.dist[u][k] + P.dist[k][v];
        if (cand < P.dist[u][v])
          P.dist[u][v] = cand;
      }
    }
  }

  for (int u = 0; u < M.N; ++u) {
    for (int v = 0; v < M.N; ++v) {
      if (u == v || P.dist[u][v] == INF)
        continue;
      double best_score = INF;
      for (int nb : M.adj[u]) {
        if (P.dist[nb][v] == INF)
          continue;
        double score = euclid_ceil(M, u, nb) + P.dist[nb][v];
        if (score < best_score) {
          best_score = score;
          P.nxt[u][v] = nb;
        }
      }
    }
  }
  return P;
}

// Returns the next step on the path from u to v.
// If the path is not reachable, returns -1.
static int next_step(const Paths &P, int u, int v) { return P.nxt[u][v]; }

// Returns the path from u to v.
// If the path is not reachable, returns an empty vector.
static std::vector<int> path(const Paths &P, int u, int v) {
  std::vector<int> out;
  if (P.nxt[u][v] == -1)
    return out;
  out.push_back(u);
  while (u != v) {
    u = P.nxt[u][v];
    out.push_back(u);
  }
  return out;
}

static void emit_command() { std::cout << "COMMAND\n"; }

static void emit_actions(const Actions &a) {
  for (const auto &[id, target] : a.moves) {
    std::cout << "MOVE " << format_warrior(id) << ' ' << target << '\n';
  }
  for (int r : a.upgrades) {
    std::cout << "UPGRADE " << r << '\n';
  }
  if (a.train_n > 0) {
    std::cout << "TRAIN " << a.train_n << '\n';
  }
}

static void emit_end() { std::cout << "END" << std::endl; }

//////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
//////////////////////////////////
// Strategy version: timing_rush_v4

// Timing-rush parameters.
// These can be overridden at compile time, e.g.
//   g++ -std=c++20 -O2 -DRUSH_GROUP_SIZE=8 -DINITIAL_BASE_CAPTURE_COUNT=2 timing_rush_v4.cpp
//
// RUSH_GROUP_SIZE is the number of warriors actually sent to the opponent HQ.
// One stationary warrior is additionally protected at our HQ, so a rush of size
// RUSH_GROUP_SIZE requires at least RUSH_GROUP_SIZE + 1 stationary HQ warriors.
#ifndef RUSH_GROUP_SIZE
#define RUSH_GROUP_SIZE 5
#endif

#ifndef INITIAL_BASE_CAPTURE_COUNT
#define INITIAL_BASE_CAPTURE_COUNT 0
#endif

static_assert(RUSH_GROUP_SIZE >= 1, "RUSH_GROUP_SIZE must be positive");
static_assert(INITIAL_BASE_CAPTURE_COUNT >= 0,
              "INITIAL_BASE_CAPTURE_COUNT must be non-negative");

constexpr int kRushGroupSize = RUSH_GROUP_SIZE;
constexpr int kInitialBaseCaptureCount = INITIAL_BASE_CAPTURE_COUNT;

static const Building *find_building_const(const GameState &S, int region) {
  for (const auto &b : S.buildings)
    if (b.region == region)
      return &b;
  return nullptr;
}

static bool same_warrior(WarriorId a, WarriorId b) { return a == b; }

static bool contains_id(const std::vector<WarriorId> &ids, WarriorId id) {
  for (WarriorId x : ids)
    if (same_warrior(x, id))
      return true;
  return false;
}

static std::vector<WarriorId>
stationary_my_warriors_at(const GameState &S, const GameMap &M, int region,
                          const std::vector<WarriorId> &excluded = {}) {
  std::vector<WarriorId> out;
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side)
      continue;
    if (w.region != region || w.state != WState::STATIONARY)
      continue;
    if (contains_id(excluded, w.id))
      continue;
    out.push_back(w.id);
  }
  std::sort(out.begin(), out.end(), [](const WarriorId &a, const WarriorId &b) {
    if ((int)a.side != (int)b.side)
      return (int)a.side < (int)b.side;
    return a.num < b.num;
  });
  return out;
}

static int stationary_my_warrior_count_at(const GameState &S, const GameMap &M,
                                          int region) {
  int cnt = 0;
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side && w.region == region &&
        w.state == WState::STATIONARY) {
      ++cnt;
    }
  }
  return cnt;
}

static bool has_my_stationary_warrior_at(const GameState &S, const GameMap &M,
                                         int region) {
  return stationary_my_warrior_count_at(S, M, region) > 0;
}

static bool has_my_warrior_moving_to(const GameState &S, const GameMap &M,
                                     int region) {
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side && w.state == WState::MOVING &&
        w.target == region) {
      return true;
    }
  }
  return false;
}

static int command_move_cost(const GameState &S, const GameMap &M, int target) {
  const Building *b = find_building_const(S, target);
  return (b != nullptr && b->side == M.my_side) ? 0 : MOVE_COST;
}

static int my_hq_level(const GameState &S, const GameMap &M) {
  const Building *hq = find_building_const(S, M.my_hq);
  return hq == nullptr ? 1 : hq->level;
}

static int my_alive_warrior_count(const GameState &S, const GameMap &M) {
  int alive = 0;
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side)
      ++alive;
  }
  return alive;
}

static int expected_income_now(const GameState &S, const GameMap &M) {
  int income = 0;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side)
      continue;
    int cnt = 0;
    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side && w.region == b.region &&
          w.state == WState::STATIONARY) {
        ++cnt;
      }
    }
    income += WORK_INCOME * std::min(cnt, b.work_cap());
  }
  return income;
}

static std::vector<int> nearest_capture_targets(const GameMap &M,
                                                const Paths &P) {
  std::vector<int> candidates;
  for (int r : M.strongholds) {
    if (r == M.my_hq || r == M.opp_hq)
      continue;
    if (P.dist[M.my_hq][r] == std::numeric_limits<double>::infinity())
      continue;
    candidates.push_back(r);
  }
  std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
    if (P.dist[M.my_hq][a] != P.dist[M.my_hq][b])
      return P.dist[M.my_hq][a] < P.dist[M.my_hq][b];
    return a < b;
  });
  if ((int)candidates.size() > kInitialBaseCaptureCount)
    candidates.resize(kInitialBaseCaptureCount);
  return candidates;
}

static bool is_required_base_captured_and_garrisoned(const GameState &S,
                                                     const GameMap &M,
                                                     int region) {
  const Building *b = find_building_const(S, region);
  return b != nullptr && b->side == M.my_side &&
         has_my_stationary_warrior_at(S, M, region);
}

static bool required_bases_ready(const GameState &S, const GameMap &M,
                                 const Paths &P) {
  for (int r : nearest_capture_targets(M, P)) {
    if (!is_required_base_captured_and_garrisoned(S, M, r))
      return false;
  }
  return true;
}

static bool all_owned_buildings_garrisoned(const GameState &S,
                                           const GameMap &M) {
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side)
      continue;
    if (!has_my_stationary_warrior_at(S, M, b.region))
      return false;
  }
  return true;
}

// Pick one stationary worker at every owned building and protect it.  The
// protected IDs are excluded from every later MOVE-selection routine.  This is
// stronger than checking the count only at the end: the strategy never even
// selects the income workers as candidates for capture or rush.
static std::vector<WarriorId> protected_garrison_ids(const GameState &S,
                                                     const GameMap &M) {
  std::vector<int> owned_regions;
  owned_regions.push_back(M.my_hq); // protect HQ first.
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side || b.region == M.my_hq)
      continue;
    owned_regions.push_back(b.region);
  }
  std::sort(owned_regions.begin() + 1, owned_regions.end());

  std::vector<WarriorId> protected_ids;
  for (int region : owned_regions) {
    const Building *b = find_building_const(S, region);
    if (b == nullptr || b->side != M.my_side)
      continue;
    std::vector<WarriorId> here = stationary_my_warriors_at(S, M, region);
    if (!here.empty())
      protected_ids.push_back(here.front());
  }
  return protected_ids;
}

static bool warrior_is_stationary_at_region(const GameState &S, WarriorId id,
                                            int region) {
  for (const auto &w : S.warriors) {
    if (w.id == id)
      return w.region == region && w.state == WState::STATIONARY;
  }
  return false;
}

// Final safety guard.  It removes any MOVE that would consume the explicitly
// protected garrison, and it also enforces the count invariant for every owned
// building: at most (stationary workers at that building - 1) may leave.
static void enforce_owned_building_garrisons(Actions &a, const GameState &S,
                                             const GameMap &M) {
  std::vector<WarriorId> protected_ids = protected_garrison_ids(S, M);

  std::vector<int> owned_regions;
  for (const auto &b : S.buildings) {
    if (b.side == M.my_side)
      owned_regions.push_back(b.region);
  }
  std::sort(owned_regions.begin(), owned_regions.end());
  owned_regions.erase(std::unique(owned_regions.begin(), owned_regions.end()),
                      owned_regions.end());

  std::vector<int> allowance(owned_regions.size(), 0);
  for (int i = 0; i < (int)owned_regions.size(); ++i) {
    allowance[i] = std::max(0, stationary_my_warrior_count_at(S, M,
                                                              owned_regions[i]) -
                                   1);
  }

  std::vector<std::pair<WarriorId, int>> filtered;
  filtered.reserve(a.moves.size());

  for (const auto &[id, target] : a.moves) {
    if (contains_id(protected_ids, id))
      continue;

    int from_owned_idx = -1;
    for (int i = 0; i < (int)owned_regions.size(); ++i) {
      if (warrior_is_stationary_at_region(S, id, owned_regions[i]) &&
          target != owned_regions[i]) {
        from_owned_idx = i;
        break;
      }
    }

    if (from_owned_idx != -1) {
      if (allowance[from_owned_idx] <= 0)
        continue;
      --allowance[from_owned_idx];
    }
    filtered.push_back({id, target});
  }

  a.moves.swap(filtered);
}

static int turns_to_train_latest_first(int need, int train_cap) {
  if (need <= 0)
    return 0;
  if (train_cap <= 0)
    return std::numeric_limits<int>::max() / 4;
  return (need + train_cap - 1) / train_cap;
}

// If the earliest feasible departure is current turn + delay, this returns how
// many warriors should be trained now under the latest-possible training
// schedule.  Slots are filled backward from the turn immediately before the
// departure turn, minimizing extra upkeep before the rush.
static int train_now_for_latest_schedule(int need, int train_cap, int delay) {
  if (need <= 0 || train_cap <= 0 || delay <= 0)
    return 0;

  int remaining = need;
  std::vector<int> train(delay, 0);
  for (int j = delay - 1; j >= 0 && remaining > 0; --j) {
    int x = std::min(train_cap, remaining);
    train[j] = x;
    remaining -= x;
  }
  return train[0];
}

// Finds the earliest departure delay for one rush group.  ready_hq_stationary
// is the total number of stationary warriors currently at our HQ, including the
// one protected worker.  Therefore the requirement is group_size + 1.
static int earliest_rush_delay(const GameState &S, const GameMap &M, int budget,
                               int ready_hq_stationary, int group_size) {
  if (group_size <= 0)
    return -1;

  int required_hq_stationary = group_size + 1;
  int need_train = std::max(0, required_hq_stationary - ready_hq_stationary);
  int hq_level = my_hq_level(S, M);
  int train_cap = HQ_LEVELS[hq_level].train_cap;
  if (need_train > 0 && train_cap <= 0)
    return -1;

  int min_delay = turns_to_train_latest_first(need_train, train_cap);
  int income = expected_income_now(S, M);
  int base_alive = my_alive_warrior_count(S, M);
  int rush_move_cost = group_size * command_move_cost(S, M, M.opp_hq);

  for (int delay = min_delay; delay <= MAX_TURN + 20; ++delay) {
    std::vector<int> train(delay, 0);
    int remaining = need_train;
    for (int j = delay - 1; j >= 0 && remaining > 0; --j) {
      int x = std::min(train_cap, remaining);
      train[j] = x;
      remaining -= x;
    }
    if (remaining > 0)
      continue;

    int gold = budget;
    int extra_alive = 0;
    bool ok = true;
    for (int j = 0; j < delay; ++j) {
      int cost = train[j] * TRAIN_COST;
      if (gold < cost) {
        ok = false;
        break;
      }
      gold -= cost;
      extra_alive += train[j];
      gold += income;
      gold = std::max(0, gold - UPKEEP_PER_WARRIOR * (base_alive + extra_alive));
    }

    if (ok && gold >= rush_move_cost)
      return delay;
  }

  return -1;
}

static Actions decide(const GameState &S, const GameMap &M, const Paths &P,
                      int turn) {
  (void)turn;
  Actions a;
  int budget = S.gold;

  // This latch is the important v3 change.  The opening bases are checked only
  // until the first observed turn where every scheduled base is ours and has a
  // stationary worker.  After that, rush production/attacks do not re-check the
  // scheduled-base-capture condition.
  static bool opening_complete = false;

  // These IDs are never allowed to move this turn.  The HQ worker is included
  // here whenever one exists, so both capture and rush selection leave it home.
  std::vector<WarriorId> reserved = protected_garrison_ids(S, M);

  const std::vector<int> capture_targets = nearest_capture_targets(M, P);

  if (!opening_complete) {
    // v4 rule: if the opener was not complete at the start of this decision,
    // this turn may only perform opening work.  Even when the required bases are
    // already observed as captured/garrisoned now, we only latch the state and
    // wait until the next decision before training any rush army.
    if (required_bases_ready(S, M, P)) {
      opening_complete = true;
      enforce_owned_building_garrisons(a, S, M);
      return a;
    }

    // 1. Mandatory opening: capture the nearest target bases and keep one
    //    warrior stationed on each.  Rush-army training is blocked until the
    //    opener has latched complete on a previous turn.
    for (int target : capture_targets) {
      const Building *b = find_building_const(S, target);
      bool my_base = (b != nullptr && b->side == M.my_side);
      bool enemy_base = (b != nullptr && b->side != M.my_side);
      bool stationed = has_my_stationary_warrior_at(S, M, target);
      bool incoming = has_my_warrior_moving_to(S, M, target);

      if (my_base && stationed)
        continue;

      // A warrior is already standing on a neutral target: build the base.
      if (!my_base && !enemy_base && stationed) {
        if (budget >= BASE_LEVELS[1].cost) {
          a.upgrades.push_back(target);
          budget -= BASE_LEVELS[1].cost;
        }
        continue;
      }

      // This simple timing-rush opener does not retake enemy-owned strongholds.
      if (enemy_base)
        continue;

      // If we own the base but it lacks a worker, or if the neutral target still
      // needs a capturing worker, send exactly one non-protected HQ spare.
      if (!incoming) {
        std::vector<WarriorId> sendable =
            stationary_my_warriors_at(S, M, M.my_hq, reserved);
        if (!sendable.empty()) {
          int cost = command_move_cost(S, M, target);
          if (budget >= cost) {
            WarriorId id = sendable.front();
            a.moves.emplace_back(id, target);
            reserved.push_back(id);
            budget -= cost;
          }
        }
      }
    }

    // Opening-phase production cap.  While opening_complete is false, training
    // is allowed only to fill the exact opening workforce:
    //   one HQ garrison + one worker for each scheduled capture target.
    // No rush soldier may be trained in this phase.  The cap is applied after
    // deciding capture moves as well, so this branch can never issue TRAIN that
    // makes total own alive warriors exceed opening_required_workers.
    int opening_required_workers = 1 + (int)capture_targets.size();
    int current_alive = my_alive_warrior_count(S, M);
    int remaining_opening_slots =
        std::max(0, opening_required_workers - current_alive);

    bool still_has_unhandled_target = false;
    for (int target : capture_targets) {
      if (!is_required_base_captured_and_garrisoned(S, M, target) &&
          !has_my_warrior_moving_to(S, M, target)) {
        still_has_unhandled_target = true;
      }
    }

    int hq_level = my_hq_level(S, M);
    int train_cap = HQ_LEVELS[hq_level].train_cap;
    if (still_has_unhandled_target && train_cap > 0 && budget >= TRAIN_COST &&
        remaining_opening_slots > 0) {
      int affordable = budget / TRAIN_COST;
      a.train_n = std::min({train_cap, affordable, remaining_opening_slots});
      budget -= TRAIN_COST * a.train_n;
    }

    // Extra hard guard against accidental early production if this block is
    // edited later.
    if (current_alive + a.train_n > opening_required_workers) {
      a.train_n = std::max(0, opening_required_workers - current_alive);
    }

    enforce_owned_building_garrisons(a, S, M);
    return a;
  }

  // 2. Keep the income invariant: every currently owned building must have a
  //    stationary worker.  This is not the opening-base completion check; it is
  //    only the per-building garrison rule required for gold income.
  if (!all_owned_buildings_garrisoned(S, M)) {
    enforce_owned_building_garrisons(a, S, M);
    return a;
  }

  // 3. Direct timing rush from our HQ to the opponent HQ.  Only non-protected
  //    HQ warriors are candidates, so the HQ worker is structurally impossible
  //    to send.
  const int group_size = std::max(1, kRushGroupSize);
  std::vector<WarriorId> attackers =
      stationary_my_warriors_at(S, M, M.my_hq, reserved);
  int rush_cost_each = command_move_cost(S, M, M.opp_hq);

  int sendable_groups = (int)attackers.size() / group_size;
  int affordable_groups =
      rush_cost_each == 0 ? sendable_groups : budget / (group_size * rush_cost_each);
  int groups_to_send = std::min(sendable_groups, affordable_groups);

  if (groups_to_send > 0) {
    int send_count = groups_to_send * group_size;
    for (int i = 0; i < send_count; ++i) {
      a.moves.emplace_back(attackers[i], M.opp_hq);
      reserved.push_back(attackers[i]);
      budget -= rush_cost_each;
    }
    enforce_owned_building_garrisons(a, S, M);
    return a;
  }

  // 4. Just-in-time training for the next rush group.  This block is reachable
  //    only after opening_complete becomes true, so attack warriors are never
  //    trained before the scheduled bases have been captured and garrisoned.
  int ready_hq_stationary_total =
      stationary_my_warrior_count_at(S, M, M.my_hq);
  int delay = earliest_rush_delay(S, M, budget, ready_hq_stationary_total,
                                  group_size);
  if (delay > 0) {
    int need_train = std::max(0, group_size + 1 - ready_hq_stationary_total);
    int train_cap = HQ_LEVELS[my_hq_level(S, M)].train_cap;
    int train_now = train_now_for_latest_schedule(need_train, train_cap, delay);
    train_now = std::min(train_now, train_cap);
    train_now = std::min(train_now, budget / TRAIN_COST);
    a.train_n = std::max(0, train_now);
  }

  enforce_owned_building_garrisons(a, S, M);
  return a;
}

int main() {
  GameMap M;
  GameState S;
  parse_init(M, S);              // initialize the game
  Paths P = calculate_paths(M); // calculate the shortest paths

  int turn;
  while (read_turn_start(turn)) {
    Actions a = decide(S, M, P, turn);
    emit_command();
    emit_actions(a);
    emit_end();
    read_turn_result(S, M, a);
  }
  return 0;
}