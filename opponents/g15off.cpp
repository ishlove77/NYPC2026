/* species2b champion gen 15 (ladder run vs 260703_15: vsP2 0.660, vs260703_15 0.840, champScore 0.597). All 105 params baked. */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define MAYBE_UNUSED __attribute__((unused))
#else
#define MAYBE_UNUSED
#endif

enum {
  MAX_TURN = 200,         /* maximum turn (days) */
  START_GOLD = 500,       /* initial gold */
  START_WARRIORS = 3,     /* initial warriors */
  MOVE_COST = 10,         /* move cost */
  TRAIN_COST = 120,       /* train cost */
  WORK_INCOME = 15,       /* income per warrior */
  UPKEEP_PER_WARRIOR = 2, /* upkeep per warrior */
  HQ_MAX_LEVEL = 5,       /* HQ max level */
  BASE_MAX_LEVEL = 3,     /* base max level */
  HQ_HEAL_COST = 1000,    /* HQ fix cost */
  BASE_HEAL_COST = 500,   /* base fix cost */
};

typedef struct {
  int upgrade_cost;
  int warrior_hp;
  int hp;
  int turret;
  int train_cap;
  int work_cap;
} HqLevelEntry;

typedef struct {
  int cost;
  int hp;
  int turret;
  int work_cap;
} BaseLevelEntry;

static const HqLevelEntry HQ_LEVELS[HQ_MAX_LEVEL + 1] = {
    {0, 0, 0, 0, 0, 0},     {0, 4, 10, 1, 1, 1},    {600, 5, 15, 2, 1, 2},
    {1200, 6, 20, 2, 2, 3}, {2400, 7, 25, 3, 2, 4}, {3600, 8, 30, 3, 3, 5},
};
static const BaseLevelEntry BASE_LEVELS[BASE_MAX_LEVEL + 1] = {
    {0, 0, 0, 0},
    {300, 6, 1, 1},
    {600, 12, 1, 2},
    {1000, 18, 2, 3},
};

typedef enum { SIDE_LEFT = 0, SIDE_RIGHT = 1 } Side;
typedef enum { BTYPE_HQ = 0, BTYPE_BASE = 1 } BType;
typedef enum { WSTATE_STATIONARY = 0, WSTATE_MOVING = 1 } WState;

static Side opposite(Side s) { return s == SIDE_LEFT ? SIDE_RIGHT : SIDE_LEFT; }
static char side_char(Side s) { return s == SIDE_LEFT ? 'A' : 'B'; }
static Side parse_side_char(char c) { return c == 'A' ? SIDE_LEFT : SIDE_RIGHT; }

typedef struct {
  Side side;
  int num;
} WarriorId;

static int wid_eq(WarriorId a, WarriorId b) {
  return a.side == b.side && a.num == b.num;
}

typedef struct {
  WarriorId id;
  int region;
  int hp;
  WState state;
  int target;
} Warrior;

typedef struct {
  int region;
  Side side;
  BType type;
  int level;
  int hp;
} Building;

static int building_current_hp(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_LEVELS[b->level].hp : BASE_LEVELS[b->level].hp;
}
static int building_work_cap(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_LEVELS[b->level].work_cap
                             : BASE_LEVELS[b->level].work_cap;
}
static int building_max_level(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL;
}
static int building_upgrade_cost(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_LEVELS[b->level + 1].upgrade_cost
                             : BASE_LEVELS[b->level + 1].cost;
}
static void building_apply_upgrade(Building *b) {
  b->level += 1;
  b->hp = building_current_hp(b);
}

#define VEC_PUSH(vec, item)                                                    \
  do {                                                                         \
    if ((vec).len == (vec).cap) {                                              \
      (vec).cap = (vec).cap ? (vec).cap * 2 : 8;                               \
      (vec).data = (__typeof__((vec).data))realloc((vec).data, (size_t)(vec).cap * sizeof(*(vec).data)); \
    }                                                                          \
    (vec).data[(vec).len++] = (item);                                          \
  } while (0)

typedef struct {
  WarriorId id;
  int target;
} Move;

typedef struct {
  Warrior *data;
  int len, cap;
} WarriorVec;
typedef struct {
  Building *data;
  int len, cap;
} BuildingVec;
typedef struct {
  Move *data;
  int len, cap;
} MoveVec;
typedef struct {
  int *data;
  int len, cap;
} IntVec;

typedef struct {
  int N, K;
  long long *x, *y;
  IntVec strongholds;
  IntVec *adj;

  Side my_side;
  int my_hq;
  int opp_hq;
} GameMap;

typedef struct {
  int gold;
  int my_countdown;
  int opp_countdown;
  WarriorVec warriors;
  BuildingVec buildings;
} GameState;

typedef struct {
  int train_n;
  MoveVec moves;
  IntVec upgrades;
} Actions;

/* ---- input helpers ---- */

static char *readln(void) {
  static char *buf = NULL;
  static size_t cap = 0;
  ssize_t len = getline(&buf, &cap, stdin);
  if (len < 0)
    exit(0);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
    buf[--len] = '\0';
  return buf;
}

static char **tokens(const char *line, int *count) {
  static char *storage = NULL;
  static size_t storage_cap = 0;
  static char **items = NULL;
  static int items_cap = 0;

  size_t len = strlen(line) + 1;
  if (len > storage_cap) {
    storage_cap = len;
    storage = (char *)realloc(storage, storage_cap);
  }
  memcpy(storage, line, len);

  int n = 0;
  char *save = NULL;
  for (char *t = strtok_r(storage, " \t", &save); t;
       t = strtok_r(NULL, " \t", &save)) {
    if (n == items_cap) {
      items_cap = items_cap ? items_cap * 2 : 16;
      items = (char **)realloc(items, (size_t)items_cap * sizeof(char *));
    }
    items[n++] = t;
  }
  *count = n;
  return items;
}

static int cmp_int(const void *a, const void *b) {
  int x = *(const int *)a, y = *(const int *)b;
  return (x > y) - (x < y);
}

static WarriorId parse_warrior(const char *tok) {
  WarriorId id;
  id.side = parse_side_char(tok[0]);
  id.num = atoi(tok + 1);
  return id;
}

static int hq_of(const GameMap *M, Side s) {
  return (s == SIDE_LEFT) ? 0 : M->N - 1;
}

static Building make_base(int region, Side s) {
  Building b = {region, s, BTYPE_BASE, 1, BASE_LEVELS[1].hp};
  return b;
}

static Building *find_building(GameState *S, int region) {
  for (int i = 0; i < S->buildings.len; ++i)
    if (S->buildings.data[i].region == region)
      return &S->buildings.data[i];
  return NULL;
}

static Warrior *find_warrior(GameState *S, WarriorId id) {
  for (int i = 0; i < S->warriors.len; ++i)
    if (wid_eq(S->warriors.data[i].id, id))
      return &S->warriors.data[i];
  return NULL;
}

static void parse_init(GameMap *M, GameState *S) {
  memset(M, 0, sizeof(*M));
  memset(S, 0, sizeof(*S));

  {
    int n;
    char **t = tokens(readln(), &n);
    M->my_side = (strcmp(t[1], "LEFT") == 0) ? SIDE_LEFT : SIDE_RIGHT;
  }
  {
    int n;
    char **t = tokens(readln(), &n);
    M->N = atoi(t[0]);
    M->K = atoi(t[1]);
  }
  M->x = (long long *)malloc((size_t)M->N * sizeof(long long));
  M->y = (long long *)malloc((size_t)M->N * sizeof(long long));
  {
    int n;
    char **t = tokens(readln(), &n); /* x_0 x_1 ... x_{N-1} */
    for (int i = 0; i < M->N; ++i)
      M->x[i] = atoll(t[i]);
  }
  {
    int n;
    char **t = tokens(readln(), &n); /* y_0 y_1 ... y_{N-1} */
    for (int i = 0; i < M->N; ++i)
      M->y[i] = atoll(t[i]);
  }
  {
    int n;
    char **t = tokens(readln(), &n); /* K strongholds */
    for (int i = 0; i < n; ++i)
      VEC_PUSH(M->strongholds, atoi(t[i]));
    qsort(M->strongholds.data, (size_t)M->strongholds.len,
          sizeof(int), cmp_int);
  }
  M->adj = (IntVec *)calloc((size_t)M->N, sizeof(IntVec));
  for (int r = 0; r < M->N; ++r) {
    int n;
    char **t = tokens(readln(), &n); /* deg n_1 n_2 ... */
    int deg = atoi(t[0]);
    for (int j = 0; j < deg; ++j)
      VEC_PUSH(M->adj[r], atoi(t[1 + j]));
    qsort(M->adj[r].data, (size_t)M->adj[r].len, sizeof(int), cmp_int);
  }

  M->my_hq = hq_of(M, M->my_side);
  M->opp_hq = hq_of(M, opposite(M->my_side));

  S->gold = START_GOLD;
  S->my_countdown = 5;
  S->opp_countdown = 5;
  Side opp = opposite(M->my_side);
  for (int sfx = 1; sfx <= START_WARRIORS; ++sfx) {
    Warrior w1 = {{M->my_side, sfx}, M->my_hq, HQ_LEVELS[1].warrior_hp,
                  WSTATE_STATIONARY, 0};
    Warrior w2 = {{opp, sfx}, M->opp_hq, HQ_LEVELS[1].warrior_hp,
                  WSTATE_STATIONARY, 0};
    VEC_PUSH(S->warriors, w1);
    VEC_PUSH(S->warriors, w2);
  }
  Building hq_l = {hq_of(M, SIDE_LEFT), SIDE_LEFT, BTYPE_HQ, 1, HQ_LEVELS[1].hp};
  Building hq_r = {hq_of(M, SIDE_RIGHT), SIDE_RIGHT, BTYPE_HQ, 1,
                   HQ_LEVELS[1].hp};
  VEC_PUSH(S->buildings, hq_l);
  VEC_PUSH(S->buildings, hq_r);

  printf("OK\n");
  fflush(stdout);
}

static int read_turn_start(int *turn_index) {
  char *line = readln();
  if (strcmp(line, "FINISH") == 0)
    return 0;
  int n;
  char **t = tokens(line, &n);
  *turn_index = atoi(t[2]);
  return 1;
}

static void read_turn_result(GameState *S, const GameMap *M,
                             const Actions *submitted) {
  for (int i = 0; i < submitted->upgrades.len; ++i) {
    int region = submitted->upgrades.data[i];
    Building *b = find_building(S, region);
    if (b == NULL) {
      S->gold -= BASE_LEVELS[1].cost;
      Building nb = make_base(region, M->my_side);
      VEC_PUSH(S->buildings, nb);
    } else if (b->level >= building_max_level(b)) {
      S->gold -= (b->type == BTYPE_HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
      b->hp = building_current_hp(b);
    } else {
      S->gold -= building_upgrade_cost(b);
      building_apply_upgrade(b);
    }
  }

  for (int i = 0; i < submitted->moves.len; ++i) {
    Move mv = submitted->moves.data[i];
    Building *b = find_building(S, mv.target);
    int cost = (b != NULL && b->side == M->my_side) ? 0 : MOVE_COST;
    S->gold -= cost;
    Warrior *w = find_warrior(S, mv.id);
    if (w != NULL) {
      w->state = WSTATE_MOVING;
      w->target = mv.target;
    }
  }

  S->gold -= TRAIN_COST * submitted->train_n;

  {
    char *line = readln();
    if (strcmp(line, "FINISH") == 0)
      exit(0);
  }
  {
    int n;
    char **t = tokens(readln(), &n);
    S->my_countdown = atoi(t[2]);
    S->opp_countdown = atoi(t[4]);
  }
  /* UPGRADE */
  {
    int n;
    char **t = tokens(readln(), &n); /* "UPGRADE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m); /* "<A|B> <region>" */
      Side s = parse_side_char(r[0][0]);
      int region = atoi(r[1]);
      Building *b = find_building(S, region);
      if (b == NULL) {
        Building nb = make_base(region, s);
        VEC_PUSH(S->buildings, nb);
      } else if (b->side != M->my_side) {
        if (b->level >= building_max_level(b))
          b->hp = building_current_hp(b);
        else
          building_apply_upgrade(b);
      }
    }
  }
  /* TRAIN */
  {
    int n;
    char **t = tokens(readln(), &n); /* "TRAIN N" */
    int count = atoi(t[1]);
    if (count > 0) {
      int m;
      char **ids = tokens(readln(), &m);
      for (int i = 0; i < count; ++i) {
        WarriorId id = parse_warrior(ids[i]);
        int hq_region = hq_of(M, id.side);
        Building *hq_b = find_building(S, hq_region);
        int hq_level = (hq_b != NULL) ? hq_b->level : 1;
        Warrior w = {id, hq_region, HQ_LEVELS[hq_level].warrior_hp,
                     WSTATE_STATIONARY, 0};
        VEC_PUSH(S->warriors, w);
      }
    }
  }
  /* MOVE */
  {
    int n;
    char **t = tokens(readln(), &n); /* "MOVE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m);
      WarriorId id = parse_warrior(r[0]);
      int region = atoi(r[1]);
      Warrior *w = find_warrior(S, id);
      if (w != NULL) {
        w->region = region;
        if (id.side == M->my_side && w->state == WSTATE_MOVING &&
            w->region == w->target)
          w->state = WSTATE_STATIONARY;
      }
    }
  }
  /* DAMAGE */
  {
    int n;
    char **t = tokens(readln(), &n); /* "DAMAGE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m);
      WarriorId id = parse_warrior(r[1]);
      int damage = atoi(r[2]);
      Warrior *w = find_warrior(S, id);
      if (w != NULL)
        w->hp -= damage;
    }
    int kept = 0;
    for (int i = 0; i < S->warriors.len; ++i)
      if (S->warriors.data[i].hp > 0)
        S->warriors.data[kept++] = S->warriors.data[i];
    S->warriors.len = kept;
  }
  /* SIEGE */
  {
    int n;
    char **t = tokens(readln(), &n); /* "SIEGE N" */
    int count = atoi(t[1]);
    for (int i = 0; i < count; ++i) {
      int m;
      char **r = tokens(readln(), &m);
      int region = atoi(r[1]);
      int damage = atoi(r[2]);
      Building *b = find_building(S, region);
      if (b != NULL)
        b->hp -= damage;
    }
    int kept = 0;
    for (int i = 0; i < S->buildings.len; ++i)
      if (S->buildings.data[i].hp > 0)
        S->buildings.data[kept++] = S->buildings.data[i];
    S->buildings.len = kept;
  }
  (void)readln(); /* "END" */

  int income = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side)
      continue;
    int count = 0;
    for (int wi = 0; wi < S->warriors.len; ++wi) {
      const Warrior *w = &S->warriors.data[wi];
      if (w->id.side == M->my_side && w->region == b->region)
        ++count;
    }
    int cap = building_work_cap(b);
    income += WORK_INCOME * (count < cap ? count : cap);
  }
  S->gold += income;

  int alive = 0;
  for (int wi = 0; wi < S->warriors.len; ++wi)
    if (S->warriors.data[wi].id.side == M->my_side)
      ++alive;
  S->gold -= UPKEEP_PER_WARRIOR * alive;
  if (S->gold < 0)
    S->gold = 0;
}

typedef struct {
  int N;
  double **dist;
  int **nxt;
} Paths;

static double euclid_ceil(const GameMap *M, int u, int v) {
  double dx = (double)(M->x[u] - M->x[v]);
  double dy = (double)(M->y[u] - M->y[v]);
  return ceil(sqrt(dx * dx + dy * dy));
}

static Paths calculate_paths(const GameMap *M) {
  int N = M->N;
  Paths P;
  P.N = N;
  P.dist = (double **)malloc((size_t)N * sizeof(double *));
  P.nxt = (int **)malloc((size_t)N * sizeof(int *));
  for (int i = 0; i < N; ++i) {
    P.dist[i] = (double *)malloc((size_t)N * sizeof(double));
    P.nxt[i] = (int *)malloc((size_t)N * sizeof(int));
    for (int j = 0; j < N; ++j) {
      P.dist[i][j] = INFINITY;
      P.nxt[i][j] = -1;
    }
    P.dist[i][i] = 0.0;
    P.nxt[i][i] = i;
  }
  for (int u = 0; u < N; ++u) {
    for (int k = 0; k < M->adj[u].len; ++k) {
      int v = M->adj[u].data[k];
      double w = euclid_ceil(M, u, v);
      if (w < P.dist[u][v])
        P.dist[u][v] = w;
    }
  }

  for (int k = 0; k < N; ++k) {
    for (int u = 0; u < N; ++u) {
      if (isinf(P.dist[u][k]))
        continue;
      for (int v = 0; v < N; ++v) {
        double cand = P.dist[u][k] + P.dist[k][v];
        if (cand < P.dist[u][v])
          P.dist[u][v] = cand;
      }
    }
  }

  for (int u = 0; u < N; ++u) {
    for (int v = 0; v < N; ++v) {
      if (u == v || isinf(P.dist[u][v]))
        continue;
      double best_score = INFINITY;
      for (int k = 0; k < M->adj[u].len; ++k) {
        int nb = M->adj[u].data[k];
        if (isinf(P.dist[nb][v]))
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

/*
 * Returns the next step on the path from u to v.
 * If the path is not reachable, returns -1.
 */
static MAYBE_UNUSED int next_step(const Paths *P, int u, int v) { return P->nxt[u][v]; }

/*
 * Returns the path from u to v.
 * If the path is not reachable, returns an empty path.
 */
static MAYBE_UNUSED int path(const Paths *P, int u, int v, int *out) {
  if (P->nxt[u][v] == -1)
    return 0;
  int len = 0;
  out[len++] = u;
  while (u != v) {
    u = P->nxt[u][v];
    out[len++] = u;
  }
  return len;
}

static char *format_warrior(WarriorId id) {
  static char buf[16];
  snprintf(buf, sizeof(buf), "%c%d", side_char(id.side), id.num);
  return buf;
}

static void emit_actions(const Actions *a) {
  printf("COMMAND\n");
  for (int i = 0; i < a->moves.len; ++i)
    printf("MOVE %s %d\n", format_warrior(a->moves.data[i].id),
           a->moves.data[i].target);
  for (int i = 0; i < a->upgrades.len; ++i)
    printf("UPGRADE %d\n", a->upgrades.data[i]);
  if (a->train_n > 0)
    printf("TRAIN %d\n", a->train_n);
  printf("END\n");
  fflush(stdout);
}

/*//////////////////////////////////////////////////////////////////////////////
//// STRATEGY AREA //////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////*/

/*
 * TUNING DASHBOARD
 *
 * The strategy below is intentionally kept in one translation unit for contest
 * submission, but its knobs are grouped here by phase.  Every knob is guarded by
 * #ifndef, so experiments can override values without editing the file, e.g.
 *
 *   g++ -O2 -std=gnu++17 -DBASE_DUMMY_HP_DIV=2 jyprush_refactored.cpp
 *   g++ -O2 -std=gnu++17 -DTHIN_OPENING_MAX_TURN=65 -DRALLY_STACK_MIN_LAUNCH_UNITS=5 jyprush_refactored.cpp
 *
 * Suggested tuning order:
 *   1. Opening race: THIN_OPENING_* and OPENING_*
 *   2. Combat conservatism: BASE_DUMMY_HP_DIV, MIN_ATTACK_WAVE_UNITS, rally size
 *   3. Defense: DEFENSE_* and STACK_*
 *   4. Economy/endgame: HQ_SAVE_*, CASH_DUMP_*, FINAL_HQ5_*, ENDGAME_*
 *
 * The code path in decide() is phase ordered:
 *   endgame -> home defense -> stack cleanup -> thin opening -> opening neutral
 *   -> HQ/tiebreak saving -> neutral claiming -> rally attack -> upgrades
 *   -> worker redistribution -> builds/captures -> staging -> training.
 */

#ifndef BUILD_RESERVE
#define BUILD_RESERVE 236
#endif
#ifndef EXPANSION_MOVE_RESERVE
#define EXPANSION_MOVE_RESERVE 9
#endif
#ifndef ENABLE_ENEMY_BASE_CAPTURE
#define ENABLE_ENEMY_BASE_CAPTURE 1
#endif
#ifndef ENABLE_ENEMY_HQ_ATTACK
#define ENABLE_ENEMY_HQ_ATTACK 0
#endif


/* HP-ratio train priority only.
   No HQ-rush logic is added here.  If our total warrior HP is at or below
   HP_RATIO_TRAIN_PRIORITY_X_NUM / HP_RATIO_TRAIN_PRIORITY_X_DEN of the
   opponent's total warrior HP, training is issued before other gold-spending
   economy/upgrade/capture logic.  Default: 3/5 = 0.6. */
#ifndef ENABLE_HP_RATIO_TRAIN_PRIORITY
#define ENABLE_HP_RATIO_TRAIN_PRIORITY 0
#endif
#ifndef HP_RATIO_TRAIN_PRIORITY_X_NUM
#define HP_RATIO_TRAIN_PRIORITY_X_NUM 5
#endif
#ifndef HP_RATIO_TRAIN_PRIORITY_X_DEN
#define HP_RATIO_TRAIN_PRIORITY_X_DEN 2
#endif
#ifndef HP_RATIO_TRAIN_PRIORITY_MAX_TURN
#define HP_RATIO_TRAIN_PRIORITY_MAX_TURN 73
#endif
#ifndef HP_RATIO_TRAIN_PRIORITY_MIN_ENEMY_HP
#define HP_RATIO_TRAIN_PRIORITY_MIN_ENEMY_HP 9
#endif
#ifndef HP_RATIO_TRAIN_PRIORITY_MAX_PER_TURN
#define HP_RATIO_TRAIN_PRIORITY_MAX_PER_TURN 1
#endif
#ifndef HP_RATIO_TRAIN_PRIORITY_KEEP_SOLVENT
#define HP_RATIO_TRAIN_PRIORITY_KEEP_SOLVENT 1
#endif

/* During normal economy play, never drain a source building down to only its
   worker slots.  Keeping one extra local garrison prevents the expansion code
   from emptying a productive region too aggressively. */
#ifndef NORMAL_EXTRA_GARRISON
#define NORMAL_EXTRA_GARRISON 3
#endif


/* Opening neutral-first expansion is intentionally limited.  It grabs a small
   front slice of nearby empty strongholds before the normal v12 strategy starts
   competing for attacks/upgrades again.  This prevents the bot from spending
   the whole early game walking to every neutral point while early attack chances
   are available. */
#ifndef ENABLE_OPENING_NEUTRAL_FIRST
#define ENABLE_OPENING_NEUTRAL_FIRST 1
#endif
#ifndef OPENING_NEUTRAL_FIRST_MAX_TURN
#define OPENING_NEUTRAL_FIRST_MAX_TURN 60
#endif
#ifndef OPENING_NEUTRAL_FIRST_TARGET_PERCENT
#define OPENING_NEUTRAL_FIRST_TARGET_PERCENT 54
#endif
#ifndef OPENING_NEUTRAL_FIRST_MIN_TARGETS
#define OPENING_NEUTRAL_FIRST_MIN_TARGETS 6
#endif
#ifndef OPENING_NEUTRAL_FIRST_MAX_TARGETS
#define OPENING_NEUTRAL_FIRST_MAX_TARGETS 10
#endif

/* Faster opening expansion knobs.  While the opening-neutral quota is still
   active, allow the HQ to keep only its worker slot for a short time so the
   second initial warrior can also claim a nearby neutral.  Also delay the HQ
   upgrade/save lock so early gold becomes bases/warriors instead of sitting
   on HQ2 timing. */
#ifndef OPENING_RELAX_HQ_KEEP_UNTIL_TURN
#define OPENING_RELAX_HQ_KEEP_UNTIL_TURN 52
#endif
#ifndef OPENING_DELAY_HQ_UPGRADE_UNTIL_TURN
#define OPENING_DELAY_HQ_UPGRADE_UNTIL_TURN 21
#endif
#ifndef OPENING_DELAY_HQ_UPGRADE_MIN_HANDLED
#define OPENING_DELAY_HQ_UPGRADE_MIN_HANDLED 0
#endif
#ifndef OPENING_FORCE_TRAIN_IF_STUCK
#define OPENING_FORCE_TRAIN_IF_STUCK 1
#endif

/* All game: neutral empty strongholds are not attacks.  Send exactly one
   surplus warrior per empty stronghold, closest-to-HQ first, before enemy
   attacks. */
#ifndef ENABLE_ALLGAME_NEUTRAL_ONE_BY_ONE
#define ENABLE_ALLGAME_NEUTRAL_ONE_BY_ONE 1
#endif
/* When sending a warrior to an empty neutral stronghold, reserve the base build
   gold immediately.  This prevents the bot from occupying empty strongholds and
   then spending the construction money on upgrades/training before it can build. */
#ifndef RESERVE_BASE_GOLD_FOR_NEUTRAL_CLAIM
#define RESERVE_BASE_GOLD_FOR_NEUTRAL_CLAIM 0
#endif
#ifndef OPENING_FORCE_ONE_NEUTRAL_PER_TURN_UNTIL
#define OPENING_FORCE_ONE_NEUTRAL_PER_TURN_UNTIL 100
#endif
#ifndef OPENING_FORCE_TRAIN_NEUTRAL_UNTIL
#define OPENING_FORCE_TRAIN_NEUTRAL_UNTIL 30
#endif

/* v29 thin opening grab.  This is stricter than the older opening-neutral
   slice and is active only in the very early race for neutral strongholds.
   While active, owned HQ/bases keep exactly one stationary worker, no HQ/base
   level-up is issued, and empty neutral strongholds may be pre-occupied even
   when we cannot yet afford to build all of them. */
#ifndef ENABLE_THIN_OPENING_GRAB
#define ENABLE_THIN_OPENING_GRAB 1
#endif
#ifndef THIN_OPENING_MAX_TURN
#define THIN_OPENING_MAX_TURN 75
#endif
#ifndef THIN_OPENING_RUSH_SAFETY_TURNS
#define THIN_OPENING_RUSH_SAFETY_TURNS 1
#endif

static int g_current_turn = 0;
static int g_hq5_repair_reserve_budget_excluded = 0;
static int g_allow_base_upgrades_after_enemy_baseclear = 0;
static int g_opening_neutral_done = 0;
/* Hardcoded advantage-conversion state.  A single middle staging base is kept
   sticky so late attacks do not scatter across multiple anchors. */
static int g_advantage_stage_region = -1;
static int g_advantage_stage_target = -1;

/* Optional anchor-route attack state.  When ENABLE_ANCHOR_ROUTE_ATTACKS is on,
   the original capture logic still chooses the target and timing, but execution
   is routed through one owned anchor.  The anchor is sticky while alive; if it
   is lost, a new anchor is selected by ANCHOR_ROUTE_ANCHOR_MODE. */
static int g_anchor_route_stage_region = -1;
static int g_anchor_route_last_attack_anchor = -1;
static int g_anchor_route_last_attack_target = -1;

/* Keep the good v12 defensive timing as the main defense logic.  Opening
   neutral expansion is paused only after pressure is confirmed by enemy units
   actually moving toward our HQ, instead of by raw enemy army size. */
#ifndef OPENING_NEUTRAL_ABORT_ENEMY_ADVANTAGE
#define OPENING_NEUTRAL_ABORT_ENEMY_ADVANTAGE 3
#endif
#ifndef OPENING_NEUTRAL_ABORT_ENEMY_MASS_ETA
#define OPENING_NEUTRAL_ABORT_ENEMY_MASS_ETA 12
#endif
#ifndef OPENING_NEUTRAL_ABORT_ENEMY_MASS
#define OPENING_NEUTRAL_ABORT_ENEMY_MASS 9
#endif

/* Endgame all-in timing.  These are intentionally tunable compile-time knobs.
   If ENDGAME_SYNC_START_TURN <= ENDGAME_SYNC_ARRIVAL_TURN, stationary warriors
   are synced during that window so they try to arrive on ENDGAME_SYNC_ARRIVAL_TURN.
   If START > ARRIVAL, the synced endgame window is disabled.  Example tuning:
     -DENDGAME_SYNC_START_TURN=176 -DENDGAME_SYNC_ARRIVAL_TURN=196
   ENDGAME_ATTACK_TURNS and ENDGAME_GOLD_SAFETY remain independent knobs for
   legacy late direct-attack behavior. */
#ifndef ENDGAME_ATTACK_TURNS
#define ENDGAME_ATTACK_TURNS 17
#endif
#ifndef ENDGAME_GOLD_SAFETY
#define ENDGAME_GOLD_SAFETY 5
#endif
#ifndef ENDGAME_SYNC_START_TURN
#define ENDGAME_SYNC_START_TURN 170
#endif
#ifndef ENDGAME_SYNC_ARRIVAL_TURN
#define ENDGAME_SYNC_ARRIVAL_TURN 190
#endif

/* Normal economy acceleration.  A region keeps work_cap + NORMAL_EXTRA_GARRISON
   warriors; additional stationary surplus can be staged forward to a nearby
   owned base/HQ so future expansion starts closer to the frontier. */
#ifndef ENABLE_FORWARD_STAGING
#define ENABLE_FORWARD_STAGING 0
#endif
#ifndef FORWARD_STAGING_MAX_PER_SOURCE
#define FORWARD_STAGING_MAX_PER_SOURCE 5
#endif
#ifndef FORWARD_STAGING_EXTRA_CAP
#define FORWARD_STAGING_EXTRA_CAP 9
#endif

/* Home defense trigger.
   v6 no longer compares our army against every enemy on the map.  Only enemies
   that are moving closer to our HQ, or are already very near it, are counted as
   incoming threats.  Defense is ETA-based: if the number of defenders that can
   reach the HQ by time t is below incoming enemies by time t plus a safety
   margin, recall nearby stationary units and train at HQ before other spending. */
#ifndef ENABLE_HOME_DEFENSE
#define ENABLE_HOME_DEFENSE 1
#define ENABLE_UNDERFILLED_BUILDING_WORKER_SUPPORT 1
#define UNDERFILLED_WORKER_TRAIN_MAX_DEFICIT 66

#endif
#ifndef DEFENSE_SAFETY_MARGIN
#define DEFENSE_SAFETY_MARGIN 0
#endif
#ifndef DEFENSE_IMMEDIATE_RADIUS
#define DEFENSE_IMMEDIATE_RADIUS 1
#endif
#ifndef DEFENSE_HARD_ETA
#define DEFENSE_HARD_ETA 4
#endif
#ifndef DEFENSE_RECALL_EXTRA
#define DEFENSE_RECALL_EXTRA 8
#endif
#ifndef DEFENSE_MAX_TRACKED_ID
#define DEFENSE_MAX_TRACKED_ID 10000
#endif

/* A hard defense trigger means the opponent is already close enough that
   expansion/capture/staging is disabled for this turn after recall+training. */
#ifndef HARD_DEFENSE_SKIPS_ECONOMY_MOVES
#define HARD_DEFENSE_SKIPS_ECONOMY_MOVES 0
#endif


/* v21-stack-only patch: keep the original v21 economy/attack policy, but add
   only a global anti-drip stack guard and a synchronized cleanup wave for enemy
   stacks on HQ<->base supply paths. */
#ifndef ENABLE_STACK_ONLY_GUARD
#define ENABLE_STACK_ONLY_GUARD 1
#endif
#ifndef STACK_DANGER_COUNT
#define STACK_DANGER_COUNT 1
#endif
#ifndef STACK_LOOKAHEAD
#define STACK_LOOKAHEAD 14
#endif
#ifndef STACK_TARGET_DANGER_COUNT
#define STACK_TARGET_DANGER_COUNT 2
#endif
#ifndef ENABLE_STACK_ONLY_CLEANUP
#define ENABLE_STACK_ONLY_CLEANUP 0
#endif
#ifndef STACK_CLEANUP_MIN_ENEMY
#define STACK_CLEANUP_MIN_ENEMY 3
#endif
#ifndef STACK_CLEANUP_MAX_TARGET_ETA
#define STACK_CLEANUP_MAX_TARGET_ETA 8
#endif
#ifndef STACK_CLEANUP_RATIO_NUM
#define STACK_CLEANUP_RATIO_NUM 3
#endif
#ifndef STACK_CLEANUP_RATIO_DEN
#define STACK_CLEANUP_RATIO_DEN 1
#endif
#ifndef STACK_CLEANUP_EXTRA_MARGIN
#define STACK_CLEANUP_EXTRA_MARGIN 1
#endif
#ifndef STACK_CLEANUP_MIN_WAVE
#define STACK_CLEANUP_MIN_WAVE 3
#endif
#ifndef STACK_CLEANUP_MAX_SEND_EXTRA
#define STACK_CLEANUP_MAX_SEND_EXTRA 5
#endif
#ifndef MOVE_FLAG_ALLOW_CONTESTED_SOURCE
#define MOVE_FLAG_ALLOW_CONTESTED_SOURCE 1
#endif
#ifndef MOVE_FLAG_ALLOW_DANGER_TARGET
#define MOVE_FLAG_ALLOW_DANGER_TARGET 2
#endif
#ifndef MOVE_FLAG_IGNORE_STACK_GUARD
#define MOVE_FLAG_IGNORE_STACK_GUARD 4
#endif

static const Paths *g_stack_guard_paths = NULL;

#ifndef ENABLE_RETREAT_LOSING_FIGHTS
#define ENABLE_RETREAT_LOSING_FIGHTS 0
#endif
#ifndef ENABLE_RETREAT_FOLLOWUP_BLOCK
#define ENABLE_RETREAT_FOLLOWUP_BLOCK 1
#endif
#ifndef RETREAT_SIM_DAYS
#define RETREAT_SIM_DAYS 88
#endif
#ifndef RETREAT_MAX_UNITS
#define RETREAT_MAX_UNITS 256
#endif

static int g_retreat_bad_region[256];
static int g_retreat_safe_target[256];


/* v10/v13/v18 capture policy.
   New attacks are allowed only when no warrior is already committed to that
   target.  The attack wave must be synchronized: every warrior issued for a
   target today must have the same ETA to the target, and that same-ETA group
   must win the combat simulation by itself.  We intentionally do not count
   closer warriors in the simulation unless they are also sent in the same ETA
   wave, because they would enter the target earlier and get picked off.

   No-drip attack: once any warrior is already on or moving to a target, do not
   add one-by-one follow-up reinforcements.  If the first wave fails, the AI
   waits for a fresh opportunity instead of trickling more warriors into the
   same failed target. */
#ifndef ENABLE_INITIAL_SYNC_CAPTURE
#define ENABLE_INITIAL_SYNC_CAPTURE 1
#endif
#ifndef ENABLE_NO_DRIP_ATTACK
#define ENABLE_NO_DRIP_ATTACK 0
#endif
#ifndef MIN_ATTACK_WAVE_UNITS
#define MIN_ATTACK_WAVE_UNITS 4
#endif
#ifndef ENABLE_STRICT_SAME_ETA_ATTACK_WAVE
#define ENABLE_STRICT_SAME_ETA_ATTACK_WAVE 0
#endif
#ifndef ENABLE_TIMED_CAPTURE
#define ENABLE_TIMED_CAPTURE 0  /* legacy helper remains compiled but disabled */
#endif

/* v30 rally-stack attack: replace direct multi-source attacks with the
   winning-log pattern: first gather surplus warriors at one of our owned bases,
   then attack using only the stationary warriors that are physically stacked on
   that rally base.  This prevents scattered one-by-one infiltration and makes
   attacks look like the 1-42/1-43 winners: base rally -> single stack push. */
#ifndef ENABLE_RALLY_STACK_ATTACK
#define ENABLE_RALLY_STACK_ATTACK 0
#endif
#ifndef RALLY_STACK_STAGE_START_TURN
#define RALLY_STACK_STAGE_START_TURN 32
#endif
#ifndef RALLY_STACK_HQ_ATTACK_START_TURN
#define RALLY_STACK_HQ_ATTACK_START_TURN 141
#endif
#ifndef RALLY_STACK_MIN_LAUNCH_UNITS
#define RALLY_STACK_MIN_LAUNCH_UNITS 5
#endif
#ifndef RALLY_STACK_MAX_STAGE_MOVES_PER_TURN
#define RALLY_STACK_MAX_STAGE_MOVES_PER_TURN 28
#endif
#ifndef RALLY_STACK_KEEP_AT_RALLY
#define RALLY_STACK_KEEP_AT_RALLY 1
#endif


/* Optional anchor-routing for existing attacks only.
   This does NOT choose new targets or new attack timing.  Existing logic still
   decides when a target is capturable and which target to attack; when this is
   enabled, only the execution is changed: attackers first gather at one owned
   BASE anchor, then attack from that anchor only. */
#ifndef ENABLE_ANCHOR_ROUTE_ATTACKS
#define ENABLE_ANCHOR_ROUTE_ATTACKS 0
#endif
#ifndef ANCHOR_ROUTE_START_TURN
#define ANCHOR_ROUTE_START_TURN 0
#endif
/* Anchor choice for a target selected by the existing attack logic:
   0 = owned BASE closest to that selected target.
   1 = owned BASE closest to enemy HQ.
   2 = owned BASE closest to the middle between HQs.
   3 = owned BASE with best source-supply score plus target distance. */
#ifndef ANCHOR_ROUTE_ANCHOR_MODE
#define ANCHOR_ROUTE_ANCHOR_MODE 0
#endif
#ifndef ANCHOR_ROUTE_MAX_STAGE_PER_TURN
#define ANCHOR_ROUTE_MAX_STAGE_PER_TURN 8
#endif
#ifndef ANCHOR_ROUTE_KEEP_EXTRA_AT_SOURCE
#define ANCHOR_ROUTE_KEEP_EXTRA_AT_SOURCE 16
#endif
#ifndef ANCHOR_ROUTE_MAX_STACK
#define ANCHOR_ROUTE_MAX_STACK 18
#endif
#ifndef ANCHOR_ROUTE_LEAVE_AT_ANCHOR
#define ANCHOR_ROUTE_LEAVE_AT_ANCHOR 0
#endif
#ifndef ANCHOR_ROUTE_USE_ONLY_BASE_ANCHOR
#define ANCHOR_ROUTE_USE_ONLY_BASE_ANCHOR 1
#endif
/* Minimum attackers for anchor-routed attacks.  Existing capture logic still
   decides target/timing/win simulation, but the anchor execution never launches
   fewer than this many attackers. */
#ifndef ANCHOR_ROUTE_MIN_ATTACKERS
#define ANCHOR_ROUTE_MIN_ATTACKERS 26
#endif
/* Important: the actual anchor population needed before launch is
   ANCHOR_ROUTE_MIN_ATTACKERS plus the workers/garrison kept at the anchor.
   For the default MIN_ATTACKERS=4 and LEAVE_AT_ANCHOR=1, at least 5 total
   warriors must be committed to the anchor before staging stops. */
/* Do not start anchor-routed attacks until we own at least this many ordinary
   BASEs.  This is combined with the ax+b stronghold-count gate below. */
#ifndef ANCHOR_ROUTE_MIN_OWNED_BASES_TO_ATTACK
#define ANCHOR_ROUTE_MIN_OWNED_BASES_TO_ATTACK 0
#endif
/* Stronghold-count gate: if the map has x strongholds, require
   owned_BASE_count >= ceil(A_NUM/A_DEN * x) + B before anchor attacks begin.
   Default 0*x+0 keeps old behavior unless tuned. */
#ifndef ANCHOR_ROUTE_START_BASES_A_NUM
#define ANCHOR_ROUTE_START_BASES_A_NUM 1
#endif
#ifndef ANCHOR_ROUTE_START_BASES_A_DEN
#define ANCHOR_ROUTE_START_BASES_A_DEN 1
#endif
#ifndef ANCHOR_ROUTE_START_BASES_B
#define ANCHOR_ROUTE_START_BASES_B 1
#endif
#ifndef ANCHOR_ROUTE_STICKY_ANCHOR
#define ANCHOR_ROUTE_STICKY_ANCHOR 1
#endif
#ifndef ANCHOR_ROUTE_BUILD_CAPTURED_FIRST
#define ANCHOR_ROUTE_BUILD_CAPTURED_FIRST 1
#endif
#ifndef ANCHOR_ROUTE_RETURN_AFTER_CAPTURE
#define ANCHOR_ROUTE_RETURN_AFTER_CAPTURE 1
#endif
#ifndef ANCHOR_ROUTE_LEAVE_ON_CAPTURE
#define ANCHOR_ROUTE_LEAVE_ON_CAPTURE 5
#endif
/* If no target is currently capturable by the legacy attack check, still pick
   the best legacy-style enemy BASE target that has a valid anchor path and move
   surplus units to that anchor.  This fixes the "units pile up at HQ but never
   gather at anchor" failure mode; launching still requires the normal combat
   simulation to pass later. */
#ifndef ANCHOR_ROUTE_STAGE_WITHOUT_READY_TARGET
#define ANCHOR_ROUTE_STAGE_WITHOUT_READY_TARGET 0
#endif
/* When anchor routing is enabled, offensive attacks are allowed only from the
   selected anchor and only after at least ANCHOR_ROUTE_MIN_ATTACKERS are
   physically stacked there.  This blocks old one-by-one direct attacks from
   non-anchor regions while leaving neutral expansion/defense intact. */
#ifndef ANCHOR_ROUTE_STRICT_OFFENSE_ONLY
#define ANCHOR_ROUTE_STRICT_OFFENSE_ONLY 0
#endif

/* v9: HQ upgrades are saved for based on economy, not on fixed turn marks.
   If the next HQ upgrade can be afforded within this many turns using current
   net income, avoid lower-priority capital spending and bring a worker to HQ. */
#ifndef HQ_SAVE_LOOKAHEAD_TURNS
#define HQ_SAVE_LOOKAHEAD_TURNS 2
#endif

/* v9 combat simulation parameters.  Enemy bases get a zero-attack dummy body
   with HP equal to roughly one quarter of the visible defending side HP.  Enemy
   HQ simulations additionally let the opponent spend current income on HQ
   training every simulated combat day. */
#ifndef BASE_DUMMY_HP_DIV
#define BASE_DUMMY_HP_DIV 4
#endif
#ifndef MAX_COMBAT_SIM_UNITS
#define MAX_COMBAT_SIM_UNITS 256
#endif
#ifndef MAX_CAPTURE_SIM_DAYS
#define MAX_CAPTURE_SIM_DAYS 69
#endif
#ifndef HQ_SIM_MAX_REINFORCE_DAYS
#define HQ_SIM_MAX_REINFORCE_DAYS 66
#endif

/* v8: keep extra workers during defensive recall so the army does not starve
   immediately after all units are pulled home. */
#ifndef DEFENSE_KEEP_EXTRA_WORKERS
#define DEFENSE_KEEP_EXTRA_WORKERS 4
#endif

/* v12: late-game tiebreak preparation.  From this turn onward, if HQ5
   or HQ repair can be achieved before the turn limit using current net income,
   reserve gold for that action.  This matters because the game can be decided
   by HQ HP at the turn limit, and the v10 endgame all-in otherwise spends all
   spare gold on movement while skipping construction. */
#ifndef LATE_TIEBREAK_START_TURN
#define LATE_TIEBREAK_START_TURN 146
#endif
#ifndef LATE_TIEBREAK_SAVE_LOOKAHEAD_TURNS
#define LATE_TIEBREAK_SAVE_LOOKAHEAD_TURNS 25
#endif

/* v14 cash-use fixes: when the game reaches the cash-rich middle/late stage,
   unused gold is worse than extra bodies.  Once HQ tiebreak money is not being
   reserved, train up to the HQ cap instead of limiting training to missing
   worker slots. */
#ifndef CASH_DUMP_TRAIN_START_TURN
#define CASH_DUMP_TRAIN_START_TURN 116
#endif
#ifndef CASH_DUMP_MIN_GOLD
#define CASH_DUMP_MIN_GOLD 717
#endif

/* v21-stack-cashdump: once HQ5 is secured, the final resource objective is
   simple: keep enough gold to repair HQ on the last morning if needed, and
   keep enough food for the enlarged army.  Any remaining gold should become
   warriors, especially in turns 180..200 where unused gold no longer compounds. */
#ifndef FINAL_HQ5_CASH_DUMP_START_TURN
#define FINAL_HQ5_CASH_DUMP_START_TURN 162
#endif
#ifndef FINAL_HQ5_HARD_DUMP_START_TURN
#define FINAL_HQ5_HARD_DUMP_START_TURN 165
#endif
#ifndef FINAL_HQ5_REPAIR_RESERVE_GOLD
#define FINAL_HQ5_REPAIR_RESERVE_GOLD HQ_HEAL_COST
#endif
#ifndef FINAL_HQ5_MIN_TRAIN_GOLD
#define FINAL_HQ5_MIN_TRAIN_GOLD 169
#endif

/* v10: material-advantage HQ pressure was removed.  Enemy HQ attacks are still
   allowed, but only when the combat simulation says the attack can actually
   succeed against HQ training reinforcements. */

static int min_int(int a, int b) { return a < b ? a : b; }
static int max_int(int a, int b) { return a > b ? a : b; }

static const int INF_HOPS = 1000000000;

static int path_hops_between(const Paths *P, int u, int v) {
  if (u < 0 || v < 0 || u >= P->N || v >= P->N) return INF_HOPS;
  if (P->nxt[u][v] == -1) return INF_HOPS;
  if (u == v) return 0;
  int hops = 0;
  int cur = u;
  while (cur != v && hops <= P->N + 5) {
    cur = P->nxt[cur][v];
    if (cur < 0) return INF_HOPS;
    ++hops;
  }
  return cur == v ? hops : INF_HOPS;
}

static const Warrior *find_warrior_const(const GameState *S, WarriorId id) {
  for (int i = 0; i < S->warriors.len; ++i)
    if (wid_eq(S->warriors.data[i].id, id))
      return &S->warriors.data[i];
  return NULL;
}

static const Building *find_building_const(const GameState *S, int region) {
  for (int i = 0; i < S->buildings.len; ++i)
    if (S->buildings.data[i].region == region)
      return &S->buildings.data[i];
  return NULL;
}

static int is_stronghold(const GameMap *M, int region) {
  int l = 0, r = M->strongholds.len - 1;
  while (l <= r) {
    int m = (l + r) >> 1;
    int v = M->strongholds.data[m];
    if (v == region) return 1;
    if (v < region) l = m + 1;
    else r = m - 1;
  }
  return 0;
}

static int is_hq_region(const GameMap *M, int region) {
  return region == hq_of(M, SIDE_LEFT) || region == hq_of(M, SIDE_RIGHT);
}

static int is_move_destination_candidate(const GameMap *M, int region) {
  return is_hq_region(M, region) || is_stronghold(M, region);
}


/* Attack-path collision guard.
   When an attack orders units to an enemy building, the simulator moves them
   along the shortest path one hop at a time.  If another enemy building lies
   earlier on that path, combat starts there first.  Therefore an attack source
   is valid only when the first enemy building encountered on its path is the
   intended target itself.  This is deliberately used only by attack planners;
   economy/neutral movement remains unchanged. */
static int first_enemy_building_on_path(const GameState *S, const GameMap *M,
                                        const Paths *P, int from, int target) {
  if (P == NULL) return -1;
  if (from < 0 || target < 0 || from >= P->N || target >= P->N) return -1;
  if (P->nxt[from][target] == -1) return -1;

  Side enemy = opposite(M->my_side);
  int cur = from;
  int guard = 0;
  while (cur != target && guard++ <= P->N + 5) {
    cur = P->nxt[cur][target];
    if (cur < 0) return -1;
    const Building *b = find_building_const(S, cur);
    if (b != NULL && b->side == enemy)
      return cur;
  }
  return -1;
}

static int attack_path_first_enemy_is_target(const GameState *S,
                                             const GameMap *M,
                                             const Paths *P, int from,
                                             int target) {
  const Building *target_b = find_building_const(S, target);
  if (target_b == NULL || target_b->side == M->my_side) return 0;
  return first_enemy_building_on_path(S, M, P, from, target) == target;
}

static int action_has_upgrade(const Actions *a, int region) {
  for (int i = 0; i < a->upgrades.len; ++i)
    if (a->upgrades.data[i] == region)
      return 1;
  return 0;
}

static int action_has_move_warrior(const Actions *a, WarriorId id) {
  for (int i = 0; i < a->moves.len; ++i)
    if (wid_eq(a->moves.data[i].id, id))
      return 1;
  return 0;
}

static int planned_new_base(const GameState *S, const Actions *a, int region) {
  return find_building_const(S, region) == NULL && action_has_upgrade(a, region);
}

static int planned_my_building(const GameState *S, const GameMap *M,
                               const Actions *a, int region) {
  const Building *b = find_building_const(S, region);
  if (b != NULL) return b->side == M->my_side;
  return planned_new_base(S, a, region);
}

static int planned_building_level(const GameState *S, const Actions *a,
                                  int region) {
  const Building *b = find_building_const(S, region);
  if (b == NULL) return planned_new_base(S, a, region) ? 1 : 0;
  int lv = b->level;
  if (action_has_upgrade(a, region) && lv < building_max_level(b)) ++lv;
  return lv;
}

static int planned_work_cap_at(const GameState *S, const GameMap *M,
                               const Actions *a, int region) {
  if (!planned_my_building(S, M, a, region)) return 0;
  const Building *b = find_building_const(S, region);
  int lv = planned_building_level(S, a, region);
  if (b == NULL) return BASE_LEVELS[lv].work_cap;
  return b->type == BTYPE_HQ ? HQ_LEVELS[lv].work_cap
                             : BASE_LEVELS[lv].work_cap;
}

static int planned_train_cap(const GameState *S, const GameMap *M,
                             const Actions *a) {
  int hq = M->my_hq;
  const Building *b = find_building_const(S, hq);
  if (b == NULL || b->side != M->my_side) return 0;
  int lv = planned_building_level(S, a, hq);
  return HQ_LEVELS[lv].train_cap;
}

static int building_turret_power_const(const Building *b) {
  return b->type == BTYPE_HQ ? HQ_LEVELS[b->level].turret
                             : BASE_LEVELS[b->level].turret;
}

static int count_warriors_at(const GameState *S, Side side, int region) {
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == side && w->region == region) ++cnt;
  }
  return cnt;
}

static int count_stationary_warriors_at(const GameState *S, Side side,
                                        int region) {
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == side && w->region == region &&
        w->state == WSTATE_STATIONARY)
      ++cnt;
  }
  return cnt;
}

static int count_side_warriors(const GameState *S, Side side) {
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i)
    if (S->warriors.data[i].id.side == side) ++cnt;
  return cnt;
}


static int side_total_warrior_hp(const GameState *S, Side side) {
  int hp = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == side) hp += w->hp;
  }
  return hp;
}

static MAYBE_UNUSED int side_base_count(const GameState *S, Side side) {
  int cnt = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side == side && b->type == BTYPE_BASE) ++cnt;
  }
  return cnt;
}

static int side_current_income(const GameState *S, Side side) {
  int income = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != side) continue;
    int workers = count_warriors_at(S, side, b->region);
    income += WORK_INCOME * min_int(workers, building_work_cap(b));
  }
  return income;
}

static MAYBE_UNUSED int side_total_building_hp(const GameState *S, Side side) {
  int hp = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side == side) hp += max_int(0, b->hp);
  }
  return hp;
}

static int total_side_warrior_hp_at(const GameState *S, Side side, int region) {
  int hp = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == side && w->region == region) hp += max_int(0, w->hp);
  }
  return hp;
}

static int planned_total_work_cap(const GameState *S, const GameMap *M,
                                  const Actions *a) {
  int cap = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    cap += planned_work_cap_at(S, M, a, b->region);
  }
  for (int i = 0; i < a->upgrades.len; ++i) {
    int r = a->upgrades.data[i];
    if (find_building_const(S, r) == NULL)
      cap += BASE_LEVELS[1].work_cap;
  }
  return cap;
}

static MAYBE_UNUSED int planned_moves_from_region(const Actions *a, int region,
                                     const GameState *S) {
  int cnt = 0;
  for (int i = 0; i < a->moves.len; ++i) {
    const Warrior *w = NULL;
    for (int j = 0; j < S->warriors.len; ++j) {
      if (wid_eq(S->warriors.data[j].id, a->moves.data[i].id)) {
        w = &S->warriors.data[j];
        break;
      }
    }
    if (w != NULL && w->region == region) ++cnt;
  }
  return cnt;
}

static int planned_workers_physically_remaining_at(const GameState *S,
                                                    const Actions *a,
                                                    Side side, int region) {
  int cnt = count_warriors_at(S, side, region);
  for (int i = 0; i < a->moves.len; ++i) {
    const Move *mv = &a->moves.data[i];
    const Warrior *w = NULL;
    for (int j = 0; j < S->warriors.len; ++j) {
      if (wid_eq(S->warriors.data[j].id, mv->id)) {
        w = &S->warriors.data[j];
        break;
      }
    }
    if (w == NULL || w->id.side != side) continue;
    if (w->region == region) --cnt;
  }
  return cnt;
}

static int planned_workers_committed_to_region(const GameState *S,
                                               const Actions *a,
                                               Side side, int region) {
  int cnt = planned_workers_physically_remaining_at(S, a, side, region);
  for (int i = 0; i < a->moves.len; ++i) {
    const Move *mv = &a->moves.data[i];
    const Warrior *w = NULL;
    for (int j = 0; j < S->warriors.len; ++j) {
      if (wid_eq(S->warriors.data[j].id, mv->id)) {
        w = &S->warriors.data[j];
        break;
      }
    }
    if (w == NULL || w->id.side != side) continue;
    if (mv->target == region) ++cnt;
  }
  return cnt;
}

/* Construction and movement happen before training, but labor happens after
   training.  A warrior trained this turn cannot be used as a movement source or
   to satisfy this morning's construction legality, but it can work at HQ this
   evening.  Worker-slot accounting therefore needs this labor-only helper. */
static int planned_workers_committed_to_region_for_labor(const GameState *S,
                                                         const GameMap *M,
                                                         const Actions *a,
                                                         Side side,
                                                         int region) {
  int cnt = 0;

  /* Count warriors by their committed work destination, not just by their
     current square.  A warrior already moving to a base should reserve that
     slot, otherwise the worker-support loop keeps sending a new HQ unit every
     turn until the first one finally arrives. */
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != side) continue;
    if (w->state == WSTATE_MOVING) {
      if (w->target == region) ++cnt;
    } else if (w->region == region) {
      ++cnt;
    }
  }

  for (int i = 0; i < a->moves.len; ++i) {
    const Move *mv = &a->moves.data[i];
    const Warrior *w = NULL;
    for (int j = 0; j < S->warriors.len; ++j) {
      if (wid_eq(S->warriors.data[j].id, mv->id)) {
        w = &S->warriors.data[j];
        break;
      }
    }
    if (w == NULL || w->id.side != side) continue;
    if (w->state == WSTATE_STATIONARY && w->region == region && mv->target != region)
      --cnt;
    if (mv->target == region && w->region != region)
      ++cnt;
  }

  if (side == M->my_side && region == M->my_hq)
    cnt += a->train_n;
  return cnt;
}


static int region_has_enemy_warrior(const GameState *S, const GameMap *M,
                                    int region) {
  return count_warriors_at(S, opposite(M->my_side), region) > 0;
}


static int enemy_warrior_count_at(const GameState *S, const GameMap *M,
                                  int region) {
  return count_warriors_at(S, opposite(M->my_side), region);
}

static int enemy_next_step_count_at(const GameState *S, const GameMap *M,
                                    const Paths *P, int region) {
  if (P == NULL) return 0;
  Side opp = opposite(M->my_side);
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != opp || w->state != WSTATE_MOVING) continue;
    if (w->region < 0 || w->region >= P->N || w->target < 0 || w->target >= P->N) continue;
    int nx = P->nxt[w->region][w->target];
    if (nx == region) ++cnt;
  }
  return cnt;
}

static int enemy_committed_target_count_at(const GameState *S,
                                           const GameMap *M, int region) {
  Side opp = opposite(M->my_side);
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != opp) continue;
    if (w->state == WSTATE_MOVING && w->target == region) ++cnt;
  }
  return cnt;
}

static int enemy_projected_stack_count_at(const GameState *S,
                                          const GameMap *M,
                                          const Paths *P, int region) {
  int cur = enemy_warrior_count_at(S, M, region);
  int next = enemy_next_step_count_at(S, M, P, region);
  int committed = enemy_committed_target_count_at(S, M, region);
  return cur + next + committed;
}

static int stack_path_first_danger(const GameState *S, const GameMap *M,
                                   const Paths *P, int from, int target,
                                   int include_target, int *danger_region_out,
                                   int *danger_count_out) {
  if (!ENABLE_STACK_ONLY_GUARD) return 0;
  if (P == NULL) return 0;
  if (from < 0 || target < 0 || from >= P->N || target >= P->N) return 0;
  if (P->nxt[from][target] == -1) return 0;

  int cur = from;
  for (int step = 0; step < STACK_LOOKAHEAD && cur != target; ++step) {
    cur = P->nxt[cur][target];
    if (cur < 0) break;
    if (!include_target && cur == target) break;
    int enemies = enemy_projected_stack_count_at(S, M, P, cur);
    if (enemies >= STACK_DANGER_COUNT) {
      if (danger_region_out) *danger_region_out = cur;
      if (danger_count_out) *danger_count_out = enemies;
      return 1;
    }
  }
  return 0;
}

static int stack_target_is_deliberate_combat(const GameState *S,
                                             const GameMap *M, int target) {
  if (target == M->opp_hq) return 1;
  const Building *b = find_building_const(S, target);
  if (b != NULL && b->side != M->my_side) return 1;
  if (enemy_warrior_count_at(S, M, target) > 0) return 1;
  return 0;
}

/* Full shortest-path safety check.
   Economy moves, neutral claims, and worker refills should not walk through an
   enemy base or enemy stack just because the target itself is harmless.  For a
   deliberate combat move, only the final target cell may contain enemies. */
static int full_path_enemy_blocked(const GameState *S, const GameMap *M,
                                   const Paths *P, int from, int target,
                                   int allow_enemy_on_target) {
  if (P == NULL) return 0;
  if (from < 0 || target < 0 || from >= P->N || target >= P->N) return 1;
  if (P->nxt[from][target] == -1) return 1;

  Side opp = opposite(M->my_side);
  int cur = from;
  int guard = 0;
  while (cur != target && guard++ <= P->N + 5) {
    cur = P->nxt[cur][target];
    if (cur < 0) return 1;
    int is_target = (cur == target);
    if (is_target && allow_enemy_on_target) continue;

    const Building *b = find_building_const(S, cur);
    if (b != NULL && b->side == opp) return 1;
    /* Blocking every cell with projected enemy movement was too conservative:
       against the original 46175.cpp it stopped many harmless economy/staging
       moves and cut our midgame production.  The intended loss-prevention rule
       is to avoid paths that currently contain enemy material; projected enemy
       presence is still checked at the destination by move_target_has_enemy_projected(). */
    if (enemy_warrior_count_at(S, M, cur) > 0) return 1;
  }
  return 0;
}

static int move_target_has_enemy_projected(const GameState *S,
                                           const GameMap *M,
                                           const Paths *P, int target) {
  if (enemy_warrior_count_at(S, M, target) > 0) return 1;
  if (P != NULL && enemy_projected_stack_count_at(S, M, P, target) > 0) return 1;
  return 0;
}

static int stack_guard_would_feed(const GameState *S, const GameMap *M,
                                  const Paths *P, const Warrior *w,
                                  int target, int flags) {
  if (!ENABLE_STACK_ONLY_GUARD) return 0;
  if (P == NULL) return 0;
  if ((flags & MOVE_FLAG_IGNORE_STACK_GUARD) != 0) return 0;
  if (w == NULL) return 1;

  int deliberate_combat = stack_target_is_deliberate_combat(S, M, target) ||
                          (target == M->my_hq) ||
                          ((flags & MOVE_FLAG_ALLOW_DANGER_TARGET) != 0);

  if (!deliberate_combat) {
    int dest_enemy = enemy_projected_stack_count_at(S, M, P, target);
    if (dest_enemy >= STACK_TARGET_DANGER_COUNT) return 1;
  }

  int danger_region = -1, danger_count = 0;
  int include_target = deliberate_combat ? 0 : 1;
  if (stack_path_first_danger(S, M, P, w->region, target, include_target,
                              &danger_region, &danger_count))
    return 1;
  return 0;
}

static int legal_upgrade_or_build_now(const GameState *S, const GameMap *M,
                                      int region) {
  if (count_warriors_at(S, M->my_side, region) <= 0) return 0;
  if (region_has_enemy_warrior(S, M, region)) return 0;
  const Building *b = find_building_const(S, region);
  if (b != NULL) return b->side == M->my_side && b->level < building_max_level(b);
  return is_stronghold(M, region);
}

static int legal_build_neutral_now(const GameState *S, const GameMap *M,
                                   int region) {
  if (!is_stronghold(M, region)) return 0;
  if (find_building_const(S, region) != NULL) return 0;
  if (count_warriors_at(S, M->my_side, region) <= 0) return 0;
  if (region_has_enemy_warrior(S, M, region)) return 0;
  return 1;
}

static int add_upgrade_action(Actions *a, int region) {
  if (action_has_upgrade(a, region)) return 0;
  VEC_PUSH(a->upgrades, region);
  return 1;
}

static int add_move_action_ex_stack_flags(Actions *a, const GameState *S,
                                          const GameMap *M, const Paths *P,
                                          WarriorId id, int target,
                                          int *budget, int flags) {
  if (!is_move_destination_candidate(M, target)) return 0;
#if ENABLE_RETREAT_FOLLOWUP_BLOCK
  if (target >= 0 && target < 256 && g_retreat_bad_region[target]) return 0;
#endif
  if (action_has_move_warrior(a, id)) return 0;
  const Warrior *w = find_warrior_const(S, id);
  if (w == NULL || w->id.side != M->my_side) return 0;
  if (w->state != WSTATE_STATIONARY) return 0;
  if ((flags & MOVE_FLAG_ALLOW_CONTESTED_SOURCE) == 0 &&
      region_has_enemy_warrior(S, M, w->region)) return 0;

  const Building *target_b = find_building_const(S, target);
  int target_enemy_building = (target_b != NULL && target_b->side != M->my_side);
  int allow_enemy_on_target = target_enemy_building || target == M->my_hq ||
      ((flags & MOVE_FLAG_ALLOW_DANGER_TARGET) != 0);
  if (full_path_enemy_blocked(S, M, P, w->region, target, allow_enemy_on_target))
    return 0;
  if (!allow_enemy_on_target && move_target_has_enemy_projected(S, M, P, target))
    return 0;

  /* Do not move a warrior away from an unbuilt stronghold.  If one of our
     warriors has reached a neutral stronghold, first build the base there in a
     later construction phase, then use that owned base as a source. */
  if (is_stronghold(M, w->region) && find_building_const(S, w->region) == NULL &&
      target != M->my_hq)
    return 0;

  if (stack_guard_would_feed(S, M, P, w, target, flags)) return 0;

  int cost = planned_my_building(S, M, a, target) ? 0 : MOVE_COST;
  if (*budget < cost) return 0;
  Move mv = {id, target};
  VEC_PUSH(a->moves, mv);
  *budget -= cost;
  return 1;
}

static int add_move_action_ex(Actions *a, const GameState *S, const GameMap *M,
                              WarriorId id, int target, int *budget,
                              int allow_contested_source) {
  int flags = allow_contested_source ? MOVE_FLAG_ALLOW_CONTESTED_SOURCE : 0;
  return add_move_action_ex_stack_flags(a, S, M, g_stack_guard_paths, id,
                                        target, budget, flags);
}

static int add_move_action(Actions *a, const GameState *S, const GameMap *M,
                           WarriorId id, int target, int *budget) {
  return add_move_action_ex(a, S, M, id, target, budget, 0);
}

static int add_neutral_claim_move_with_build_reserve(Actions *a, const GameState *S,
                                                    const GameMap *M,
                                                    WarriorId id, int target,
                                                    int *budget) {
#if RESERVE_BASE_GOLD_FOR_NEUTRAL_CLAIM
  if (find_building_const(S, target) != NULL) return 0;
  if (!is_stronghold(M, target)) return 0;
  if (region_has_enemy_warrior(S, M, target)) return 0;
  if (g_stack_guard_paths != NULL &&
      enemy_projected_stack_count_at(S, M, g_stack_guard_paths, target) > 0)
    return 0;
  int move_cost = planned_my_building(S, M, a, target) ? 0 : MOVE_COST;
  int reserve = BASE_LEVELS[1].cost;
  if (*budget < move_cost + reserve) return 0;
  if (!add_move_action(a, S, M, id, target, budget)) return 0;
  /* The construction will happen on a later turn after the warrior arrives, so
     no command spends this gold now.  We still remove it from the planner budget
     to stop later priorities in this turn from consuming the future build money. */
  *budget -= reserve;
  return 1;
#else
  return add_move_action(a, S, M, id, target, budget);
#endif
}


/* Hardcoded post-base-clear economy gate.
   Once the opponent has no ordinary BASE left, existing non-HQ base level-ups
   are disabled.  At that point the only allowed UPGRADE on an already-owned
   building should be our HQ level-up/repair; neutral BASE construction is still
   allowed because that is a new building, not a level-up. */
static int normal_base_count_for_side(const GameState *S, Side side) {
  int cnt = 0;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side == side && b->type == BTYPE_BASE) ++cnt;
  }
  return cnt;
}

static int enemy_normal_bases_cleared_now(const GameState *S, const GameMap *M) {
  return normal_base_count_for_side(S, opposite(M->my_side)) == 0;
}

static int skip_existing_non_hq_upgrade_after_enemy_baseclear(
    const GameState *S, const GameMap *M, const Building *b) {
  if (g_allow_base_upgrades_after_enemy_baseclear) return 0;
  return b != NULL && b->side == M->my_side && b->type == BTYPE_BASE &&
         enemy_normal_bases_cleared_now(S, M);
}

static int affordable_existing_upgrade_exists(const GameState *S,
                                              const GameMap *M,
                                              const Actions *a,
                                              int budget) {
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    if (skip_existing_non_hq_upgrade_after_enemy_baseclear(S, M, b)) continue;
    if (action_has_upgrade(a, b->region)) continue;
    if (!legal_upgrade_or_build_now(S, M, b->region)) continue;
    int cost = building_upgrade_cost(b);
    if (b->level < building_max_level(b) && cost <= budget) return 1;
  }
  return 0;
}

static int underfilled_building_deficit_total(const GameState *S,
                                              const GameMap *M,
                                              const Actions *a);

static int choose_best_existing_upgrade(const GameState *S, const GameMap *M,
                                        const Actions *a, int budget) {
  int best_region = -1;
  int best_score = 1000000000;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    if (skip_existing_non_hq_upgrade_after_enemy_baseclear(S, M, b)) continue;
    if (b->level >= building_max_level(b)) continue;
    if (action_has_upgrade(a, b->region)) continue;
    if (!legal_upgrade_or_build_now(S, M, b->region)) continue;
    int cost = building_upgrade_cost(b);
    if (cost > budget) continue;

    /* v8 upgrade order: prioritize HQ much earlier than v7.
       The intended ladder is roughly:
         HQ2 -> HQ3 -> Base2 -> HQ4 -> Base3 -> HQ5.
       This reflects that HQ upgrades improve training speed, warrior HP,
       HQ HP, turret power, and work capacity, not just income. */
    int target_level = b->level + 1;
    int priority;
    if (b->type == BTYPE_HQ) {
      if (target_level <= 2) priority = 0;
      else if (target_level == 3) priority = 1;
      else if (target_level == 4) priority = 3;
      else priority = 5;
    } else {
      if (target_level <= 2) priority = 2;
      else priority = 4;
    }
    int distance_penalty = M->my_side == SIDE_LEFT ? b->region : (M->N - 1 - b->region);
    int score = priority * 1000000 + cost * 10 + distance_penalty;
    if (score < best_score) {
      best_score = score;
      best_region = b->region;
    }
  }
  return best_region;
}


/* ---- Retreat losing fights -------------------------------------------------
   A retreat is represented by a MOVE order to the nearest owned building. */

typedef struct { int hp; int num; int eta; } RetreatUnit;

static int action_move_target_for_id(const Actions *a, WarriorId id, int *target_out) {
  for (int i = 0; i < a->moves.len; ++i) {
    if (wid_eq(a->moves.data[i].id, id)) {
      if (target_out) *target_out = a->moves.data[i].target;
      return 1;
    }
  }
  return 0;
}

static int retreat_path_eta_to_region(const Paths *P, int from, int final_target, int region) {
  if (P == NULL) return INF_HOPS;
  if (from < 0 || final_target < 0 || region < 0 || from >= P->N || final_target >= P->N || region >= P->N) return INF_HOPS;
  if (from == region) return 0;
  if (P->nxt[from][final_target] == -1) return INF_HOPS;
  int cur = from;
  for (int eta = 1; eta <= P->N + 5 && cur != final_target; ++eta) {
    cur = P->nxt[cur][final_target];
    if (cur < 0) return INF_HOPS;
    if (cur == region) return eta;
  }
  return INF_HOPS;
}

static int retreat_unit_eta_to_region(const GameState *S, const Paths *P, const Actions *a,
                                      const Warrior *w, int region) {
  int planned_target = -1;
  int has_planned = action_move_target_for_id(a, w->id, &planned_target);
  int moving = has_planned || w->state == WSTATE_MOVING;
  int final_target = has_planned ? planned_target : w->target;
  if (w->region == region) {
    Side enemy = opposite(w->id.side);
    int force_stays_now = count_warriors_at(S, enemy, region) > 0;
    if (!moving || final_target == region || force_stays_now) return 0;
    return INF_HOPS;
  }
  if (!moving) return INF_HOPS;
  return retreat_path_eta_to_region(P, w->region, final_target, region);
}

static int retreat_collect_units_for_region(const GameState *S, const GameMap *M,
                                            const Paths *P, const Actions *a,
                                            int region, Side side,
                                            RetreatUnit *out, int maxn) {
  (void)M;
  int n = 0;
  for (int i = 0; i < S->warriors.len && n < maxn; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != side || w->hp <= 0) continue;
    int eta = retreat_unit_eta_to_region(S, P, a, w, region);
    if (eta == INF_HOPS || eta > RETREAT_SIM_DAYS) continue;
    out[n].hp = max_int(1, w->hp);
    out[n].num = w->id.num;
    out[n].eta = eta;
    ++n;
  }
  return n;
}

static int retreat_alive_count_eta(const RetreatUnit *u, int n, int day) {
  int c = 0;
  for (int i = 0; i < n; ++i) if (u[i].eta <= day && u[i].hp > 0) ++c;
  return c;
}

static int retreat_weakest_index(RetreatUnit *u, int n, int day) {
  int best = -1;
  for (int i = 0; i < n; ++i) {
    if (u[i].eta > day || u[i].hp <= 0) continue;
    if (best < 0 || u[i].hp < u[best].hp || (u[i].hp == u[best].hp && u[i].num < u[best].num)) best = i;
  }
  return best;
}
static int retreat_damage_weakest(RetreatUnit *u, int n, int day) {
  int idx = retreat_weakest_index(u,n,day); if (idx < 0) return 0; --u[idx].hp; return 1;
}
static void retreat_side_attack(RetreatUnit *def, int def_n, int day, int *building_hp) {
  if (retreat_damage_weakest(def,def_n,day)) return;
  if (building_hp != NULL && *building_hp > 0) --(*building_hp);
}

static int retreat_sim_losing_region(const GameState *S, const GameMap *M,
                                     const Paths *P, const Actions *a, int region) {
  const Building *b = find_building_const(S, region);
  if (b != NULL && b->side == M->my_side) return 0;
  RetreatUnit fr[RETREAT_MAX_UNITS], en[RETREAT_MAX_UNITS];
  int fn = retreat_collect_units_for_region(S,M,P,a,region,M->my_side,fr,RETREAT_MAX_UNITS);
  if (fn <= 0) return 0;
  int en_n = retreat_collect_units_for_region(S,M,P,a,region,opposite(M->my_side),en,RETREAT_MAX_UNITS);
  int enemy_building_hp = 0, enemy_turret = 0, enemy_hq_train_each = 0, enemy_hq_train_hp = 0;
  if (b != NULL && b->side != M->my_side) {
    enemy_building_hp = max_int(0,b->hp);
    enemy_turret = building_turret_power_const(b);
    if (b->type == BTYPE_HQ) {
      int inc = side_current_income(S, opposite(M->my_side));
      enemy_hq_train_each = min_int(HQ_LEVELS[b->level].train_cap, inc / TRAIN_COST);
      enemy_hq_train_hp = HQ_LEVELS[b->level].warrior_hp;
    }
  }
  if (en_n <= 0 && enemy_building_hp <= 0) return 0;
  for (int day=0; day<RETREAT_SIM_DAYS; ++day) {
    if (enemy_hq_train_each > 0 && day > 0) {
      for (int k=0; k<enemy_hq_train_each && en_n<RETREAT_MAX_UNITS; ++k) {
        en[en_n].hp = enemy_hq_train_hp; en[en_n].num = 1000000 + day*10 + k; en[en_n].eta = day; ++en_n;
      }
    }
    int f_alive=retreat_alive_count_eta(fr,fn,day), e_alive=retreat_alive_count_eta(en,en_n,day);
    int enemy_present = e_alive>0 || enemy_building_hp>0;
    if (f_alive <= 0) {
      if (e_alive <= 0 && enemy_building_hp <= 0) return 0; /* mutual wipe */
      return 1;
    }
    if (!enemy_present) return 0;
    int f_cap=f_alive, e_cap=e_alive + (enemy_building_hp>0 ? enemy_turret : 0);
    if (M->my_side == SIDE_LEFT) {
      for (int k=0;k<f_cap;++k) retreat_side_attack(en,en_n,day,enemy_building_hp>0?&enemy_building_hp:NULL);
      for (int k=0;k<e_cap;++k) retreat_side_attack(fr,fn,day,NULL);
    } else {
      for (int k=0;k<e_cap;++k) retreat_side_attack(fr,fn,day,NULL);
      for (int k=0;k<f_cap;++k) retreat_side_attack(en,en_n,day,enemy_building_hp>0?&enemy_building_hp:NULL);
    }
    f_alive=retreat_alive_count_eta(fr,fn,day); e_alive=retreat_alive_count_eta(en,en_n,day);
    if (f_alive <= 0) {
      if (e_alive <= 0 && enemy_building_hp <= 0) return 0;
      return 1;
    }
    if (e_alive <= 0 && enemy_building_hp <= 0) return 0;
  }
  return 0;
}

static int retreat_nearest_owned_building(const GameState *S, const GameMap *M, const Paths *P, int from) {
  int best=-1; double best_d=1e100;
  for (int bi=0; bi<S->buildings.len; ++bi) {
    const Building *b=&S->buildings.data[bi];
    if (b->side != M->my_side || b->region == from) continue;
    if (P->nxt[from][b->region] == -1) continue;
    if (best < 0 || P->dist[from][b->region] < best_d) { best=b->region; best_d=P->dist[from][b->region]; }
  }
  return best;
}

static int issue_losing_fight_retreats(Actions *a, const GameState *S, const GameMap *M, const Paths *P, int *budget) {
#if !ENABLE_RETREAT_LOSING_FIGHTS
  (void)a; (void)S; (void)M; (void)P; (void)budget; return 0;
#else
  for (int i=0;i<256;++i) { g_retreat_bad_region[i]=0; g_retreat_safe_target[i]=-1; }
  int did=0;
  for (int r=0; r<M->N && r<256; ++r) {
    const Building *own_b=find_building_const(S,r);
    if (own_b != NULL && own_b->side == M->my_side) continue;
    if (count_stationary_warriors_at(S,M->my_side,r) <= 0) continue;
    const Building *b=find_building_const(S,r);
    RetreatUnit tmp[1];
    int enemy_material = (b != NULL && b->side != M->my_side) ||
      retreat_collect_units_for_region(S,M,P,a,r,opposite(M->my_side),tmp,1) > 0;
    if (!enemy_material) continue;
    if (!retreat_sim_losing_region(S,M,P,a,r)) continue;
    int safe=retreat_nearest_owned_building(S,M,P,r); if (safe < 0) safe=M->my_hq;
    g_retreat_bad_region[r]=1; g_retreat_safe_target[r]=safe;
    for (int wi=0; wi<S->warriors.len; ++wi) {
      const Warrior *w=&S->warriors.data[wi];
      if (w->id.side != M->my_side || w->region != r || w->state != WSTATE_STATIONARY) continue;
      if (action_has_move_warrior(a,w->id)) continue;
      if (add_move_action_ex_stack_flags(a,S,M,P,w->id,safe,budget,
          MOVE_FLAG_ALLOW_CONTESTED_SOURCE | MOVE_FLAG_IGNORE_STACK_GUARD)) did=1;
    }
  }
  return did;
#endif
}

static int issue_existing_upgrades(Actions *a, const GameState *S,
                                   const GameMap *M, int *budget) {
  int did = 0;
  while (1) {
    int r = choose_best_existing_upgrade(S, M, a, *budget);
    if (r < 0) break;
    const Building *b = find_building_const(S, r);
    if (b == NULL) break;
    if (b->type != BTYPE_HQ && underfilled_building_deficit_total(S, M, a) > 0) break;
    if (skip_existing_non_hq_upgrade_after_enemy_baseclear(S, M, b)) break;
    int cost = building_upgrade_cost(b);
    if (cost > *budget) break;
    if (add_upgrade_action(a, r)) {
      *budget -= cost;
      did = 1;
    } else {
      break;
    }
  }
  return did;
}

static int issue_neutral_builds(Actions *a, const GameState *S,
                                const GameMap *M, int *budget) {
  int did = 0;
  while (*budget >= BASE_LEVELS[1].cost) {
    int best_region = -1;
    int best_score = 1000000000;
    for (int i = 0; i < M->strongholds.len; ++i) {
      int r = M->strongholds.data[i];
      if (action_has_upgrade(a, r)) continue;
      if (!legal_build_neutral_now(S, M, r)) continue;
      int score = M->my_side == SIDE_LEFT ? r : (M->N - 1 - r);
      if (score < best_score) {
        best_score = score;
        best_region = r;
      }
    }
    if (best_region < 0) break;
    add_upgrade_action(a, best_region);
    *budget -= BASE_LEVELS[1].cost;
    did = 1;
  }
  return did;
}

static int source_surplus_after_plan(const GameState *S, const GameMap *M,
                                     const Actions *a, int region) {
  if (!planned_my_building(S, M, a, region)) return 0;
  int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, region);
  int cap = planned_work_cap_at(S, M, a, region);

  /* Normal expansion keeps one extra garrison at each building, but the
     opening neutral phase needs the second starting warrior.  For the first
     few turns, while the opening quota is not done, let the HQ keep only its
     worker slot.  This matches opponents that send two initial warriors to
     nearby neutral strongholds and prevents us from falling behind immediately. */
  int keep = cap + NORMAL_EXTRA_GARRISON;
  if (region == M->my_hq && !g_opening_neutral_done &&
      g_current_turn <= OPENING_RELAX_HQ_KEEP_UNTIL_TURN) {
    keep = cap;
  }
  return max_int(0, remaining - keep);
}

static int pick_surplus_warrior_from_region(const GameState *S,
                                            const GameMap *M,
                                            const Actions *a, int region,
                                            WarriorId *out) {
  if (source_surplus_after_plan(S, M, a, region) <= 0) return 0;
  for (int i = S->warriors.len - 1; i >= 0; --i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side || w->region != region) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (region_has_enemy_warrior(S, M, region)) continue;
    *out = w->id;
    return 1;
  }
  return 0;
}

static int my_hq_level(const GameState *S, const GameMap *M) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  return hq->level;
}

static int side_net_income(const GameState *S, Side side) {
  int income = side_current_income(S, side);
  int upkeep = UPKEEP_PER_WARRIOR * count_side_warriors(S, side);
  return income - upkeep;
}

static int hq_upgrade_should_be_prioritized(const GameState *S,
                                            const GameMap *M,
                                            const Actions *a,
                                            int budget) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (hq->level >= HQ_MAX_LEVEL) return 0;
  if (action_has_upgrade(a, M->my_hq)) return 0;

  int cost = building_upgrade_cost(hq);
  if (budget >= cost) return 1;

  int net = side_net_income(S, M->my_side);
  if (net <= 0) return 0;
  return budget + HQ_SAVE_LOOKAHEAD_TURNS * net >= cost;
}

static int issue_hq_upgrade_if_affordable(Actions *a, const GameState *S,
                                           const GameMap *M, int *budget) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (hq->level >= HQ_MAX_LEVEL) return 0;
  if (action_has_upgrade(a, M->my_hq)) return 0;
  if (!legal_upgrade_or_build_now(S, M, M->my_hq)) return 0;
  int cost = building_upgrade_cost(hq);
  if (cost > *budget) return 0;
  if (!add_upgrade_action(a, M->my_hq)) return 0;
  *budget -= cost;
  return 1;
}

static int should_save_for_hq_upgrade(const GameState *S, const GameMap *M,
                                      const Actions *a, int budget) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (!hq_upgrade_should_be_prioritized(S, M, a, budget)) return 0;
  if (action_has_upgrade(a, M->my_hq)) return 0;
  int cost = building_upgrade_cost(hq);
  if (budget >= cost && legal_upgrade_or_build_now(S, M, M->my_hq)) return 0;
  return 1;
}

static int legal_hq_construction_now(const GameState *S, const GameMap *M) {
  if (count_warriors_at(S, M->my_side, M->my_hq) <= 0) return 0;
  if (region_has_enemy_warrior(S, M, M->my_hq)) return 0;
  const Building *hq = find_building_const(S, M->my_hq);
  return hq != NULL && hq->side == M->my_side && hq->type == BTYPE_HQ;
}

static int late_hq_tiebreak_action_cost(const GameState *S,
                                        const GameMap *M) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;

  /* Prefer reaching HQ5 over repairing: upgrading also refreshes current HQ HP
     to the next level's maximum and improves warrior HP/train capacity. */
  if (hq->level < HQ_MAX_LEVEL) return building_upgrade_cost(hq);

  int max_hp = HQ_LEVELS[hq->level].hp;
  if (hq->hp < max_hp) return HQ_HEAL_COST;
  return 0;
}

static int late_hq_tiebreak_should_save(const GameState *S, const GameMap *M,
                                        const Actions *a, int budget,
                                        int turn) {
  if (turn < LATE_TIEBREAK_START_TURN) return 0;
  if (action_has_upgrade(a, M->my_hq)) return 0;

  int cost = late_hq_tiebreak_action_cost(S, M);
  if (cost <= 0) return 0;

  if (budget >= cost) {
    /* If it is affordable but construction is not legal this morning, reserve
       the gold while a worker is being brought to HQ. */
    return !legal_hq_construction_now(S, M);
  }

  int net = side_net_income(S, M->my_side);
  if (net <= 0) return 0;
  int remaining = MAX_TURN - turn + 1;
  int lookahead = min_int(LATE_TIEBREAK_SAVE_LOOKAHEAD_TURNS, remaining);
  return budget + lookahead * net >= cost;
}

static int late_hq_tiebreak_reserved_gold(const GameState *S,
                                          const GameMap *M,
                                          const Actions *a, int budget,
                                          int turn) {
  if (!late_hq_tiebreak_should_save(S, M, a, budget, turn)) return 0;
  int cost = late_hq_tiebreak_action_cost(S, M);
  if (cost <= 0) return 0;
  return min_int(budget, cost);
}

static int issue_late_hq_tiebreak_if_affordable(Actions *a,
                                                const GameState *S,
                                                const GameMap *M,
                                                int *budget, int turn) {
  if (turn < LATE_TIEBREAK_START_TURN) return 0;
  if (action_has_upgrade(a, M->my_hq)) return 0;
  if (!legal_hq_construction_now(S, M)) return 0;

  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL) return 0;

  int cost = 0;
  if (hq->level < HQ_MAX_LEVEL) {
    cost = building_upgrade_cost(hq);
  } else if (hq->hp < HQ_LEVELS[hq->level].hp) {
    cost = HQ_HEAL_COST;
  } else {
    return 0;
  }

  if (cost > *budget) return 0;
  if (!add_upgrade_action(a, M->my_hq)) return 0;
  *budget -= cost;
  return 1;
}

static int ensure_late_hq_tiebreak_worker(Actions *a, const GameState *S,
                                          const GameMap *M, const Paths *P,
                                          int *budget, int turn) {
  if (turn < LATE_TIEBREAK_START_TURN) return 0;
  int cost = late_hq_tiebreak_action_cost(S, M);
  if (cost <= 0) return 0;
  if (count_warriors_at(S, M->my_side, M->my_hq) > 0) return 0;
  for (int mi = 0; mi < a->moves.len; ++mi)
    if (a->moves.data[mi].target == M->my_hq) return 0;

  /* Do not force a worker home if the action is economically impossible before
     the turn limit; otherwise this would weaken the endgame attack for no gain. */
  if (!late_hq_tiebreak_should_save(S, M, a, *budget, turn) && *budget < cost)
    return 0;

  double best = INFINITY;
  WarriorId best_id = {M->my_side, -1};
  for (int wi = 0; wi < S->warriors.len; ++wi) {
    const Warrior *w = &S->warriors.data[wi];
    if (w->id.side != M->my_side) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (region_has_enemy_warrior(S, M, w->region)) continue;
    if (P->nxt[w->region][M->my_hq] == -1) continue;

    int surplus_ok = 0;
    if (planned_my_building(S, M, a, w->region)) {
      int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, w->region);
      int cap = planned_work_cap_at(S, M, a, w->region);
      surplus_ok = remaining > cap;
    } else if (is_stronghold(M, w->region) && find_building_const(S, w->region) == NULL) {
      surplus_ok = 0;
    } else {
      surplus_ok = 1;
    }
    if (!surplus_ok && best_id.num >= 0) continue;

    double d = P->dist[w->region][M->my_hq];
    if (best_id.num < 0 || surplus_ok || d < best) {
      best = d;
      best_id = w->id;
    }
  }
  if (best_id.num < 0) return 0;
  return add_move_action_ex(a, S, M, best_id, M->my_hq, budget, 1);
}

static int ensure_hq_upgrade_worker(Actions *a, const GameState *S,
                                    const GameMap *M, const Paths *P,
                                    int *budget) {
  if (!hq_upgrade_should_be_prioritized(S, M, a, *budget)) return 0;
  if (count_warriors_at(S, M->my_side, M->my_hq) > 0) return 0;
  for (int mi = 0; mi < a->moves.len; ++mi)
    if (a->moves.data[mi].target == M->my_hq) return 0;

  double best = INFINITY;
  WarriorId best_id = {M->my_side, -1};
  for (int wi = 0; wi < S->warriors.len; ++wi) {
    const Warrior *w = &S->warriors.data[wi];
    if (w->id.side != M->my_side) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (region_has_enemy_warrior(S, M, w->region)) continue;
    if (P->nxt[w->region][M->my_hq] == -1) continue;

    int surplus_ok = 0;
    if (planned_my_building(S, M, a, w->region)) {
      int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, w->region);
      int cap = planned_work_cap_at(S, M, a, w->region);
      surplus_ok = remaining > cap;
    } else if (is_stronghold(M, w->region) && find_building_const(S, w->region) == NULL) {
      surplus_ok = 0;
    } else {
      surplus_ok = 1;
    }
    if (!surplus_ok && best_id.num >= 0) continue;

    double d = P->dist[w->region][M->my_hq];
    if (best_id.num < 0 || surplus_ok || d < best) {
      best = d;
      best_id = w->id;
    }
  }
  if (best_id.num < 0) return 0;
  return add_move_action_ex(a, S, M, best_id, M->my_hq, budget, 1);
}

static int choose_surplus_source_for_target(const GameState *S,
                                            const GameMap *M,
                                            const Paths *P,
                                            const Actions *a, int target,
                                            WarriorId *out) {
  double best = INFINITY;
  WarriorId best_id = {M->my_side, -1};
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    if (b->region == target) continue;
    if (source_surplus_after_plan(S, M, a, b->region) <= 0) continue;
    if (P->nxt[b->region][target] == -1) continue;
    WarriorId cand;
    if (!pick_surplus_warrior_from_region(S, M, a, b->region, &cand)) continue;
    double d = P->dist[b->region][target];
    if (d < best) {
      best = d;
      best_id = cand;
    }
  }
  if (best_id.num < 0) return 0;
  *out = best_id;
  return 1;
}

static int choose_worker_support_source_for_target(const GameState *S,
                                                   const GameMap *M,
                                                   const Paths *P,
                                                   const Actions *a,
                                                   int target,
                                                   WarriorId *out) {
  double best = INFINITY;
  int best_is_building_surplus = -1;
  WarriorId best_id = {M->my_side, -1};

  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (w->region == target) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (region_has_enemy_warrior(S, M, w->region)) continue;
    if (P->nxt[w->region][target] == -1) continue;
    if (full_path_enemy_blocked(S, M, P, w->region, target, 0)) continue;

    int is_building_surplus = 0;
    int movable = 0;
    if (planned_my_building(S, M, a, w->region)) {
      int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, w->region);
      int cap = planned_work_cap_at(S, M, a, w->region);
      if (remaining > cap) {
        movable = 1;
        is_building_surplus = 1;
      }
    } else {
      const Building *fb = find_building_const(S, w->region);
      if (is_stronghold(M, w->region) && fb == NULL) {
        movable = 0;  /* do not steal a neutral-builder standing on empty land */
      } else {
        movable = 1;  /* idle field/staging unit can become a worker */
      }
    }
    if (!movable) continue;

    double d = P->dist[w->region][target];
    int better = 0;
    if (best_id.num < 0) better = 1;
    else if (is_building_surplus != best_is_building_surplus)
      better = is_building_surplus > best_is_building_surplus;
    else if (d < best)
      better = 1;
    if (better) {
      best = d;
      best_is_building_surplus = is_building_surplus;
      best_id = w->id;
    }
  }

  if (best_id.num < 0) return 0;
  *out = best_id;
  return 1;
}

static int issue_worker_redistribution(Actions *a, const GameState *S,
                                       const GameMap *M, const Paths *P,
                                       int *budget) {
  int did = 0;
  for (int pass = 0; pass < 2; ++pass) {
    int best_bi = -1;
    int best_def = 0;
    int best_region_score = 1000000000;
    for (int bi = 0; bi < S->buildings.len; ++bi) {
      const Building *dst = &S->buildings.data[bi];
      if (dst->side != M->my_side) continue;
      if (move_target_has_enemy_projected(S, M, P, dst->region)) continue;
      int cap = planned_work_cap_at(S, M, a, dst->region);
      int have = planned_workers_committed_to_region_for_labor(S, M, a, M->my_side, dst->region);
      int def = cap - have;
      if (def <= 0) continue;
      int region_score = M->my_side == SIDE_LEFT ? dst->region : (M->N - 1 - dst->region);
      if (best_bi < 0 || def > best_def ||
          (def == best_def && region_score < best_region_score)) {
        best_bi = bi;
        best_def = def;
        best_region_score = region_score;
      }
    }
    if (best_bi < 0) break;

    const Building *dst = &S->buildings.data[best_bi];
    int target = dst->region;
    while (planned_workers_committed_to_region_for_labor(S, M, a, M->my_side, target) <
           planned_work_cap_at(S, M, a, target)) {
      WarriorId id;
      if (!choose_worker_support_source_for_target(S, M, P, a, target, &id)) break;
      if (!add_move_action_ex_stack_flags(a, S, M, P, id, target, budget,
                                          MOVE_FLAG_IGNORE_STACK_GUARD)) break;
      did = 1;
    }
  }
  return did;
}


static int level1_base_upgrade_candidate_exists(const GameState *S,
                                                const GameMap *M,
                                                const Actions *a);
static int hq5_base2_savings_needed_before_train(const GameState *S,
                                                  const GameMap *M,
                                                  const Actions *a,
                                                  int budget);

static int underfilled_building_deficit_total(const GameState *S,
                                              const GameMap *M,
                                              const Actions *a) {
  int deficit = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    int cap = planned_work_cap_at(S, M, a, b->region);
    int have = planned_workers_committed_to_region_for_labor(S, M, a, M->my_side, b->region);
    if (have < cap) deficit += cap - have;
  }
  if (deficit > UNDERFILLED_WORKER_TRAIN_MAX_DEFICIT)
    deficit = UNDERFILLED_WORKER_TRAIN_MAX_DEFICIT;
  return deficit;
}

static int underfilled_base_deficit_total(const GameState *S,
                                          const GameMap *M,
                                          const Actions *a) {
  int deficit = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    if (b->type != BTYPE_BASE) continue;
    int cap = planned_work_cap_at(S, M, a, b->region);
    int have = planned_workers_committed_to_region_for_labor(S, M, a, M->my_side, b->region);
    if (have < cap) deficit += cap - have;
  }
  if (deficit > UNDERFILLED_WORKER_TRAIN_MAX_DEFICIT)
    deficit = UNDERFILLED_WORKER_TRAIN_MAX_DEFICIT;
  return deficit;
}

static int issue_underfilled_building_worker_support(Actions *a,
                                                     const GameState *S,
                                                     const GameMap *M,
                                                     const Paths *P,
                                                     int *budget,
                                                     int hq_save_lock) {
#if !ENABLE_UNDERFILLED_BUILDING_WORKER_SUPPORT
  (void)a; (void)S; (void)M; (void)P; (void)budget; (void)hq_save_lock;
  return 0;
#else
  int did = 0;

  /* First use existing surplus workers.  This does not spend upgrade gold when
     the destination is an owned building, but it raises income before ordinary
     base-level upgrades get a chance to consume the same economy plan. */
  did |= issue_worker_redistribution(a, S, M, P, budget);

  /* If no nearby/owned-building surplus can cover the remaining open work
     slots, ask HQ to make workers.  This is deliberately below HQ save-lock:
     while saving for an HQ upgrade/repair we do not spend the exact HQ budget
     on population.  It is, however, above ordinary base upgrades, so base level
     money is not hoarded while existing work slots are empty. */
  if (!hq_save_lock) {
    int hq5 = my_hq_level(S, M) >= HQ_MAX_LEVEL;
    int base2_pending = hq5 && level1_base_upgrade_candidate_exists(S, M, a);
    int base_deficit = underfilled_base_deficit_total(S, M, a);
    int deficit = base2_pending ? base_deficit :
        underfilled_building_deficit_total(S, M, a);

    /* Do not spend the base-upgrade reserve merely to fill HQ-only work slots.
       In the v14 logs, HQ5 surplus was repeatedly converted into TRAIN while
       many legal BASE 1->2 upgrades were still waiting for the 600-gold budget.
       BASE slots remain priority 1; HQ-only deficits wait until no legal BASE
       1->2 upgrade is pending. */
    if (base2_pending && base_deficit <= 0)
      deficit = 0;

    if (deficit > 0 && *budget >= TRAIN_COST) {
      int cap = planned_train_cap(S, M, a);
      int room = cap - a->train_n;
      int n = min_int(deficit, room);
      n = min_int(n, *budget / TRAIN_COST);
      if (base2_pending)
        n = min_int(n, max_int(0, *budget - BASE_LEVELS[2].cost) / TRAIN_COST);
      if (n > 0) {
        a->train_n += n;
        *budget -= TRAIN_COST * n;
        did = 1;
      }
    }
  }
  return did;
#endif
}

static int neutral_target_already_claimed(const GameState *S, const GameMap *M,
                                          const Actions *a, int target) {
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->region == target) return 1;
    if (w->state == WSTATE_MOVING && w->target == target) return 1;
  }
  for (int i = 0; i < a->moves.len; ++i)
    if (a->moves.data[i].target == target)
      return 1;
  return 0;
}

static int choose_neutral_expansion(const GameState *S, const GameMap *M,
                                    const Paths *P, const Actions *a,
                                    int *target_out, WarriorId *id_out) {
  double best = INFINITY;
  int best_target = -1;
  WarriorId best_id = {M->my_side, -1};

  for (int ti = 0; ti < M->strongholds.len; ++ti) {
    int target = M->strongholds.data[ti];
    if (find_building_const(S, target) != NULL) continue;
    if (neutral_target_already_claimed(S, M, a, target)) continue;
    if (move_target_has_enemy_projected(S, M, P, target)) continue;

    WarriorId id;
    if (!choose_surplus_source_for_target(S, M, P, a, target, &id)) continue;

    const Warrior *w = NULL;
    for (int i = 0; i < S->warriors.len; ++i)
      if (wid_eq(S->warriors.data[i].id, id)) {
        w = &S->warriors.data[i];
        break;
      }
    if (w == NULL) continue;

    double own_distance = P->dist[M->my_hq][target];
    double source_distance = P->dist[w->region][target];
    double forward_penalty = M->my_side == SIDE_LEFT ? target : (M->N - 1 - target);
    /* Base-claiming priority is closest-to-our-HQ first.  Source distance is
       only a tie-breaker among equally close candidate strongholds. */
    double score = 1000.0 * own_distance + source_distance + 0.001 * forward_penalty;
    if (score < best) {
      best = score;
      best_target = target;
      best_id = id;
    }
  }

  if (best_target < 0) return 0;
  *target_out = best_target;
  *id_out = best_id;
  return 1;
}

static int required_attackers_for_enemy_base(const GameState *S,
                                             const GameMap *M, int region) {
  const Building *b = find_building_const(S, region);
  if (b == NULL || b->side == M->my_side) return 0;
  if (b->type == BTYPE_HQ && !ENABLE_ENEMY_HQ_ATTACK) return 1000000;

  int enemy_hp = total_side_warrior_hp_at(S, opposite(M->my_side), region);
  int enemy_cnt = count_warriors_at(S, opposite(M->my_side), region);
  int turret = building_turret_power_const(b);

  /* v8 is intentionally more aggressive.  The previous one-wave criterion
     effectively required one fresh attacker per visible HP point, which made
     the bot hesitate even when it had a clear economic and army advantage.
     A sustained fight over multiple combat phases can convert fewer bodies
     into more total damage, so use a reduced HP-to-attacker estimate plus a
     small flat cushion for turret/combat uncertainty. */
  int hp_need = enemy_hp + max_int(0, b->hp);
  int sustained_need = (hp_need * 2 + 2) / 3;
  int margin = (b->type == BTYPE_HQ) ? 4 : 2;
  int need = sustained_need + turret + enemy_cnt + margin;
  return max_int(1, need);
}

static int planned_moves_to_region(const Actions *a, int region) {
  int cnt = 0;
  for (int i = 0; i < a->moves.len; ++i)
    if (a->moves.data[i].target == region) ++cnt;
  return cnt;
}

static int movable_surplus_from_region(const GameState *S, const GameMap *M,
                                          const Actions *a, int region) {
  int surplus = source_surplus_after_plan(S, M, a, region);
  if (surplus <= 0) return 0;
  if (region_has_enemy_warrior(S, M, region)) return 0;

  int movable = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side || w->region != region) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    ++movable;
  }
  return min_int(surplus, movable);
}

static int attack_surplus_after_plan(const GameState *S, const GameMap *M,
                                     const Actions *a, int region) {
  if (!planned_my_building(S, M, a, region)) return 0;
  int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, region);
  int cap = planned_work_cap_at(S, M, a, region);
  /* For attacks, allow using the normal extra garrison.  Worker slots are still
     preserved so the economy does not collapse immediately after committing. */
  return max_int(0, remaining - cap);
}

static int movable_attack_surplus_from_region(const GameState *S, const GameMap *M,
                                              const Actions *a, int region) {
  int surplus = attack_surplus_after_plan(S, M, a, region);
  if (surplus <= 0) return 0;
  if (region_has_enemy_warrior(S, M, region)) return 0;

  int movable = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side || w->region != region) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    ++movable;
  }
  return min_int(surplus, movable);
}

static int pick_attack_warrior_from_region(const GameState *S, const GameMap *M,
                                           const Actions *a, int region,
                                           WarriorId *out) {
  if (movable_attack_surplus_from_region(S, M, a, region) <= 0) return 0;
  for (int i = S->warriors.len - 1; i >= 0; --i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side || w->region != region) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (region_has_enemy_warrior(S, M, region)) continue;
    *out = w->id;
    return 1;
  }
  return 0;
}

static MAYBE_UNUSED int choose_attack_source_for_target(const GameState *S, const GameMap *M,
                                           const Paths *P, const Actions *a,
                                           int target, WarriorId *out) {
  double best = INFINITY;
  WarriorId best_id = {M->my_side, -1};
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    if (b->region == target) continue;
    if (movable_attack_surplus_from_region(S, M, a, b->region) <= 0) continue;
    if (P->nxt[b->region][target] == -1) continue;
    if (!attack_path_first_enemy_is_target(S, M, P, b->region, target)) continue;
    WarriorId cand;
    if (!pick_attack_warrior_from_region(S, M, a, b->region, &cand)) continue;
    double d = P->dist[b->region][target];
    if (d < best) {
      best = d;
      best_id = cand;
    }
  }
  if (best_id.num < 0) return 0;
  *out = best_id;
  return 1;
}

static MAYBE_UNUSED int available_surplus_attackers_to_target(const GameState *S,
                                                 const GameMap *M,
                                                 const Paths *P,
                                                 const Actions *a,
                                                 int target) {
  int available = 0;
  for (int sj = 0; sj < S->buildings.len; ++sj) {
    const Building *src = &S->buildings.data[sj];
    if (src->side != M->my_side) continue;
    if (P->nxt[src->region][target] == -1) continue;
    if (!attack_path_first_enemy_is_target(S, M, P, src->region, target)) continue;
    available += movable_attack_surplus_from_region(S, M, a, src->region);
  }
  return available;
}

static int committed_attackers_eta_count(const GameState *S, const GameMap *M,
                                         const Paths *P, const Actions *a,
                                         int target, int eta) {
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->region == target) {
      if (eta == 0) ++cnt;
      continue;
    }
    if (w->state == WSTATE_MOVING && w->target == target) {
      int h = path_hops_between(P, w->region, target);
      if (h == eta) ++cnt;
    }
  }
  for (int i = 0; i < a->moves.len; ++i) {
    const Warrior *w = find_warrior_const(S, a->moves.data[i].id);
    if (w == NULL || w->id.side != M->my_side) continue;
    if (a->moves.data[i].target != target) continue;
    int h = path_hops_between(P, w->region, target);
    if (h == eta) ++cnt;
  }
  return cnt;
}

static int max_committed_eta_to_target(const GameState *S, const GameMap *M,
                                       const Paths *P, const Actions *a,
                                       int target) {
  int best = -1;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->region == target) best = max_int(best, 0);
    if (w->state == WSTATE_MOVING && w->target == target) {
      int h = path_hops_between(P, w->region, target);
      if (h < INF_HOPS) best = max_int(best, h);
    }
  }
  for (int i = 0; i < a->moves.len; ++i) {
    const Warrior *w = find_warrior_const(S, a->moves.data[i].id);
    if (w == NULL || w->id.side != M->my_side) continue;
    if (a->moves.data[i].target != target) continue;
    int h = path_hops_between(P, w->region, target);
    if (h < INF_HOPS) best = max_int(best, h);
  }
  return best;
}

static int available_stationary_attackers_with_hop_cmp(const GameState *S,
                                                       const GameMap *M,
                                                       const Paths *P,
                                                       const Actions *a,
                                                       int target,
                                                       int hop_limit,
                                                       int exact) {
  int total = 0;
  for (int sj = 0; sj < S->buildings.len; ++sj) {
    const Building *src = &S->buildings.data[sj];
    if (src->side != M->my_side) continue;
    int h = path_hops_between(P, src->region, target);
    if (h >= INF_HOPS) continue;
    if (!attack_path_first_enemy_is_target(S, M, P, src->region, target)) continue;
    if (exact) {
      if (h != hop_limit) continue;
    } else {
      if (h > hop_limit) continue;
    }
    total += movable_surplus_from_region(S, M, a, src->region);
  }
  return total;
}

static int collect_stationary_attackers_exact_hops(const GameState *S,
                                                   const GameMap *M,
                                                   const Paths *P,
                                                   const Actions *a,
                                                   int target, int hops,
                                                   WarriorId *ids, int max_ids) {
  int len = 0;
  for (int sj = 0; sj < S->buildings.len && len < max_ids; ++sj) {
    const Building *src = &S->buildings.data[sj];
    if (src->side != M->my_side) continue;
    if (path_hops_between(P, src->region, target) != hops) continue;
    if (!attack_path_first_enemy_is_target(S, M, P, src->region, target)) continue;
    int surplus = movable_surplus_from_region(S, M, a, src->region);
    if (surplus <= 0) continue;

    for (int wi = S->warriors.len - 1; wi >= 0 && surplus > 0 && len < max_ids; --wi) {
      const Warrior *w = &S->warriors.data[wi];
      if (w->id.side != M->my_side || w->region != src->region) continue;
      if (w->state != WSTATE_STATIONARY) continue;
      if (action_has_move_warrior(a, w->id)) continue;
      ids[len++] = w->id;
      --surplus;
    }
  }
  return len;
}

static int timed_capture_feasible_eta(const GameState *S, const GameMap *M,
                                      const Paths *P, const Actions *a,
                                      int target, int req, int *eta_out,
                                      int *need_today_out) {
  int max_eta = max_committed_eta_to_target(S, M, P, a, target);
  int chosen_eta = -1;
  int need_today = 0;

  if (max_eta >= 0) {
    chosen_eta = max_eta;
    int committed_exact = committed_attackers_eta_count(S, M, P, a, target, chosen_eta);
    int waitable_le = available_stationary_attackers_with_hop_cmp(S, M, P, a, target, chosen_eta, 0);
    int waitable_lt = chosen_eta > 0 ?
        available_stationary_attackers_with_hop_cmp(S, M, P, a, target, chosen_eta - 1, 0) : 0;
    if (committed_exact + waitable_le < req) return 0;
    need_today = max_int(0, req - committed_exact - waitable_lt);
  } else {
    for (int h = 1; h <= P->N + 5; ++h) {
      int waitable_le = available_stationary_attackers_with_hop_cmp(S, M, P, a, target, h, 0);
      if (waitable_le >= req) {
        chosen_eta = h;
        int waitable_lt = available_stationary_attackers_with_hop_cmp(S, M, P, a, target, h - 1, 0);
        need_today = max_int(0, req - waitable_lt);
        break;
      }
    }
    if (chosen_eta < 0) return 0;
  }

  if (need_today > 0) {
    int exact_available = available_stationary_attackers_with_hop_cmp(S, M, P, a, target, chosen_eta, 1);
    if (exact_available < need_today) return 0;
  }
  *eta_out = chosen_eta;
  *need_today_out = need_today;
  return 1;
}

static MAYBE_UNUSED int issue_timed_capture_group_to_target(Actions *a, const GameState *S,
                                               const GameMap *M, const Paths *P,
                                               int target, int *budget,
                                               int *sent_today_out) {
  if (!ENABLE_TIMED_CAPTURE) {
    if (sent_today_out) *sent_today_out = 0;
    return 0;
  }

  const Building *b = find_building_const(S, target);
  if (b == NULL || b->side == M->my_side) return 0;
  if (b->type == BTYPE_HQ && !ENABLE_ENEMY_HQ_ATTACK) return 0;
  if (!is_move_destination_candidate(M, target)) return 0;

  int req = required_attackers_for_enemy_base(S, M, target);
  if (req <= 0 || req >= 1000000) return 0;

  int eta = -1, need_today = 0;
  if (!timed_capture_feasible_eta(S, M, P, a, target, req, &eta, &need_today)) return 0;
  if (*budget < need_today * MOVE_COST) return 0;

  if (sent_today_out) *sent_today_out = 0;
  if (need_today == 0) return 1;

  WarriorId *ids = (WarriorId *)malloc((size_t)S->warriors.len * sizeof(WarriorId));
  int cnt = collect_stationary_attackers_exact_hops(S, M, P, a, target, eta, ids, S->warriors.len);
  if (cnt < need_today) {
    free(ids);
    return 0;
  }

  int sent = 0;
  for (int i = 0; i < need_today; ++i) {
    if (add_move_action(a, S, M, ids[i], target, budget))
      ++sent;
  }
  free(ids);
  if (sent_today_out) *sent_today_out = sent;
  return sent == need_today;
}

static MAYBE_UNUSED int can_start_or_continue_timed_capture(const GameState *S,
                                               const GameMap *M,
                                               const Paths *P,
                                               const Actions *a,
                                               int target, int budget) {
  const Building *b = find_building_const(S, target);
  if (b == NULL || b->side == M->my_side) return 0;
  if (b->type == BTYPE_HQ && !ENABLE_ENEMY_HQ_ATTACK) return 0;
  int req = required_attackers_for_enemy_base(S, M, target);
  if (req <= 0 || req >= 1000000) return 0;
  int eta = -1, need_today = 0;
  if (!timed_capture_feasible_eta(S, M, P, a, target, req, &eta, &need_today)) return 0;
  return budget >= need_today * MOVE_COST;
}

typedef struct {
  WarriorId id;
  int hp;
  int region;
  int hops;
  double dist;
} AttackPoolEntry;

static int cmp_attack_pool_entry(const void *pa, const void *pb) {
  const AttackPoolEntry *a = (const AttackPoolEntry *)pa;
  const AttackPoolEntry *b = (const AttackPoolEntry *)pb;
  if (a->dist < b->dist) return -1;
  if (a->dist > b->dist) return 1;
  if (a->region != b->region) return (a->region > b->region) - (a->region < b->region);
  return (a->id.num > b->id.num) - (a->id.num < b->id.num);
}

static int alive_hp_count(const int *hp, int n) {
  int c = 0;
  for (int i = 0; i < n; ++i)
    if (hp[i] > 0) ++c;
  return c;
}

static MAYBE_UNUSED int total_positive_hp(const int *hp, int n) {
  int s = 0;
  for (int i = 0; i < n; ++i)
    if (hp[i] > 0) s += hp[i];
  return s;
}

static void deal_one_to_weakest(int *hp, int n) {
  int best = -1;
  for (int i = 0; i < n; ++i) {
    if (hp[i] <= 0) continue;
    if (best < 0 || hp[i] < hp[best]) best = i;
  }
  if (best >= 0) --hp[best];
}

static void attacker_deal_one(int *real_def_hp, int real_n, int *dummy_hp,
                              int *building_hp) {
  int best_real = -1;
  for (int i = 0; i < real_n; ++i) {
    if (real_def_hp[i] <= 0) continue;
    if (best_real < 0 || real_def_hp[i] < real_def_hp[best_real]) best_real = i;
  }

  if (best_real >= 0 || *dummy_hp > 0) {
    if (best_real >= 0 && (*dummy_hp <= 0 || real_def_hp[best_real] <= *dummy_hp)) {
      --real_def_hp[best_real];
    } else {
      --(*dummy_hp);
    }
    return;
  }

  if (*building_hp > 0) --(*building_hp);
}

static int combat_simulation_win(const GameState *S, const GameMap *M,
                                 int target, const int *input_attacker_hp,
                                 int attacker_n) {
  const Building *b = find_building_const(S, target);
  if (b == NULL || b->side == M->my_side) return 0;
  if (attacker_n <= 0) return 0;

  int *atk = (int *)malloc((size_t)MAX_COMBAT_SIM_UNITS * sizeof(int));
  int *real_def = (int *)malloc((size_t)MAX_COMBAT_SIM_UNITS * sizeof(int));
  int atk_n = 0, real_n = 0;
  for (int i = 0; i < attacker_n && atk_n < MAX_COMBAT_SIM_UNITS; ++i)
    if (input_attacker_hp[i] > 0) atk[atk_n++] = input_attacker_hp[i];

  int real_hp_sum = 0;
  Side opp = opposite(M->my_side);
  for (int i = 0; i < S->warriors.len && real_n < MAX_COMBAT_SIM_UNITS; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == opp && w->region == target && w->hp > 0) {
      real_def[real_n++] = w->hp;
      real_hp_sum += w->hp;
    }
  }

  int building_hp = max_int(0, b->hp);
  int dummy_hp = (real_hp_sum + building_hp + BASE_DUMMY_HP_DIV - 1) / BASE_DUMMY_HP_DIV;
  int turret = building_turret_power_const(b);

  int train_each_turn = 0;
  int reinf_hp = 0;
  if (b->type == BTYPE_HQ) {
    int hq_level = b->level;
    int opp_income = side_current_income(S, opp);
    train_each_turn = min_int(HQ_LEVELS[hq_level].train_cap, opp_income / TRAIN_COST);
    reinf_hp = HQ_LEVELS[hq_level].warrior_hp;
  }

  int max_days = b->type == BTYPE_HQ ? HQ_SIM_MAX_REINFORCE_DAYS : MAX_CAPTURE_SIM_DAYS;
  for (int day = 0; day < max_days; ++day) {
    if (b->type == BTYPE_HQ && train_each_turn > 0) {
      for (int k = 0; k < train_each_turn && real_n < MAX_COMBAT_SIM_UNITS; ++k)
        real_def[real_n++] = reinf_hp;
    }

    int attacker_attacks = alive_hp_count(atk, atk_n);
    if (attacker_attacks <= 0) {
      free(atk);
      free(real_def);
      return 0;
    }

    for (int k = 0; k < turret; ++k)
      deal_one_to_weakest(atk, atk_n);

    int defender_attacks = alive_hp_count(real_def, real_n);

    for (int k = 0; k < attacker_attacks; ++k)
      attacker_deal_one(real_def, real_n, &dummy_hp, &building_hp);

    for (int k = 0; k < defender_attacks; ++k)
      deal_one_to_weakest(atk, atk_n);

    if (building_hp <= 0 && alive_hp_count(atk, atk_n) > 0) {
      free(atk);
      free(real_def);
      return 1;
    }

    if (alive_hp_count(atk, atk_n) <= 0) {
      free(atk);
      free(real_def);
      return 0;
    }

    if (b->type != BTYPE_HQ && alive_hp_count(real_def, real_n) == 0 && dummy_hp <= 0 &&
        building_hp <= 0) {
      free(atk);
      free(real_def);
      return 1;
    }
  }

  free(atk);
  free(real_def);
  return 0;
}

static int neutral_combat_simulation_win(const GameState *S, const GameMap *M,
                                         int target, const int *input_attacker_hp,
                                         int attacker_n) {
  if (attacker_n <= 0) return 0;
  int atk[MAX_COMBAT_SIM_UNITS];
  int def[MAX_COMBAT_SIM_UNITS];
  int atk_n = 0, def_n = 0;
  for (int i = 0; i < attacker_n && atk_n < MAX_COMBAT_SIM_UNITS; ++i)
    if (input_attacker_hp[i] > 0) atk[atk_n++] = input_attacker_hp[i];

  Side opp = opposite(M->my_side);
  for (int i = 0; i < S->warriors.len && def_n < MAX_COMBAT_SIM_UNITS; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == opp && w->region == target && w->hp > 0)
      def[def_n++] = w->hp;
  }
  if (def_n <= 0) return 0;

  for (int day = 0; day < 10; ++day) {
    int attacker_attacks = alive_hp_count(atk, atk_n);
    int defender_attacks = alive_hp_count(def, def_n);
    if (attacker_attacks <= 0) return 0;
    if (defender_attacks <= 0) return 1;

    for (int k = 0; k < attacker_attacks; ++k)
      deal_one_to_weakest(def, def_n);
    for (int k = 0; k < defender_attacks; ++k)
      deal_one_to_weakest(atk, atk_n);

    if (alive_hp_count(def, def_n) <= 0 && alive_hp_count(atk, atk_n) > 0)
      return 1;
    if (alive_hp_count(atk, atk_n) <= 0) return 0;
  }
  return 0;
}

static int enemy_occupied_neutral_stronghold(const GameState *S,
                                             const GameMap *M, int target) {
  if (!is_stronghold(M, target)) return 0;
  if (find_building_const(S, target) != NULL) return 0;
  return enemy_warrior_count_at(S, M, target) > 0;
}

static int collect_committed_attack_hps(const GameState *S, const GameMap *M,
                                        const Actions *a, int target,
                                        int *hp_out, int maxn) {
  int n = 0;
  for (int i = 0; i < S->warriors.len && n < maxn; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (w->region == target || (w->state == WSTATE_MOVING && w->target == target))
      hp_out[n++] = max_int(1, w->hp);
  }
  for (int i = 0; i < a->moves.len && n < maxn; ++i) {
    if (a->moves.data[i].target != target) continue;
    const Warrior *w = find_warrior_const(S, a->moves.data[i].id);
    if (w == NULL || w->id.side != M->my_side) continue;
    hp_out[n++] = max_int(1, w->hp);
  }
  return n;
}

static int collect_attack_pool(const GameState *S, const GameMap *M,
                               const Paths *P, const Actions *a, int target,
                               AttackPoolEntry *pool, int maxn) {
  int n = 0;
  const Building *target_b = find_building_const(S, target);
  int target_enemy_building = (target_b != NULL && target_b->side != M->my_side);
  int target_neutral_enemy = enemy_occupied_neutral_stronghold(S, M, target);
  if (!target_enemy_building && !target_neutral_enemy) return 0;

  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *src = &S->buildings.data[bi];
    if (src->side != M->my_side) continue;
    if (src->region == target) continue;
    if (P->nxt[src->region][target] == -1) continue;
    if (full_path_enemy_blocked(S, M, P, src->region, target, 1)) continue;
    if (target_enemy_building &&
        !attack_path_first_enemy_is_target(S, M, P, src->region, target)) continue;
    int allow = movable_attack_surplus_from_region(S, M, a, src->region);
    if (allow <= 0) continue;
    int taken = 0;
    for (int wi = S->warriors.len - 1; wi >= 0 && taken < allow && n < maxn; --wi) {
      const Warrior *w = &S->warriors.data[wi];
      if (w->id.side != M->my_side || w->region != src->region) continue;
      if (w->state != WSTATE_STATIONARY) continue;
      if (action_has_move_warrior(a, w->id)) continue;
      AttackPoolEntry e;
      e.id = w->id;
      e.hp = max_int(1, w->hp);
      e.region = w->region;
      e.hops = path_hops_between(P, w->region, target);
      e.dist = P->dist[w->region][target];
      pool[n++] = e;
      ++taken;
    }
  }
  qsort(pool, (size_t)n, sizeof(AttackPoolEntry), cmp_attack_pool_entry);
  return n;
}

static int region_on_my_supply_path(const GameState *S, const GameMap *M,
                                    const Paths *P, int region) {
  if (P == NULL) return 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side || b->region == M->my_hq) continue;
    if (P->nxt[M->my_hq][b->region] == -1) continue;
    int cur = M->my_hq;
    int guard = 0;
    while (cur != b->region && guard++ <= P->N + 5) {
      cur = P->nxt[cur][b->region];
      if (cur < 0) break;
      if (cur == region) return 1;
    }
  }
  return 0;
}

static int stack_cleanup_required_attackers(const GameState *S,
                                            const GameMap *M,
                                            const Paths *P,
                                            int region) {
  int enemy_cnt = enemy_projected_stack_count_at(S, M, P, region);
  if (enemy_cnt < STACK_CLEANUP_MIN_ENEMY) return 1000000;

  Side opp = opposite(M->my_side);
  int my_present = count_warriors_at(S, M->my_side, region);
  const Building *b = find_building_const(S, region);
  int enemy_turret = (b != NULL && b->side == opp) ? building_turret_power_const(b) : 0;
  int my_turret = (b != NULL && b->side == M->my_side) ? building_turret_power_const(b) : 0;

  int need_total = (enemy_cnt * STACK_CLEANUP_RATIO_NUM + STACK_CLEANUP_RATIO_DEN - 1)
                 / STACK_CLEANUP_RATIO_DEN;
  need_total += enemy_turret + STACK_CLEANUP_EXTRA_MARGIN;
  need_total -= my_turret / 2;
  if (need_total < enemy_cnt + 1) need_total = enemy_cnt + 1;

  int need_today = max_int(0, need_total - my_present);
  if (need_today > 0 && need_today < STACK_CLEANUP_MIN_WAVE)
    need_today = STACK_CLEANUP_MIN_WAVE;
  return need_today;
}

static int stack_cleanup_already_ordered(const Actions *a, int target) {
  for (int i = 0; i < a->moves.len; ++i)
    if (a->moves.data[i].target == target) return 1;
  return 0;
}

static int choose_stack_cleanup_target(const GameState *S, const GameMap *M,
                                       const Paths *P, const Actions *a,
                                       int budget, int *target_out,
                                       int *need_out) {
  int best = -1;
  int best_need = 0;
  double best_score = -INFINITY;

  for (int r = 0; r < M->N; ++r) {
    int enemy_cnt = enemy_projected_stack_count_at(S, M, P, r);
    if (enemy_cnt < STACK_CLEANUP_MIN_ENEMY) continue;
    if (!region_on_my_supply_path(S, M, P, r)) continue;
    if (stack_cleanup_already_ordered(a, r)) continue;

    int need = stack_cleanup_required_attackers(S, M, P, r);
    if (need <= 0 || need >= 1000000) continue;
    if (budget < need * MOVE_COST) continue;

    int best_eta = INF_HOPS;
    for (int h = 1; h <= STACK_CLEANUP_MAX_TARGET_ETA; ++h) {
      int exact = available_stationary_attackers_with_hop_cmp(S, M, P, a, r, h, 1);
      if (exact >= need) {
        best_eta = h;
        break;
      }
    }
    if (best_eta >= INF_HOPS) continue;

    double hq_d = P->dist[M->my_hq][r];
    double score = 100000.0 * enemy_cnt - 2500.0 * best_eta - hq_d;
    if (score > best_score) {
      best_score = score;
      best = r;
      best_need = need;
    }
  }

  if (best < 0) return 0;
  *target_out = best;
  *need_out = best_need;
  return 1;
}

static int issue_stack_cleanup(Actions *a, const GameState *S,
                               const GameMap *M, const Paths *P,
                               int *budget) {
  if (!ENABLE_STACK_ONLY_CLEANUP) return 0;
  int target = -1, need = 0;
  if (!choose_stack_cleanup_target(S, M, P, a, *budget, &target, &need)) return 0;

  AttackPoolEntry *pool = (AttackPoolEntry *)malloc((size_t)MAX_COMBAT_SIM_UNITS * sizeof(AttackPoolEntry));
  if (pool == NULL) return 0;
  int pool_n = collect_attack_pool(S, M, P, a, target, pool, MAX_COMBAT_SIM_UNITS);
  int sent = 0;
  for (int h = 1; h <= STACK_CLEANUP_MAX_TARGET_ETA && sent == 0; ++h) {
    int exact = 0;
    for (int i = 0; i < pool_n; ++i)
      if (pool[i].hops == h) ++exact;
    if (exact < need) continue;

    int to_send = min_int(exact, need + STACK_CLEANUP_MAX_SEND_EXTRA);
    if (*budget < to_send * MOVE_COST) continue;
    for (int i = 0; i < pool_n && sent < to_send; ++i) {
      if (pool[i].hops != h) continue;
      if (add_move_action_ex_stack_flags(a, S, M, P, pool[i].id, target,
                                         budget, MOVE_FLAG_ALLOW_DANGER_TARGET))
        ++sent;
    }
  }
  free(pool);
  return sent;
}


static int initial_synced_capture_plan(const GameState *S, const GameMap *M,
                                       const Paths *P, const Actions *a,
                                       int target, WarriorId *send_today,
                                       int max_send, int *eta_out,
                                       int *need_today_out) {
  if (!ENABLE_INITIAL_SYNC_CAPTURE) return 0;

  const Building *b = find_building_const(S, target);
  if (b == NULL || b->side == M->my_side) return 0;
  if (b->type == BTYPE_HQ && !ENABLE_ENEMY_HQ_ATTACK) return 0;
  if (!is_move_destination_candidate(M, target)) return 0;

  int hp[MAX_COMBAT_SIM_UNITS];
  int committed = collect_committed_attack_hps(S, M, a, target, hp, MAX_COMBAT_SIM_UNITS);

  /* Synchronize only when a new attack is being started.  Once at least one
     warrior is already on the target or moving to it, v13 no-drip attack does
     not add follow-up reinforcements one by one. */
  if (committed > 0) return 0;

  AttackPoolEntry *pool = (AttackPoolEntry *)malloc((size_t)MAX_COMBAT_SIM_UNITS * sizeof(AttackPoolEntry));
  if (pool == NULL) return 0;
  int pool_n = collect_attack_pool(S, M, P, a, target, pool, MAX_COMBAT_SIM_UNITS);
  if (pool_n <= 0) {
    free(pool);
    return 0;
  }

  int max_h = 0;
  for (int i = 0; i < pool_n; ++i)
    if (pool[i].hops < INF_HOPS) max_h = max_int(max_h, pool[i].hops);

  for (int h = 1; h <= max_h; ++h) {
    WarriorId chosen[MAX_COMBAT_SIM_UNITS];
    int chosen_n = 0;
    int n = 0;

    /* Strict synchronized wave: evaluate only warriors that can arrive on the
       same turn.  Older code also counted closer warriors in the simulation,
       then sent only the farther ETA group; that created staggered, one-by-one
       infiltration and follow-up losses. */
    for (int i = 0; i < pool_n && n < MAX_COMBAT_SIM_UNITS; ++i) {
      if (pool[i].hops != h) continue;
      hp[n++] = pool[i].hp;
      if (chosen_n < MAX_COMBAT_SIM_UNITS)
        chosen[chosen_n++] = pool[i].id;
    }

    if (ENABLE_NO_DRIP_ATTACK && chosen_n < MIN_ATTACK_WAVE_UNITS) continue;
    if (chosen_n <= 0) continue;

    if (combat_simulation_win(S, M, target, hp, n)) {
      if (eta_out) *eta_out = h;
      if (need_today_out) *need_today_out = chosen_n;
      if (send_today != NULL && max_send > 0) {
        int lim = min_int(chosen_n, max_send);
        for (int k = 0; k < lim; ++k)
          send_today[k] = chosen[k];
      }
      free(pool);
      return chosen_n > 0;
    }
  }

  free(pool);
  return 0;
}

static int immediate_capture_new_attackers_needed(const GameState *S, const GameMap *M,
                                                  const Paths *P, const Actions *a,
                                                  int target) {
  const Building *b = find_building_const(S, target);
  if (b == NULL || b->side == M->my_side) return 1000000;
  if (b->type == BTYPE_HQ && !ENABLE_ENEMY_HQ_ATTACK) return 1000000;
  if (!is_move_destination_candidate(M, target)) return 1000000;

  int hp[MAX_COMBAT_SIM_UNITS];
  int committed = collect_committed_attack_hps(S, M, a, target, hp, MAX_COMBAT_SIM_UNITS);
  if (combat_simulation_win(S, M, target, hp, committed)) return 0;

  /* No-drip rule: if an attack is already committed but is not currently
     winning in the simulation, do not add a one-by-one follow-up.  Wait until
     the old wave resolves and look for a fresh winning wave later. */
  if (ENABLE_NO_DRIP_ATTACK && committed > 0) return 1000000;

  AttackPoolEntry *pool = (AttackPoolEntry *)malloc((size_t)MAX_COMBAT_SIM_UNITS * sizeof(AttackPoolEntry));
  int pool_n = collect_attack_pool(S, M, P, a, target, pool, MAX_COMBAT_SIM_UNITS);
  for (int k = 1; k <= pool_n && committed + k < MAX_COMBAT_SIM_UNITS; ++k) {
    hp[committed + k - 1] = pool[k - 1].hp;
    if (combat_simulation_win(S, M, target, hp, committed + k)) {
      int need = k;
      if (ENABLE_NO_DRIP_ATTACK && need < MIN_ATTACK_WAVE_UNITS)
        need = MIN_ATTACK_WAVE_UNITS;
      if (pool_n < need) {
        free(pool);
        return 1000000;
      }
      free(pool);
      return need;
    }
  }
  free(pool);
  return 1000000;
}

static int capture_new_attackers_needed(const GameState *S, const GameMap *M,
                                        const Paths *P, const Actions *a,
                                        int target) {
  int eta = -1, need_today = 0;
  if (initial_synced_capture_plan(S, M, P, a, target, NULL, 0, &eta, &need_today))
    return need_today;
  return immediate_capture_new_attackers_needed(S, M, P, a, target);
}

static int capture_candidate_eta(const GameState *S, const GameMap *M,
                                 const Paths *P, const Actions *a,
                                 int target) {
  int eta = -1, need_today = 0;
  if (initial_synced_capture_plan(S, M, P, a, target, NULL, 0, &eta, &need_today))
    return eta;
  int need = immediate_capture_new_attackers_needed(S, M, P, a, target);
  return (need >= 0 && need < 1000000) ? 0 : INF_HOPS;
}

static int can_capture_enemy_building_now(const GameState *S, const GameMap *M,
                                          const Paths *P, const Actions *a,
                                          int region, int budget) {
  int need = capture_new_attackers_needed(S, M, P, a, region);
  if (need <= 0 || need >= 1000000) return 0;
  return budget >= need * MOVE_COST;
}

static int issue_capture_group_to_target(Actions *a, const GameState *S,
                                         const GameMap *M, const Paths *P,
                                         int target, int *budget) {
  WarriorId sync_ids[MAX_COMBAT_SIM_UNITS];
  int sync_eta = -1, sync_need = 0;
  if (initial_synced_capture_plan(S, M, P, a, target, sync_ids,
                                  MAX_COMBAT_SIM_UNITS, &sync_eta, &sync_need)) {
    (void)sync_eta;
    if (sync_need <= 0 || sync_need >= 1000000) return 0;
    if (*budget < sync_need * MOVE_COST) return 0;
    int sent = 0;
    for (int i = 0; i < sync_need; ++i)
      if (add_move_action(a, S, M, sync_ids[i], target, budget)) ++sent;
    return sent == sync_need;
  }

  /* With no-drip + strict sync enabled, do not fall back to the legacy
     immediate plan.  That plan picks the nearest `need` attackers, often with
     different ETAs, so they enter the target one by one.  If no same-ETA wave
     wins, skip this target for now. */
  if (ENABLE_NO_DRIP_ATTACK && ENABLE_STRICT_SAME_ETA_ATTACK_WAVE) return 0;

  int need = immediate_capture_new_attackers_needed(S, M, P, a, target);
  if (need == 0) return 1;
  if (need < 0 || need >= 1000000) return 0;
  if (*budget < need * MOVE_COST) return 0;

  AttackPoolEntry *pool = (AttackPoolEntry *)malloc((size_t)MAX_COMBAT_SIM_UNITS * sizeof(AttackPoolEntry));
  int pool_n = collect_attack_pool(S, M, P, a, target, pool, MAX_COMBAT_SIM_UNITS);
  if (pool_n < need) {
    free(pool);
    return 0;
  }

  int sent = 0;
  for (int i = 0; i < pool_n && sent < need; ++i) {
    if (add_move_action(a, S, M, pool[i].id, target, budget)) ++sent;
  }
  free(pool);
  return sent == need;
}


/* Anchor-routed version of the existing capture.  The caller has already used
   the original choose/can-capture logic, so this function never picks a new
   target on its own. */
/* Anchor-route gating.  This counts ordinary owned BASEs, not HQ. */
static int anchor_route_my_base_count(const GameState *S, const GameMap *M) {
  int n = 0;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side == M->my_side && b->type == BTYPE_BASE) ++n;
  }
  return n;
}

static int anchor_route_required_owned_bases(const GameMap *M) {
  int den = ANCHOR_ROUTE_START_BASES_A_DEN;
  if (den <= 0) den = 1;
  long long num = (long long)ANCHOR_ROUTE_START_BASES_A_NUM * (long long)M->K;
  int ax = 0;
  if (num <= 0) ax = 0;
  else ax = (int)((num + den - 1) / den); /* ceil(a*x) for integer ratio */
  int req = ax + ANCHOR_ROUTE_START_BASES_B;
  if (req < ANCHOR_ROUTE_MIN_OWNED_BASES_TO_ATTACK)
    req = ANCHOR_ROUTE_MIN_OWNED_BASES_TO_ATTACK;
  if (req < 0) req = 0;
  return req;
}

static int anchor_route_gate_open(const GameState *S, const GameMap *M) {
  return anchor_route_my_base_count(S, M) >= anchor_route_required_owned_bases(M);
}

static int anchor_route_anchor_valid_for_target(const GameState *S,
                                                const GameMap *M,
                                                const Paths *P,
                                                int anchor, int target) {
  if (anchor < 0 || anchor >= M->N) return 0;
  const Building *b = find_building_const(S, anchor);
  if (b == NULL || b->side != M->my_side) return 0;
  if (ANCHOR_ROUTE_USE_ONLY_BASE_ANCHOR && b->type != BTYPE_BASE) return 0;
  if (anchor == target) return 0;
  if (region_has_enemy_warrior(S, M, anchor)) return 0;
  if (P->nxt[anchor][target] == -1) return 0;
  if (!attack_path_first_enemy_is_target(S, M, P, anchor, target)) return 0;
  return 1;
}

static int anchor_route_committed_to_anchor(const GameState *S, const GameMap *M,
                                            const Actions *a, int anchor) {
  int n = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->region == anchor) ++n;
    else if (w->state == WSTATE_MOVING && w->target == anchor) ++n;
  }
  for (int i = 0; i < a->moves.len; ++i)
    if (a->moves.data[i].target == anchor) ++n;
  return n;
}

static int anchor_route_keep_at_anchor(const GameState *S, const GameMap *M,
                                       const Actions *a, int anchor) {
  /* Units counted here are NOT attackers.  They are the workers/garrison that
     must remain at the anchor.  Staging must therefore gather
     attackers + keep, otherwise we can get stuck with e.g. 4 total units at the
     anchor but only 3 attackable units after keeping one worker behind. */
  return max_int(ANCHOR_ROUTE_LEAVE_AT_ANCHOR,
                 planned_work_cap_at(S, M, a, anchor));
}

static int anchor_route_attackable_at_anchor_count(const GameState *S,
                                                   const GameMap *M,
                                                   const Actions *a,
                                                   int anchor) {
  int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, anchor);
  int keep = anchor_route_keep_at_anchor(S, M, a, anchor);
  return max_int(0, remaining - keep);
}

static int anchor_route_stack_at_anchor(const GameState *S, const GameMap *M,
                                        const Actions *a, int anchor,
                                        int *hp_out, WarriorId *ids_out, int maxn) {
  int allow = anchor_route_attackable_at_anchor_count(S, M, a, anchor);
  int n = 0;
  for (int wi = S->warriors.len - 1; wi >= 0 && n < maxn && allow > 0; --wi) {
    const Warrior *w = &S->warriors.data[wi];
    if (w->id.side != M->my_side || w->region != anchor) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    hp_out[n] = max_int(1, w->hp);
    if (ids_out != NULL) ids_out[n] = w->id;
    ++n;
    --allow;
  }
  return n;
}

static int anchor_route_source_movable(const GameState *S, const GameMap *M,
                                       const Actions *a, int region) {
  if (!planned_my_building(S, M, a, region)) return 0;
  if (region_has_enemy_warrior(S, M, region)) return 0;
  int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, region);
  int cap = planned_work_cap_at(S, M, a, region);
  int keep = cap + ANCHOR_ROUTE_KEEP_EXTRA_AT_SOURCE;
  int movable = max_int(0, remaining - keep);
  int stationary = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side || w->region != region) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    ++stationary;
  }
  return min_int(movable, stationary);
}

static double anchor_route_supply_score(const GameState *S, const GameMap *M,
                                        const Paths *P, const Actions *a,
                                        int anchor) {
  double score = 0.0;
  int used = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *src = &S->buildings.data[bi];
    if (src->side != M->my_side) continue;
    if (src->region == anchor) continue;
    if (P->nxt[src->region][anchor] == -1) continue;
    int m = anchor_route_source_movable(S, M, a, src->region);
    if (m <= 0) continue;
    score += P->dist[src->region][anchor] * m;
    used += m;
  }
  if (used == 0) return 1000000000.0;
  return score / used;
}

static int choose_anchor_for_existing_attack_target(const GameState *S,
                                                    const GameMap *M,
                                                    const Paths *P,
                                                    const Actions *a,
                                                    int target, int *anchor_out) {
  if (ANCHOR_ROUTE_STICKY_ANCHOR &&
      anchor_route_anchor_valid_for_target(S, M, P, g_anchor_route_stage_region, target)) {
    *anchor_out = g_anchor_route_stage_region;
    return 1;
  }

  double best = INFINITY;
  int best_anchor = -1;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    if (ANCHOR_ROUTE_USE_ONLY_BASE_ANCHOR && b->type != BTYPE_BASE) continue;
    if (b->region == target) continue;
    if (region_has_enemy_warrior(S, M, b->region)) continue;
    if (P->nxt[b->region][target] == -1) continue;
    if (!attack_path_first_enemy_is_target(S, M, P, b->region, target)) continue;

    double score;
    if (ANCHOR_ROUTE_ANCHOR_MODE == 1) {
      score = P->dist[b->region][M->opp_hq] + 0.001 * P->dist[b->region][target];
    } else if (ANCHOR_ROUTE_ANCHOR_MODE == 2) {
      double my_d = P->dist[M->my_hq][b->region];
      double opp_d = P->dist[M->opp_hq][b->region];
      score = fabs(my_d - opp_d) + 0.01 * P->dist[b->region][target];
    } else if (ANCHOR_ROUTE_ANCHOR_MODE == 3) {
      score = anchor_route_supply_score(S, M, P, a, b->region) +
              0.25 * P->dist[b->region][target];
    } else {
      score = P->dist[b->region][target];
    }
    score += b->region * 0.000001;
    if (score < best) {
      best = score;
      best_anchor = b->region;
    }
  }
  if (best_anchor < 0) return 0;
  g_anchor_route_stage_region = best_anchor;
  *anchor_out = best_anchor;
  return 1;
}

static int pick_anchor_route_stage_warrior(const GameState *S, const GameMap *M,
                                           const Paths *P, const Actions *a,
                                           int anchor, WarriorId *out) {
  double best = INFINITY;
  WarriorId best_id = {M->my_side, -1};
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *src = &S->buildings.data[bi];
    if (src->side != M->my_side) continue;
    if (src->region == anchor) continue;
    if (P->nxt[src->region][anchor] == -1) continue;
    if (anchor_route_source_movable(S, M, a, src->region) <= 0) continue;
    for (int wi = S->warriors.len - 1; wi >= 0; --wi) {
      const Warrior *w = &S->warriors.data[wi];
      if (w->id.side != M->my_side || w->region != src->region) continue;
      if (w->state != WSTATE_STATIONARY) continue;
      if (action_has_move_warrior(a, w->id)) continue;
      double score = P->dist[src->region][anchor] + 0.001 * w->id.num;
      if (score < best) {
        best = score;
        best_id = w->id;
      }
      break;
    }
  }
  if (best_id.num < 0) return 0;
  *out = best_id;
  return 1;
}

static int issue_anchor_routed_capture_group_to_target(Actions *a,
                                                       const GameState *S,
                                                       const GameMap *M,
                                                       const Paths *P,
                                                       int target, int *budget,
                                                       int turn) {
#if ENABLE_ANCHOR_ROUTE_ATTACKS
  if (turn < ANCHOR_ROUTE_START_TURN) return 0;
  if (!anchor_route_gate_open(S, M)) return 0;
  const Building *tb = find_building_const(S, target);
  if (tb == NULL || tb->side == M->my_side) return 0;

  int need = capture_new_attackers_needed(S, M, P, a, target);
  if (need == 0) return 1;
  if (need < 0 || need >= 1000000) return 0;
  if (need < ANCHOR_ROUTE_MIN_ATTACKERS) need = ANCHOR_ROUTE_MIN_ATTACKERS;
  if (need > ANCHOR_ROUTE_MAX_STACK) return 0;

  int anchor = -1;
  if (!choose_anchor_for_existing_attack_target(S, M, P, a, target, &anchor)) return 0;

  int hp[MAX_COMBAT_SIM_UNITS];
  WarriorId ids[MAX_COMBAT_SIM_UNITS];
  int stack_n = anchor_route_stack_at_anchor(S, M, a, anchor, hp, ids, MAX_COMBAT_SIM_UNITS);

  int launch_n = 0;
  int max_try = min_int(stack_n, ANCHOR_ROUTE_MAX_STACK);
  for (int k = need; k <= max_try; ++k) {
    if (k >= ANCHOR_ROUTE_MIN_ATTACKERS && combat_simulation_win(S, M, target, hp, k)) {
      launch_n = k;
      break;
    }
  }
  if (launch_n > 0) {
    int move_cost = planned_my_building(S, M, a, target) ? 0 : MOVE_COST;
    if (*budget < move_cost * launch_n) return 0;
    int sent = 0;
    for (int i = 0; i < launch_n; ++i)
      if (add_move_action(a, S, M, ids[i], target, budget)) ++sent;
    if (sent == launch_n) {
      g_anchor_route_last_attack_anchor = anchor;
      g_anchor_route_last_attack_target = target;
      return 1;
    }
    return 0;
  }

  int committed = anchor_route_committed_to_anchor(S, M, a, anchor);
  int desired_attackers = need;
  if (desired_attackers < ANCHOR_ROUTE_MIN_ATTACKERS)
    desired_attackers = ANCHOR_ROUTE_MIN_ATTACKERS;
  if (desired_attackers > ANCHOR_ROUTE_MAX_STACK)
    desired_attackers = ANCHOR_ROUTE_MAX_STACK;
  int desired_total_at_anchor = desired_attackers +
      anchor_route_keep_at_anchor(S, M, a, anchor);
  if (committed >= desired_total_at_anchor) return 0;

  int did = 0;
  int staged = 0;
  while (committed < desired_total_at_anchor && staged < ANCHOR_ROUTE_MAX_STAGE_PER_TURN) {
    WarriorId id;
    if (!pick_anchor_route_stage_warrior(S, M, P, a, anchor, &id)) break;
    if (!add_move_action(a, S, M, id, anchor, budget)) break;
    ++did;
    ++staged;
    ++committed;
  }
  return did;
#else
  (void)a; (void)S; (void)M; (void)P; (void)target; (void)budget; (void)turn;
  return 0;
#endif
}

static int choose_anchor_route_return_anchor(const GameState *S,
                                             const GameMap *M,
                                             const Paths *P,
                                             int from_region,
                                             int *anchor_out) {
  int sticky = g_anchor_route_last_attack_anchor;
  const Building *sb = find_building_const(S, sticky);
  if (sb != NULL && sb->side == M->my_side && sticky != from_region &&
      P->nxt[from_region][sticky] != -1 && !region_has_enemy_warrior(S, M, sticky)) {
    *anchor_out = sticky;
    return 1;
  }

  int best = -1;
  int best_d = INF_HOPS;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    if (b->region == from_region) continue;
    if (region_has_enemy_warrior(S, M, b->region)) continue;
    if (ANCHOR_ROUTE_USE_ONLY_BASE_ANCHOR && b->type != BTYPE_BASE) continue;
    if (P->nxt[from_region][b->region] == -1) continue;
    int d = P->dist[from_region][b->region];
    if (d < best_d || (d == best_d && b->region < best)) {
      best_d = d;
      best = b->region;
    }
  }
  if (best < 0 && from_region != M->my_hq && P->nxt[from_region][M->my_hq] != -1)
    best = M->my_hq;
  if (best < 0) return 0;
  *anchor_out = best;
  return 1;
}

static int issue_anchor_route_capture_cleanup(Actions *a,
                                              const GameState *S,
                                              const GameMap *M,
                                              const Paths *P,
                                              int *budget) {
#if ENABLE_ANCHOR_ROUTE_ATTACKS
  if (!ANCHOR_ROUTE_BUILD_CAPTURED_FIRST && !ANCHOR_ROUTE_RETURN_AFTER_CAPTURE) return 0;
  int target = g_anchor_route_last_attack_target;
  if (target < 0 || target >= M->N) return 0;
  if (!is_stronghold(M, target)) return 0;
  if (region_has_enemy_warrior(S, M, target)) return 0;
  int own_here = count_warriors_at(S, M->my_side, target);
  if (own_here <= 0) return 0;

  const Building *b = find_building_const(S, target);
  if (b == NULL) {
    if (!ANCHOR_ROUTE_BUILD_CAPTURED_FIRST) return 0;
    if (*budget < BASE_LEVELS[1].cost) return 0;
    if (!legal_build_neutral_now(S, M, target)) return 0;
    if (!add_upgrade_action(a, target)) return 0;
    *budget -= BASE_LEVELS[1].cost;
    return 1;
  }

  if (!ANCHOR_ROUTE_RETURN_AFTER_CAPTURE) return 0;
  if (b->side != M->my_side) return 0;

  int anchor = -1;
  if (!choose_anchor_route_return_anchor(S, M, P, target, &anchor)) return 0;
  int leave = ANCHOR_ROUTE_LEAVE_ON_CAPTURE;
  if (leave < 1) leave = 1;
  int stationary = 0;
  for (int wi = 0; wi < S->warriors.len; ++wi) {
    const Warrior *w = &S->warriors.data[wi];
    if (w->id.side != M->my_side || w->region != target) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    ++stationary;
  }
  if (stationary <= leave) return 0;

  int did = 0;
  int moved = 0;
  for (int wi = 0; wi < S->warriors.len && stationary - moved > leave; ++wi) {
    const Warrior *w = &S->warriors.data[wi];
    if (w->id.side != M->my_side || w->region != target) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (add_move_action(a, S, M, w->id, anchor, budget)) {
      ++did;
      ++moved;
    }
  }
  return did;
#else
  (void)a; (void)S; (void)M; (void)P; (void)budget;
  return 0;
#endif
}

static int anchor_route_move_is_offensive(const GameState *S, const GameMap *M,
                                          const Move *mv) {
  const Building *tb = find_building_const(S, mv->target);
  if (tb != NULL)
    return tb->side != M->my_side;
  return region_has_enemy_warrior(S, M, mv->target);
}

static int anchor_route_move_source_region(const GameState *S, const Move *mv) {
  const Warrior *w = find_warrior_const(S, mv->id);
  return w != NULL ? w->region : -1;
}

static void sanitize_anchor_route_offensive_moves(Actions *a, const GameState *S,
                                                  const GameMap *M) {
#if ENABLE_ANCHOR_ROUTE_ATTACKS && ANCHOR_ROUTE_STRICT_OFFENSE_ONLY
  if (a->moves.len <= 0) return;
  int *keep = (int *)malloc((size_t)a->moves.len * sizeof(int));
  if (keep == NULL) return;
  for (int i = 0; i < a->moves.len; ++i) keep[i] = 1;

  for (int i = 0; i < a->moves.len; ++i) {
    Move *mi = &a->moves.data[i];
    if (!anchor_route_move_is_offensive(S, M, mi)) continue;
    int src_i = anchor_route_move_source_region(S, mi);

    int same_anchor_group = 0;
    for (int j = 0; j < a->moves.len; ++j) {
      Move *mj = &a->moves.data[j];
      if (mj->target != mi->target) continue;
      if (!anchor_route_move_is_offensive(S, M, mj)) continue;
      int src_j = anchor_route_move_source_region(S, mj);
      if (src_j == g_anchor_route_stage_region && src_j == src_i)
        ++same_anchor_group;
    }

    if (src_i != g_anchor_route_stage_region ||
        same_anchor_group < ANCHOR_ROUTE_MIN_ATTACKERS)
      keep[i] = 0;
  }

  int out = 0;
  for (int i = 0; i < a->moves.len; ++i)
    if (keep[i]) a->moves.data[out++] = a->moves.data[i];
  a->moves.len = out;
  free(keep);
#else
  (void)a; (void)S; (void)M;
#endif
}

static int neutral_capture_new_attackers_needed(const GameState *S,
                                                const GameMap *M,
                                                const Paths *P,
                                                const Actions *a,
                                                int target) {
  if (!enemy_occupied_neutral_stronghold(S, M, target)) return 1000000;
  if (!is_move_destination_candidate(M, target)) return 1000000;

  int hp[MAX_COMBAT_SIM_UNITS];
  int committed = collect_committed_attack_hps(S, M, a, target, hp, MAX_COMBAT_SIM_UNITS);
  if (neutral_combat_simulation_win(S, M, target, hp, committed)) return 0;
  if (ENABLE_NO_DRIP_ATTACK && committed > 0) return 1000000;

  AttackPoolEntry *pool = (AttackPoolEntry *)malloc((size_t)MAX_COMBAT_SIM_UNITS * sizeof(AttackPoolEntry));
  int pool_n = collect_attack_pool(S, M, P, a, target, pool, MAX_COMBAT_SIM_UNITS);
  for (int k = 1; k <= pool_n && committed + k < MAX_COMBAT_SIM_UNITS; ++k) {
    hp[committed + k - 1] = pool[k - 1].hp;
    if (neutral_combat_simulation_win(S, M, target, hp, committed + k)) {
      int need = k;
      if (need < MIN_ATTACK_WAVE_UNITS) need = MIN_ATTACK_WAVE_UNITS;
      if (pool_n < need) {
        free(pool);
        return 1000000;
      }
      free(pool);
      return need;
    }
  }
  free(pool);
  return 1000000;
}

static int can_capture_enemy_occupied_neutral_now(const GameState *S,
                                                  const GameMap *M,
                                                  const Paths *P,
                                                  const Actions *a,
                                                  int target, int budget) {
  int need = neutral_capture_new_attackers_needed(S, M, P, a, target);
  if (need <= 0 || need >= 1000000) return 0;
  return budget >= need * MOVE_COST;
}

static int issue_neutral_occupied_capture_group_to_target(Actions *a,
                                                          const GameState *S,
                                                          const GameMap *M,
                                                          const Paths *P,
                                                          int target,
                                                          int *budget) {
  int need = neutral_capture_new_attackers_needed(S, M, P, a, target);
  if (need == 0) return 1;
  if (need < MIN_ATTACK_WAVE_UNITS || need >= 1000000) return 0;
  if (*budget < need * MOVE_COST) return 0;

  AttackPoolEntry *pool = (AttackPoolEntry *)malloc((size_t)MAX_COMBAT_SIM_UNITS * sizeof(AttackPoolEntry));
  int pool_n = collect_attack_pool(S, M, P, a, target, pool, MAX_COMBAT_SIM_UNITS);
  if (pool_n < need) {
    free(pool);
    return 0;
  }

  int sent = 0;
  for (int i = 0; i < pool_n && sent < need; ++i) {
    if (add_move_action_ex_stack_flags(a, S, M, P, pool[i].id, target,
                                       budget, MOVE_FLAG_ALLOW_DANGER_TARGET))
      ++sent;
  }
  free(pool);
  return sent == need;
}

static int choose_enemy_occupied_neutral_target(const GameState *S,
                                                const GameMap *M,
                                                const Paths *P,
                                                const Actions *a, int budget,
                                                int *target_out) {
  double best_score = INFINITY;
  int best_target = -1;
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (!enemy_occupied_neutral_stronghold(S, M, r)) continue;
    if (planned_moves_to_region(a, r) > 0) continue;
    if (!can_capture_enemy_occupied_neutral_now(S, M, P, a, r, budget)) continue;
    double d = P->dist[M->my_hq][r];
    double forward = M->my_side == SIDE_LEFT ? r : (M->N - 1 - r);
    double score = d + 0.001 * forward;
    if (score < best_score) {
      best_score = score;
      best_target = r;
    }
  }
  if (best_target < 0) return 0;
  *target_out = best_target;
  return 1;
}

static int issue_enemy_occupied_neutral_captures(Actions *a,
                                                 const GameState *S,
                                                 const GameMap *M,
                                                 const Paths *P,
                                                 int *budget) {
#if ENABLE_ANCHOR_ROUTE_ATTACKS && ANCHOR_ROUTE_STRICT_OFFENSE_ONLY
  /* Occupied neutral attacks are legacy direct attacks and may send a small
     group from arbitrary regions.  In strict anchor mode, block them so every
     offensive move is anchor-routed with the min-stack gate. */
  (void)a; (void)S; (void)M; (void)P; (void)budget;
  return 0;
#else
  int did = 0;
  while (1) {
    int target = -1;
    if (!choose_enemy_occupied_neutral_target(S, M, P, a, *budget, &target)) break;
    if (!issue_neutral_occupied_capture_group_to_target(a, S, M, P, target, budget)) break;
    did = 1;
  }
  return did;
#endif
}

static MAYBE_UNUSED int issue_priority_enemy_hq_attack(Actions *a, const GameState *S,
                                          const GameMap *M, const Paths *P,
                                          int *budget) {
  const Building *hq = find_building_const(S, M->opp_hq);
  if (hq == NULL || hq->side == M->my_side) return 0;
  if (!can_capture_enemy_building_now(S, M, P, a, M->opp_hq, *budget)) return 0;
#if ENABLE_ANCHOR_ROUTE_ATTACKS
  return issue_anchor_routed_capture_group_to_target(a, S, M, P, M->opp_hq, budget, g_current_turn);
#else
  return issue_capture_group_to_target(a, S, M, P, M->opp_hq, budget);
#endif
}

static MAYBE_UNUSED int choose_capturable_enemy_base(const GameState *S, const GameMap *M,
                                        const Paths *P, const Actions *a,
                                        int budget, int *target_out) {
  if (!ENABLE_ENEMY_BASE_CAPTURE) return 0;
  double best_score = INFINITY;
  int best_target = -1;

  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side == M->my_side) continue;
    if (b->type == BTYPE_HQ && !ENABLE_ENEMY_HQ_ATTACK) continue;
    if (planned_moves_to_region(a, b->region) > 0) continue;
    if (!can_capture_enemy_building_now(S, M, P, a, b->region, budget)) continue;

    int eta = capture_candidate_eta(S, M, P, a, b->region);
    if (eta >= INF_HOPS) continue;
    double d = P->dist[M->my_hq][b->region];
    double forward = M->my_side == SIDE_LEFT ? b->region : (M->N - 1 - b->region);

    /* v10: among capturable enemy bases, prefer the attack whose first
       synchronized wave reaches the target soonest. */
    double hq_bonus = (b->type == BTYPE_HQ) ? -500000.0 : 0.0;
    double score = 1000000.0 * eta + d + 0.001 * forward + hq_bonus;
    if (score < best_score) {
      best_score = score;
      best_target = b->region;
    }
  }

  if (best_target < 0) return 0;
  *target_out = best_target;
  return 1;
}

static int choose_anchor_routed_capturable_enemy_base(const GameState *S, const GameMap *M,
                                                      const Paths *P, const Actions *a,
                                                      int budget, int *target_out) {
  if (!ENABLE_ENEMY_BASE_CAPTURE) return 0;
  double best_score = INFINITY;
  int best_target = -1;
  int best_anchor = -1;

  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side == M->my_side) continue;
    if (b->type == BTYPE_HQ && !ENABLE_ENEMY_HQ_ATTACK) continue;
    if (planned_moves_to_region(a, b->region) > 0) continue;
    if (!can_capture_enemy_building_now(S, M, P, a, b->region, budget)) continue;

    int anchor = -1;
    if (!choose_anchor_for_existing_attack_target(S, M, P, a, b->region, &anchor)) continue;

    int eta = capture_candidate_eta(S, M, P, a, b->region);
    if (eta >= INF_HOPS) continue;
    double d = P->dist[M->my_hq][b->region];
    double forward = M->my_side == SIDE_LEFT ? b->region : (M->N - 1 - b->region);
    double anchor_d = P->dist[anchor][b->region];

    double hq_bonus = (b->type == BTYPE_HQ) ? -500000.0 : 0.0;
    double score = 1000000.0 * eta + d + 0.001 * forward + 0.01 * anchor_d + hq_bonus;
    if (score < best_score) {
      best_score = score;
      best_target = b->region;
      best_anchor = anchor;
    }
  }

  if (best_target < 0) return 0;
  *target_out = best_target;
  if (best_anchor >= 0) g_anchor_route_stage_region = best_anchor;
  return 1;
}

static int choose_anchor_route_stage_candidate(const GameState *S, const GameMap *M,
                                              const Paths *P, const Actions *a,
                                              int *target_out, int *anchor_out) {
  double best_score = INFINITY;
  int best_target = -1, best_anchor = -1;

  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side == M->my_side) continue;
    if (b->type != BTYPE_BASE) continue;
    if (planned_moves_to_region(a, b->region) > 0) continue;
    if (!is_move_destination_candidate(M, b->region)) continue;

    int anchor = -1;
    if (!choose_anchor_for_existing_attack_target(S, M, P, a, b->region, &anchor)) continue;

    int req = required_attackers_for_enemy_base(S, M, b->region);
    if (req <= 0 || req >= 1000000) continue;
    if (req > ANCHOR_ROUTE_MAX_STACK) req = ANCHOR_ROUTE_MAX_STACK;
    if (req < ANCHOR_ROUTE_MIN_ATTACKERS) req = ANCHOR_ROUTE_MIN_ATTACKERS;

    double eta_like = P->dist[anchor][b->region];
    double from_hq = P->dist[M->my_hq][b->region];
    double forward = M->my_side == SIDE_LEFT ? b->region : (M->N - 1 - b->region);
    double score = 100000.0 * req + 1000.0 * eta_like + from_hq + 0.001 * forward;
    if (score < best_score) {
      best_score = score;
      best_target = b->region;
      best_anchor = anchor;
    }
  }

  if (best_target < 0) return 0;
  *target_out = best_target;
  *anchor_out = best_anchor;
  g_anchor_route_stage_region = best_anchor;
  return 1;
}

static int issue_anchor_route_stage_for_future_target(Actions *a,
                                                      const GameState *S,
                                                      const GameMap *M,
                                                      const Paths *P,
                                                      int target, int anchor,
                                                      int *budget) {
#if ENABLE_ANCHOR_ROUTE_ATTACKS
  int desired = required_attackers_for_enemy_base(S, M, target);
  if (desired <= 0 || desired >= 1000000) return 0;
  if (desired < ANCHOR_ROUTE_MIN_ATTACKERS) desired = ANCHOR_ROUTE_MIN_ATTACKERS;
  if (desired > ANCHOR_ROUTE_MAX_STACK) desired = ANCHOR_ROUTE_MAX_STACK;

  int committed = anchor_route_committed_to_anchor(S, M, a, anchor);
  int desired_total_at_anchor = desired + anchor_route_keep_at_anchor(S, M, a, anchor);
  if (committed >= desired_total_at_anchor) return 0;

  int did = 0;
  int staged = 0;
  while (committed < desired_total_at_anchor && staged < ANCHOR_ROUTE_MAX_STAGE_PER_TURN) {
    WarriorId id;
    if (!pick_anchor_route_stage_warrior(S, M, P, a, anchor, &id)) break;
    if (!add_move_action(a, S, M, id, anchor, budget)) break;
    ++did;
    ++staged;
    ++committed;
  }
  return did;
#else
  (void)a; (void)S; (void)M; (void)P; (void)target; (void)anchor; (void)budget;
  return 0;
#endif
}

static int issue_enemy_base_captures(Actions *a, const GameState *S,
                                     const GameMap *M, const Paths *P,
                                     int *budget) {
#if ENABLE_RALLY_STACK_ATTACK
  (void)a; (void)S; (void)M; (void)P; (void)budget;
  return 0;
#else
  int did = 0;
  while (1) {
    int target = -1;
#if ENABLE_ANCHOR_ROUTE_ATTACKS
    if (!choose_anchor_routed_capturable_enemy_base(S, M, P, a, *budget, &target)) {
#if ANCHOR_ROUTE_STAGE_WITHOUT_READY_TARGET
      int anchor = -1;
      if (choose_anchor_route_stage_candidate(S, M, P, a, &target, &anchor) &&
          issue_anchor_route_stage_for_future_target(a, S, M, P, target, anchor, budget))
        did = 1;
#endif
      break;
    }
    {
      int routed = issue_anchor_routed_capture_group_to_target(a, S, M, P, target, budget, g_current_turn);
      if (routed) did = 1;
      break;
    }
#else
    if (!choose_capturable_enemy_base(S, M, P, a, *budget, &target)) break;
    if (!issue_capture_group_to_target(a, S, M, P, target, budget)) break;
    did = 1;
#endif
  }
  return did;
#endif
}


/* Rally-stack attack helpers.  These intentionally do NOT draw attackers from
   multiple regions.  A target is launched only by warriors already stationary at
   one owned rally base.  If the stack is not ready, surplus units from other
   owned bases are moved to that rally base first. */
static int rally_region_stack_hps(const GameState *S, const GameMap *M,
                                  const Actions *a, int rally,
                                  int *hp_out, WarriorId *ids_out, int maxn) {
  int n = 0;
  int allow = attack_surplus_after_plan(S, M, a, rally);
  allow += RALLY_STACK_KEEP_AT_RALLY;
  if (allow <= 0) return 0;
  for (int wi = S->warriors.len - 1; wi >= 0 && n < maxn && allow > 0; --wi) {
    const Warrior *w = &S->warriors.data[wi];
    if (w->id.side != M->my_side || w->region != rally) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    hp_out[n] = max_int(1, w->hp);
    if (ids_out != NULL) ids_out[n] = w->id;
    ++n;
    --allow;
  }
  return n;
}

static int committed_attack_to_region_exists(const GameState *S, const GameMap *M,
                                             const Actions *a, int target) {
  int hp[MAX_COMBAT_SIM_UNITS];
  return collect_committed_attack_hps(S, M, a, target, hp, MAX_COMBAT_SIM_UNITS) > 0;
}

static int choose_rally_stack_target_and_base(const GameState *S, const GameMap *M,
                                              const Paths *P, const Actions *a,
                                              int allow_hq, int *target_out,
                                              int *rally_out) {
  double best_score = INFINITY;
  int best_target = -1, best_rally = -1;
  for (int ti = 0; ti < S->buildings.len; ++ti) {
    const Building *tb = &S->buildings.data[ti];
    if (tb->side == M->my_side) continue;
    if (tb->type == BTYPE_HQ && !allow_hq) continue;
    if (committed_attack_to_region_exists(S, M, a, tb->region)) continue;
    if (!is_move_destination_candidate(M, tb->region)) continue;

    for (int ri = 0; ri < S->buildings.len; ++ri) {
      const Building *rb = &S->buildings.data[ri];
      if (rb->side != M->my_side) continue;
      if (rb->region == tb->region) continue;
      if (region_has_enemy_warrior(S, M, rb->region)) continue;
      if (P->nxt[rb->region][tb->region] == -1) continue;
      if (!attack_path_first_enemy_is_target(S, M, P, rb->region, tb->region)) continue;

      int hp[MAX_COMBAT_SIM_UNITS];
      int stack_n = rally_region_stack_hps(S, M, a, rb->region, hp, NULL, MAX_COMBAT_SIM_UNITS);
      int can_launch = (stack_n >= RALLY_STACK_MIN_LAUNCH_UNITS &&
                        combat_simulation_win(S, M, tb->region, hp, stack_n));
      double target_kind = (tb->type == BTYPE_HQ) ? 50000.0 : 0.0;
      double launch_bonus = can_launch ? -2000000.0 : 0.0;
      double dist = P->dist[rb->region][tb->region];
      double front = M->my_side == SIDE_LEFT ? tb->region : (M->N - 1 - tb->region);
      double stack_bonus = -5000.0 * stack_n;
      double rally_front = M->my_side == SIDE_LEFT ? -rb->region : rb->region;
      double score = launch_bonus + target_kind + dist + 0.001 * front + 0.0001 * rally_front + stack_bonus;
      if (score < best_score) {
        best_score = score;
        best_target = tb->region;
        best_rally = rb->region;
      }
    }
  }
  if (best_target < 0 || best_rally < 0) return 0;
  *target_out = best_target;
  *rally_out = best_rally;
  return 1;
}

static int issue_rally_stack_launch(Actions *a, const GameState *S,
                                    const GameMap *M, const Paths *P,
                                    int *budget, int turn) {
#if ENABLE_RALLY_STACK_ATTACK
  int allow_hq = (turn >= RALLY_STACK_HQ_ATTACK_START_TURN);
  int target = -1, rally = -1;
  if (!choose_rally_stack_target_and_base(S, M, P, a, allow_hq, &target, &rally)) return 0;

  const Building *tb = find_building_const(S, target);
  if (tb == NULL || tb->side == M->my_side) return 0;
  if (tb->type == BTYPE_HQ && !allow_hq) return 0;
  if (committed_attack_to_region_exists(S, M, a, target)) return 0;

  int hp[MAX_COMBAT_SIM_UNITS];
  WarriorId ids[MAX_COMBAT_SIM_UNITS];
  int n = rally_region_stack_hps(S, M, a, rally, hp, ids, MAX_COMBAT_SIM_UNITS);
  if (n < RALLY_STACK_MIN_LAUNCH_UNITS) return 0;
  if (!combat_simulation_win(S, M, target, hp, n)) return 0;

  int move_cost = planned_my_building(S, M, a, target) ? 0 : MOVE_COST;
  if (*budget < move_cost * n) return 0;

  int sent = 0;
  for (int i = 0; i < n; ++i) {
    if (add_move_action(a, S, M, ids[i], target, budget)) ++sent;
  }
  return sent >= RALLY_STACK_MIN_LAUNCH_UNITS;
#else
  (void)a; (void)S; (void)M; (void)P; (void)budget; (void)turn;
  return 0;
#endif
}

static int issue_rally_stack_staging(Actions *a, const GameState *S,
                                     const GameMap *M, const Paths *P,
                                     int *budget, int turn) {
#if ENABLE_RALLY_STACK_ATTACK
  if (turn < RALLY_STACK_STAGE_START_TURN) return 0;
  int allow_hq = (turn >= RALLY_STACK_HQ_ATTACK_START_TURN);
  int target = -1, rally = -1;
  if (!choose_rally_stack_target_and_base(S, M, P, a, allow_hq, &target, &rally)) return 0;

  int hp[MAX_COMBAT_SIM_UNITS];
  int current = rally_region_stack_hps(S, M, a, rally, hp, NULL, MAX_COMBAT_SIM_UNITS);
  if (current >= RALLY_STACK_MIN_LAUNCH_UNITS &&
      combat_simulation_win(S, M, target, hp, current))
    return 0;

  int did = 0, staged = 0;
  while (staged < RALLY_STACK_MAX_STAGE_MOVES_PER_TURN) {
    WarriorId best = {M->my_side, -1};
    double best_score = INFINITY;
    for (int bi = 0; bi < S->buildings.len; ++bi) {
      const Building *src = &S->buildings.data[bi];
      if (src->side != M->my_side) continue;
      if (src->region == rally) continue;
      if (P->nxt[src->region][rally] == -1) continue;
      if (movable_attack_surplus_from_region(S, M, a, src->region) <= 0) continue;
      WarriorId cand;
      if (!pick_attack_warrior_from_region(S, M, a, src->region, &cand)) continue;
      double d = P->dist[src->region][rally];
      double front = M->my_side == SIDE_LEFT ? -src->region : src->region;
      double score = d + 0.0001 * front;
      if (score < best_score) {
        best_score = score;
        best = cand;
      }
    }
    if (best.num < 0) break;
    if (!add_move_action(a, S, M, best, rally, budget)) break;
    did = 1;
    ++staged;
  }
  return did;
#else
  (void)a; (void)S; (void)M; (void)P; (void)budget; (void)turn;
  return 0;
#endif
}

static int issue_neutral_expansion_to_target(Actions *a, const GameState *S,
                                             const GameMap *M, const Paths *P,
                                             int target, int *budget) {
  if (find_building_const(S, target) != NULL) return 0;
  if (neutral_target_already_claimed(S, M, a, target)) return 0;
  WarriorId id;
  if (!choose_surplus_source_for_target(S, M, P, a, target, &id)) return 0;
  return add_neutral_claim_move_with_build_reserve(a, S, M, id, target, budget);
}

static int issue_stronghold_claims_by_priority(Actions *a, const GameState *S,
                                               const GameMap *M, const Paths *P,
                                               int *budget) {
  int did = 0;
  int *used = (int *)calloc((size_t)M->strongholds.len, sizeof(int));

  for (int iter = 0; iter < M->strongholds.len; ++iter) {
    int best_i = -1;
    double best_score = INFINITY;
    for (int i = 0; i < M->strongholds.len; ++i) {
      if (used[i]) continue;
      int r = M->strongholds.data[i];
      const Building *b = find_building_const(S, r);
      if (b != NULL && b->side == M->my_side) continue;
      if (b != NULL && b->type == BTYPE_HQ) continue;

      double d = P->dist[M->my_hq][r];
      double forward = M->my_side == SIDE_LEFT ? r : (M->N - 1 - r);
      double score = d + 0.001 * forward;
      if (score < best_score) {
        best_score = score;
        best_i = i;
      }
    }
    if (best_i < 0) break;
    used[best_i] = 1;

    int target = M->strongholds.data[best_i];
    const Building *b = find_building_const(S, target);
    if (b == NULL) {
      if (enemy_occupied_neutral_stronghold(S, M, target)) {
#if !ENABLE_ANCHOR_ROUTE_ATTACKS || !ANCHOR_ROUTE_STRICT_OFFENSE_ONLY
        if (issue_neutral_occupied_capture_group_to_target(a, S, M, P, target, budget))
          did = 1;
#endif
      } else if (issue_neutral_expansion_to_target(a, S, M, P, target, budget)) {
        did = 1;
      }
    } else if (b->side != M->my_side && b->type == BTYPE_BASE) {
#if !ENABLE_RALLY_STACK_ATTACK && !ENABLE_ANCHOR_ROUTE_ATTACKS
      if (planned_moves_to_region(a, target) == 0 &&
          can_capture_enemy_building_now(S, M, P, a, target, *budget) &&
          issue_capture_group_to_target(a, S, M, P, target, budget))
        did = 1;
#endif
    }
  }

  free(used);
  return did;
}


static int choose_opening_neutral_closest(const GameState *S, const GameMap *M, const Paths *P, const Actions *a, int *target_out, WarriorId *id_out);

static int unclaimed_empty_neutral_exists(const GameState *S, const GameMap *M,
                                          const Actions *a) {
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (action_has_upgrade(a, r)) continue;
    if (neutral_target_already_claimed(S, M, a, r)) continue;
    if (region_has_enemy_warrior(S, M, r)) continue;
    if (g_stack_guard_paths != NULL &&
        enemy_projected_stack_count_at(S, M, g_stack_guard_paths, r) > 0) continue;
    return 1;
  }
  return 0;
}

static int issue_empty_neutral_claims_one_by_one(Actions *a, const GameState *S,
                                                 const GameMap *M, const Paths *P,
                                                 int *budget) {
#if ENABLE_ALLGAME_NEUTRAL_ONE_BY_ONE
  int did = 0;

  /* First build bases where a lone warrior has already arrived.  These are
     neutral claims, so they should not be postponed behind enemy attacks. */
  while (*budget >= BASE_LEVELS[1].cost) {
    int best_region = -1;
    double best_score = INFINITY;
    for (int i = 0; i < M->strongholds.len; ++i) {
      int r = M->strongholds.data[i];
      if (action_has_upgrade(a, r)) continue;
      if (!legal_build_neutral_now(S, M, r)) continue;
      double hq_dist = P->dist[M->my_hq][r];
      double forward = M->my_side == SIDE_LEFT ? r : (M->N - 1 - r);
      double score = hq_dist + 0.001 * forward;
      if (score < best_score) {
        best_score = score;
        best_region = r;
      }
    }
    if (best_region < 0) break;
    add_upgrade_action(a, best_region);
    *budget -= BASE_LEVELS[1].cost;
    did = 1;
  }

  /* Then reserve one surplus warrior per empty stronghold.  Do not group units
     for neutral claims; one body is enough to occupy and build later. */
  while (*budget >= MOVE_COST) {
    int target = -1;
    WarriorId id;
    if (!choose_opening_neutral_closest(S, M, P, a, &target, &id)) break;
    if (!add_neutral_claim_move_with_build_reserve(a, S, M, id, target, budget)) break;
    did = 1;
  }
  return did;
#else
  (void)a; (void)S; (void)M; (void)P; (void)budget;
  return 0;
#endif
}

static MAYBE_UNUSED int issue_neutral_expansion_move(Actions *a, const GameState *S,
                                        const GameMap *M, const Paths *P,
                                        int *budget) {
  int target;
  WarriorId id;
  if (!choose_neutral_expansion(S, M, P, a, &target, &id)) return 0;
  return add_neutral_claim_move_with_build_reserve(a, S, M, id, target, budget);
}

static MAYBE_UNUSED int issue_neutral_expansion_moves(Actions *a, const GameState *S,
                                         const GameMap *M, const Paths *P,
                                         int *budget) {
  int did = 0;
  while (1) {
    int target;
    WarriorId id;
    if (!choose_neutral_expansion(S, M, P, a, &target, &id)) break;
    if (!add_neutral_claim_move_with_build_reserve(a, S, M, id, target, budget)) break;
    did = 1;
  }
  return did;
}


/* previous-turn snapshot helpers are defined in the defense section below. */
static int previous_region_of(WarriorId id);

static int opening_neutral_target_goal(const GameMap *M) {
  int goal = (M->strongholds.len * OPENING_NEUTRAL_FIRST_TARGET_PERCENT + 99) / 100;
  if (goal < OPENING_NEUTRAL_FIRST_MIN_TARGETS) goal = OPENING_NEUTRAL_FIRST_MIN_TARGETS;
  if (goal > OPENING_NEUTRAL_FIRST_MAX_TARGETS) goal = OPENING_NEUTRAL_FIRST_MAX_TARGETS;
  if (goal > M->strongholds.len) goal = M->strongholds.len;
  return goal;
}

static int opening_neutral_handled_count(const GameState *S, const GameMap *M,
                                         const Actions *a) {
  int handled = 0;
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    const Building *b = find_building_const(S, r);
    if (b != NULL && b->side == M->my_side) {
      ++handled;
      continue;
    }
    if (action_has_upgrade(a, r)) {
      ++handled;
      continue;
    }
    if (b == NULL && neutral_target_already_claimed(S, M, a, r)) {
      ++handled;
      continue;
    }
  }
  return handled;
}

static int opening_neutral_empty_remaining(const GameState *S, const GameMap *M,
                                           const Actions *a) {
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (action_has_upgrade(a, r)) continue;
    if (neutral_target_already_claimed(S, M, a, r)) continue;
    if (region_has_enemy_warrior(S, M, r)) continue;
    if (g_stack_guard_paths != NULL &&
        enemy_projected_stack_count_at(S, M, g_stack_guard_paths, r) > 0) continue;
    return 1;
  }
  return 0;
}

static int opening_neutral_should_abort_for_pressure(const GameState *S,
                                                     const GameMap *M,
                                                     const Paths *P) {
  Side opp = opposite(M->my_side);

  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != opp) continue;

    int cur_h = path_hops_between(P, M->my_hq, w->region);
    if (cur_h >= INF_HOPS) continue;

    /* Pressure is confirmed only when the opponent actually starts coming
       toward our HQ.  Raw army-count/mass checks made the opening expansion
       pause too often, leaving gold unused and nearby neutral strongholds
       unclaimed. */
    if (cur_h <= DEFENSE_IMMEDIATE_RADIUS) return 1;

    int prev_r = previous_region_of(w->id);
    if (prev_r < 0) continue;
    int prev_h = path_hops_between(P, M->my_hq, prev_r);
    if (prev_h >= INF_HOPS) continue;
    if (cur_h < prev_h) return 1;
  }

  return 0;
}

static int choose_opening_neutral_closest(const GameState *S,
                                          const GameMap *M,
                                          const Paths *P,
                                          const Actions *a,
                                          int *target_out,
                                          WarriorId *id_out) {
  double best_score = INFINITY;
  int best_target = -1;
  WarriorId best_id = {M->my_side, -1};

  for (int ti = 0; ti < M->strongholds.len; ++ti) {
    int target = M->strongholds.data[ti];
    if (find_building_const(S, target) != NULL) continue;
    if (neutral_target_already_claimed(S, M, a, target)) continue;
    if (move_target_has_enemy_projected(S, M, P, target)) continue;

    WarriorId id;
    if (!choose_surplus_source_for_target(S, M, P, a, target, &id)) continue;
    const Warrior *w = find_warrior_const(S, id);
    if (w == NULL) continue;

    double hq_dist = P->dist[M->my_hq][target];
    double source_dist = P->dist[w->region][target];
    double forward = M->my_side == SIDE_LEFT ? target : (M->N - 1 - target);

    /* HQ distance dominates: the opening slice is taken from our home side
       outward, not from whichever target is closest to a stray surplus warrior. */
    double score = 1000000000.0 * hq_dist + 1000.0 * forward + source_dist;
    if (score < best_score) {
      best_score = score;
      best_target = target;
      best_id = id;
    }
  }

  if (best_target < 0) return 0;
  *target_out = best_target;
  *id_out = best_id;
  return 1;
}


static int opening_neutral_has_available_surplus(const GameState *S,
                                                 const GameMap *M,
                                                 const Paths *P,
                                                 const Actions *a) {
  for (int ti = 0; ti < M->strongholds.len; ++ti) {
    int target = M->strongholds.data[ti];
    if (find_building_const(S, target) != NULL) continue;
    if (neutral_target_already_claimed(S, M, a, target)) continue;
    if (move_target_has_enemy_projected(S, M, P, target)) continue;
    WarriorId id;
    if (choose_surplus_source_for_target(S, M, P, a, target, &id)) return 1;
  }
  return 0;
}

static int conservative_expected_income(const GameState *S, const GameMap *M,
                                        const Actions *a, int train_n);

static int pending_neutral_build_count_all(const GameState *S,
                                           const GameMap *M,
                                           const Actions *a) {
  int cnt = 0;
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (action_has_upgrade(a, r)) continue;
    if (neutral_target_already_claimed(S, M, a, r)) ++cnt;
  }
  return cnt;
}


typedef struct {
  int region;
  int ready_day;
} NeutralBuildEvent;

static void add_pending_neutral_build_event(NeutralBuildEvent *events, int *len,
                                            int region, int ready_day) {
  if (region < 0 || ready_day < 0) return;
  for (int i = 0; i < *len; ++i) {
    if (events[i].region == region) {
      if (ready_day < events[i].ready_day)
        events[i].ready_day = ready_day;
      return;
    }
  }
  events[*len].region = region;
  events[*len].ready_day = ready_day;
  ++(*len);
}

static int collect_pending_neutral_build_events(const GameState *S,
                                                const GameMap *M,
                                                const Paths *P,
                                                const Actions *a,
                                                NeutralBuildEvent *events,
                                                int max_events) {
  int len = 0;
  (void)max_events;

  /* Existing or already commanded builds are not future drains here: their cost
     has either already been paid in previous turns, or has already been removed
     from the current planner budget.  What matters for neutral training timing
     is an unbuilt stronghold that another warrior has already claimed and will
     likely build before the newly trained worker's target. */
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;

    int target = -1;
    int ready_day = INF_HOPS;

    if (is_stronghold(M, w->region) && find_building_const(S, w->region) == NULL &&
        !action_has_upgrade(a, w->region)) {
      target = w->region;
      ready_day = 0;
    } else if (w->state == WSTATE_MOVING && is_stronghold(M, w->target) &&
               find_building_const(S, w->target) == NULL &&
               !action_has_upgrade(a, w->target)) {
      int hops = path_hops_between(P, w->region, w->target);
      if (hops < INF_HOPS) {
        target = w->target;
        ready_day = hops + 1;
      }
    }

    if (target >= 0 && len < max_events)
      add_pending_neutral_build_event(events, &len, target, ready_day);
  }

  for (int i = 0; i < a->moves.len; ++i) {
    const Move *mv = &a->moves.data[i];
    const Warrior *w = find_warrior_const(S, mv->id);
    if (w == NULL || w->id.side != M->my_side) continue;
    int target = mv->target;
    if (!is_stronghold(M, target)) continue;
    if (find_building_const(S, target) != NULL) continue;
    if (action_has_upgrade(a, target)) continue;
    int hops = path_hops_between(P, w->region, target);
    if (hops >= INF_HOPS) continue;
    if (len < max_events)
      add_pending_neutral_build_event(events, &len, target, hops + 1);
  }

  return len;
}

static int spend_due_pending_neutral_builds(long long *g,
                                            NeutralBuildEvent *events,
                                            int event_count,
                                            int day) {
  int spent = 0;
  int changed = 1;
  while (changed) {
    changed = 0;
    int best_i = -1;
    int best_day = INF_HOPS;
    int best_region = 1000000000;
    for (int i = 0; i < event_count; ++i) {
      if (events[i].ready_day < 0 || events[i].ready_day > day) continue;
      if (events[i].ready_day < best_day ||
          (events[i].ready_day == best_day && events[i].region < best_region)) {
        best_i = i;
        best_day = events[i].ready_day;
        best_region = events[i].region;
      }
    }
    if (best_i >= 0 && *g >= BASE_LEVELS[1].cost) {
      *g -= BASE_LEVELS[1].cost;
      events[best_i].ready_day = -1;
      ++spent;
      changed = 1;
    }
  }
  return spent;
}

static int choose_timed_neutral_train_target(const GameState *S,
                                             const GameMap *M,
                                             const Paths *P,
                                             const Actions *a) {
  int best = -1;
  double best_score = INFINITY;
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (action_has_upgrade(a, r)) continue;
    if (neutral_target_already_claimed(S, M, a, r)) continue;
    if (move_target_has_enemy_projected(S, M, P, r)) continue;
    if (P->nxt[M->my_hq][r] == -1) continue;
    if (full_path_enemy_blocked(S, M, P, M->my_hq, r, 0)) continue;
    double forward = M->my_side == SIDE_LEFT ? r : (M->N - 1 - r);
    double score = 1000000.0 * P->dist[M->my_hq][r] + forward;
    if (score < best_score) {
      best_score = score;
      best = r;
    }
  }
  return best;
}

static int neutral_build_ready_on_arrival_after_train_delay(
    const GameState *S, const GameMap *M, const Paths *P, const Actions *a,
    int budget_before_train, int target, int train_delay_days, int train_n) {
  if (target < 0) return 0;
  int hops = path_hops_between(P, M->my_hq, target);
  if (hops >= INF_HOPS) return 0;

  /* Day 0 is the current command.  If we train on day d, the warrior is born
     in that day's training phase, moves for the first time on day d+1, reaches
     the target after `hops` move phases, and can build on the following
     morning: day d+hops+1.

     The key opening rule is not merely "can eventually build".  Training too
     early makes the new warrior pay upkeep while waiting on an empty
     stronghold.  Therefore this predicate succeeds only if the base can be
     built immediately on the first morning after the warrior arrives. */
  int build_day = train_delay_days + hops + 1;
  long long g = budget_before_train;
  int base_income = conservative_expected_income(S, M, a, 0);
  int alive0 = count_side_warriors(S, M->my_side);
  int trained = 0;
  NeutralBuildEvent pending[256];
  int pending_count = collect_pending_neutral_build_events(S, M, P, a, pending, 256);

  for (int day = 0; day <= build_day; ++day) {
    /* Other warriors that are already sitting on or moving toward empty
       strongholds will consume 300 gold before this newly trained worker's
       target if their construction window opens earlier.  Simulate those
       queued neutral builds greedily at the start of each command day, because
       construction is paid in the morning before movement/training. */
    spend_due_pending_neutral_builds(&g, pending, pending_count, day);

    if (day == train_delay_days) {
      int cost = TRAIN_COST * train_n;
      if (g < cost) return 0;
      g -= cost;
      trained += train_n;
    }

    if (day == train_delay_days + 1) {
      /* Neutral/base construction target, so the first move costs 10.  Do not
         charge this on the training day; in the real game it is paid on the
         morning the move command is issued, after one more evening of income if
         the warrior was trained the previous day. */
      if (g < MOVE_COST) return 0;
      g -= MOVE_COST;
    }

    if (day == build_day)
      return g >= BASE_LEVELS[1].cost;

    int income = base_income;
    if (trained > 0) {
      /* A just-trained warrior can work at HQ on the same evening only up to
         the HQ work cap.  Recompute conservatively by reusing the helper for
         day 0; for future days this is still a good lower-stability estimate. */
      income = conservative_expected_income(S, M, a, trained);
    }
    g += income;
    g -= UPKEEP_PER_WARRIOR * (alive0 + trained);
    if (g < 0) g = 0;
  }
  return 0;
}

static int train_timing_matches_neutral_build(const GameState *S,
                                              const GameMap *M,
                                              const Paths *P,
                                              const Actions *a,
                                              int budget_after_train,
                                              int target, int train_n) {
  if (target < 0) return 0;
  int budget_before_train = budget_after_train + TRAIN_COST * train_n;
  return neutral_build_ready_on_arrival_after_train_delay(
      S, M, P, a, budget_before_train, target, 0, train_n);
}

static int choose_trainable_timed_neutral_train_target(const GameState *S,
                                                       const GameMap *M,
                                                       const Paths *P,
                                                       const Actions *a,
                                                       int budget_before_train,
                                                       int train_n) {
  int best = -1;
  double best_score = INFINITY;
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (action_has_upgrade(a, r)) continue;
    if (neutral_target_already_claimed(S, M, a, r)) continue;
    if (move_target_has_enemy_projected(S, M, P, r)) continue;
    if (P->nxt[M->my_hq][r] == -1) continue;
    if (full_path_enemy_blocked(S, M, P, M->my_hq, r, 0)) continue;
    if (!neutral_build_ready_on_arrival_after_train_delay(
            S, M, P, a, budget_before_train, r, 0, train_n))
      continue;

    /* Prefer the earliest build day; break ties by forward/close target. */
    int hops = path_hops_between(P, M->my_hq, r);
    double forward = M->my_side == SIDE_LEFT ? r : (M->N - 1 - r);
    double score = 1000000.0 * (hops + 1) + 1000.0 * forward + P->dist[M->my_hq][r];
    if (score < best_score) {
      best_score = score;
      best = r;
    }
  }
  return best;
}

static int opening_train_one_if_stuck(Actions *a, const GameState *S,
                                      const GameMap *M, const Paths *P,
                                      int *budget, int turn) {
  if (turn > OPENING_FORCE_TRAIN_NEUTRAL_UNTIL) return 0;
  if (a->train_n > 0 || *budget < TRAIN_COST) return 0;
  if (!opening_neutral_empty_remaining(S, M, a)) return 0;
  if (opening_neutral_has_available_surplus(S, M, P, a)) return 0;
  int cap = planned_train_cap(S, M, a);
  if (cap <= 0) return 0;
  int after_gold = *budget - TRAIN_COST;
  if (after_gold < 0) return 0;
  int target = choose_trainable_timed_neutral_train_target(S, M, P, a, *budget, 1);
  if (target < 0) return 0;
  a->train_n = 1;
  *budget = after_gold;
  return 1;
}

static int issue_opening_neutral_first_phase(Actions *a, const GameState *S,
                                             const GameMap *M, const Paths *P,
                                             int *budget, int turn) {
#if ENABLE_OPENING_NEUTRAL_FIRST
  if (g_opening_neutral_done) return 0;
  if (turn > OPENING_NEUTRAL_FIRST_MAX_TURN) {
    g_opening_neutral_done = 1;
    return 0;
  }

  /* Enemy pressure should pause the opening-neutral slice, not permanently
     kill it.  A one-turn mass/advantage warning used to set opening_done=1,
     so the bot never resumed nearby neutral expansion even after the danger
     passed.  Keep this abort check as the gate immediately before expansion,
     but let the phase continue on later safe turns until quota/max-turn. */
  if (opening_neutral_should_abort_for_pressure(S, M, P)) {
    return 0;
  }

  int goal = opening_neutral_target_goal(M);
  if (opening_neutral_handled_count(S, M, a) >= goal ||
      !opening_neutral_empty_remaining(S, M, a)) {
    g_opening_neutral_done = 1;
    return 0;
  }

  /* Build already occupied neutral strongholds first, closest to HQ first. */
  while (*budget >= BASE_LEVELS[1].cost &&
         opening_neutral_handled_count(S, M, a) < goal) {
    int best_region = -1;
    double best_score = INFINITY;
    for (int i = 0; i < M->strongholds.len; ++i) {
      int r = M->strongholds.data[i];
      if (action_has_upgrade(a, r)) continue;
      if (!legal_build_neutral_now(S, M, r)) continue;
      double d = P->dist[M->my_hq][r];
      double forward = M->my_side == SIDE_LEFT ? r : (M->N - 1 - r);
      double score = d + 0.001 * forward;
      if (score < best_score) {
        best_score = score;
        best_region = r;
      }
    }
    if (best_region < 0) break;
    add_upgrade_action(a, best_region);
    *budget -= BASE_LEVELS[1].cost;
  }

  /* Then send surplus warriors to nearby empty strongholds, still in strict
     HQ-distance order, but only until the opening quota is satisfied. */
  while (*budget >= MOVE_COST &&
         opening_neutral_handled_count(S, M, a) < goal) {
    int target = -1;
    WarriorId id;
    if (!choose_opening_neutral_closest(S, M, P, a, &target, &id)) break;
    if (!add_neutral_claim_move_with_build_reserve(a, S, M, id, target, budget)) break;
  }

  if (opening_neutral_handled_count(S, M, a) >= goal ||
      !opening_neutral_empty_remaining(S, M, a)) {
    g_opening_neutral_done = 1;
  }

  /* If the only blocker is lack of a spare warrior, train exactly one body now.
     It will move next turn, which is still better than spending the turn on
     HQ/old-base upgrades while nearby empty strongholds remain. */
  opening_train_one_if_stuck(a, S, M, P, budget, turn);

  return a->moves.len > 0 || a->upgrades.len > 0 || a->train_n > 0;
#else
  (void)a; (void)S; (void)M; (void)P; (void)budget; (void)turn;
  return 0;
#endif
}



static int issue_immediate_hq_neutral_dispatch(Actions *a, const GameState *S,
                                               const GameMap *M, const Paths *P,
                                               int *budget, int turn) {
  if (turn > OPENING_FORCE_TRAIN_NEUTRAL_UNTIL) return 0;
  if (*budget < MOVE_COST) return 0;

  /* Newly trained neutral workers should not spend a full turn idling at HQ.
     If HQ has surplus bodies and an empty safe stronghold remains, send one
     immediately, even if the 300-gold build will be paid on arrival rather
     than reserved today. */
  int hq_count = planned_workers_physically_remaining_at(S, a, M->my_side, M->my_hq);
  int hq_cap = planned_work_cap_at(S, M, a, M->my_hq);
  if (hq_count <= hq_cap) return 0;

  int best_target = -1;
  double best_score = INFINITY;
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (action_has_upgrade(a, r)) continue;
    if (neutral_target_already_claimed(S, M, a, r)) continue;
    if (move_target_has_enemy_projected(S, M, P, r)) continue;
    if (P->nxt[M->my_hq][r] == -1) continue;
    if (full_path_enemy_blocked(S, M, P, M->my_hq, r, 0)) continue;
    double forward = M->my_side == SIDE_LEFT ? r : (M->N - 1 - r);
    double score = P->dist[M->my_hq][r] + 0.001 * forward;
    if (score < best_score) {
      best_score = score;
      best_target = r;
    }
  }
  if (best_target < 0) return 0;

  WarriorId best_id = {M->my_side, -1};
  int best_num = 1000000000;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (w->region != M->my_hq) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (w->id.num < best_num) {
      best_num = w->id.num;
      best_id = w->id;
    }
  }
  if (best_id.num < 0) return 0;

  return add_move_action_ex_stack_flags(a, S, M, P, best_id, best_target,
                                        budget, MOVE_FLAG_IGNORE_STACK_GUARD);
}

static int opening_neutral_unfinished(const GameState *S, const GameMap *M,
                                      const Actions *a, int turn) {
#if ENABLE_OPENING_NEUTRAL_FIRST
  if (g_opening_neutral_done) return 0;
  if (turn > OPENING_NEUTRAL_FIRST_MAX_TURN) return 0;
  int goal = opening_neutral_target_goal(M);
  if (opening_neutral_handled_count(S, M, a) >= goal) return 0;
  if (!opening_neutral_empty_remaining(S, M, a)) return 0;
  return 1;
#else
  (void)S; (void)M; (void)a; (void)turn;
  return 0;
#endif
}

static int is_forward_region(const GameMap *M, int dst, int src) {
  return M->my_side == SIDE_LEFT ? (dst > src) : (dst < src);
}

static int choose_forward_staging_target(const GameState *S, const GameMap *M,
                                         const Paths *P, const Actions *a,
                                         int source_region) {
  double best = INFINITY;
  int best_region = -1;

  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *dst = &S->buildings.data[bi];
    if (dst->side != M->my_side) continue;
    if (dst->region == source_region) continue;
    if (!is_forward_region(M, dst->region, source_region)) continue;
    if (P->nxt[source_region][dst->region] == -1) continue;

    int cap = planned_work_cap_at(S, M, a, dst->region);
    int committed = planned_workers_committed_to_region(S, a, M->my_side, dst->region);
    int target_limit = cap + FORWARD_STAGING_EXTRA_CAP;
    if (committed >= target_limit) continue;

    double d = P->dist[source_region][dst->region];
    double forward_bonus = M->my_side == SIDE_LEFT ? dst->region : (M->N - 1 - dst->region);
    double score = d - 0.01 * forward_bonus;
    if (score < best) {
      best = score;
      best_region = dst->region;
    }
  }

  return best_region;
}

static MAYBE_UNUSED int issue_forward_staging_moves(Actions *a, const GameState *S,
                                       const GameMap *M, const Paths *P,
                                       int *budget) {
  if (!ENABLE_FORWARD_STAGING) return 0;
  int did = 0;

  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *src = &S->buildings.data[bi];
    if (src->side != M->my_side) continue;

    int sent_from_source = 0;
    while (sent_from_source < FORWARD_STAGING_MAX_PER_SOURCE &&
           source_surplus_after_plan(S, M, a, src->region) > 0) {
      int target = choose_forward_staging_target(S, M, P, a, src->region);
      if (target < 0) break;

      WarriorId id;
      if (!pick_surplus_warrior_from_region(S, M, a, src->region, &id)) break;
      if (!add_move_action(a, S, M, id, target, budget)) break;

      ++sent_from_source;
      did = 1;
    }
  }

  return did;
}

/* ---- ETA-based home defense, using previous-turn positions ---- */

static int g_prev_initialized = 0;
static int g_have_prev_snapshot = 0;
static int g_prev_region[2][DEFENSE_MAX_TRACKED_ID];

static void strategy_prev_init(void) {
  if (g_prev_initialized) return;
  for (int s = 0; s < 2; ++s)
    for (int i = 0; i < DEFENSE_MAX_TRACKED_ID; ++i)
      g_prev_region[s][i] = -1;
  g_prev_initialized = 1;
}

static int side_index(Side s) { return s == SIDE_LEFT ? 0 : 1; }

static int previous_region_of(WarriorId id) {
  if (!g_have_prev_snapshot) return -1;
  if (id.num < 0 || id.num >= DEFENSE_MAX_TRACKED_ID) return -1;
  return g_prev_region[side_index(id.side)][id.num];
}

static void update_previous_snapshot(const GameState *S) {
  strategy_prev_init();
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.num >= 0 && w->id.num < DEFENSE_MAX_TRACKED_ID)
      g_prev_region[side_index(w->id.side)][w->id.num] = w->region;
  }
  g_have_prev_snapshot = 1;
}

typedef struct {
  int triggered;
  int hard;
  int recall_radius;
  int first_bad_eta;
  int incoming_total;
  int train_now;
} DefensePlan;

static int enemy_is_inbound_to_home(const GameState *S, const GameMap *M,
                                    const Paths *P, const Warrior *w) {
  (void)S;
  int cur_h = path_hops_between(P, M->my_hq, w->region);
  if (cur_h >= INF_HOPS) return 0;

  /* Very near enemies are threats even if the previous snapshot is missing or
     the enemy spent one turn fighting instead of decreasing distance. */
  if (cur_h <= DEFENSE_IMMEDIATE_RADIUS) return 1;

  int prev_r = previous_region_of(w->id);
  if (prev_r < 0) return 0;
  int prev_h = path_hops_between(P, M->my_hq, prev_r);
  if (prev_h >= INF_HOPS) return 0;
  return cur_h < prev_h;
}

static int my_warrior_defense_eta(const GameMap *M, const Paths *P,
                                  const Warrior *w) {
  int h = path_hops_between(P, M->my_hq, w->region);
  if (h >= INF_HOPS) return INF_HOPS;
  if (w->region == M->my_hq) return 0;
  if (w->state == WSTATE_STATIONARY) return h;
  if (w->state == WSTATE_MOVING && w->target == M->my_hq) return h;
  return INF_HOPS;
}

static int defense_train_count(const GameState *S, const GameMap *M,
                               const Actions *a, int budget) {
  int cap = planned_train_cap(S, M, a);
  if (cap <= 0) return 0;
  return min_int(cap, budget / TRAIN_COST);
}

static DefensePlan compute_defense_plan(const GameState *S, const GameMap *M,
                                        const Paths *P, const Actions *a,
                                        int budget) {
  DefensePlan plan;
  memset(&plan, 0, sizeof(plan));
  plan.recall_radius = -1;
  plan.first_bad_eta = INF_HOPS;

  if (!ENABLE_HOME_DEFENSE) return plan;

  int max_h = P->N + 1;
  int *friendly_eta = (int *)calloc((size_t)(max_h + 2), sizeof(int));
  int *enemy_eta = (int *)calloc((size_t)(max_h + 2), sizeof(int));
  if (friendly_eta == NULL || enemy_eta == NULL) {
    free(friendly_eta);
    free(enemy_eta);
    return plan;
  }

  Side opp = opposite(M->my_side);
  for (int wi = 0; wi < S->warriors.len; ++wi) {
    const Warrior *w = &S->warriors.data[wi];
    if (w->id.side == M->my_side) {
      if (action_has_move_warrior(a, w->id)) continue;
      int eta = my_warrior_defense_eta(M, P, w);
      if (eta <= max_h) ++friendly_eta[eta];
    } else if (w->id.side == opp) {
      if (!enemy_is_inbound_to_home(S, M, P, w)) continue;
      int eta = path_hops_between(P, M->my_hq, w->region);
      if (eta <= max_h) {
        ++enemy_eta[eta];
        ++plan.incoming_total;
      }
    }
  }

  plan.train_now = defense_train_count(S, M, a, budget);
  friendly_eta[0] += plan.train_now;

  int friendly_cum = 0;
  int enemy_cum = 0;
  int max_bad_eta = -1;
  int first_bad_eta = INF_HOPS;
  for (int eta = 0; eta <= max_h; ++eta) {
    friendly_cum += friendly_eta[eta];
    enemy_cum += enemy_eta[eta];
    if (enemy_cum <= 0) continue;
    if (friendly_cum < enemy_cum + DEFENSE_SAFETY_MARGIN) {
      if (first_bad_eta == INF_HOPS) first_bad_eta = eta;
      max_bad_eta = eta;
    }
  }

  if (max_bad_eta >= 0) {
    plan.triggered = 1;
    plan.first_bad_eta = first_bad_eta;
    plan.recall_radius = min_int(max_h, max_bad_eta + DEFENSE_RECALL_EXTRA);
    if (first_bad_eta <= DEFENSE_HARD_ETA)
      plan.hard = 1;
    /* A very large inbound mass should suspend expansion even if its first
       arrival is slightly later, because many of our distant units are already
       locked into other move orders and cannot be retasked immediately. */
    if (plan.incoming_total >= 12 && first_bad_eta <= DEFENSE_HARD_ETA + 4)
      plan.hard = 1;
  }

  free(friendly_eta);
  free(enemy_eta);
  return plan;
}

typedef struct {
  int region;
  int keep;
} DefenseKeep;

static int build_defense_keep_plan(const GameState *S, const GameMap *M,
                                   const Actions *a, int train_n,
                                   DefenseKeep **out) {
  int cnt = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi)
    if (S->buildings.data[bi].side == M->my_side) ++cnt;

  if (cnt == 0) {
    *out = NULL;
    return 0;
  }

  DefenseKeep *keep = (DefenseKeep *)calloc((size_t)cnt, sizeof(DefenseKeep));
  int *available = (int *)calloc((size_t)cnt, sizeof(int));
  int idx = 0;
  int total_possible = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    int r = b->region;
    int cap = building_work_cap(b);
    int workers = planned_workers_physically_remaining_at(S, a, M->my_side, r);
    if (r == M->my_hq) workers += train_n;
    available[idx] = min_int(cap, workers);
    keep[idx].region = r;
    keep[idx].keep = 0;
    total_possible += available[idx];
    ++idx;
  }

  int alive_after_train = count_side_warriors(S, M->my_side) + train_n;
  int needed = (UPKEEP_PER_WARRIOR * alive_after_train + WORK_INCOME - 1) / WORK_INCOME;
  needed += DEFENSE_KEEP_EXTRA_WORKERS;
  needed = min_int(needed, total_possible);

  int assigned = 0;
  while (assigned < needed) {
    int progressed = 0;
    for (int i = 0; i < cnt && assigned < needed; ++i) {
      if (keep[i].keep >= available[i]) continue;
      ++keep[i].keep;
      ++assigned;
      progressed = 1;
    }
    if (!progressed) break;
  }

  free(available);
  *out = keep;
  return cnt;
}

static int defense_keep_slot_index(const DefenseKeep *keep, int len, int region) {
  for (int i = 0; i < len; ++i)
    if (keep[i].region == region) return i;
  return -1;
}

static int issue_home_defense(Actions *a, const GameState *S, const GameMap *M,
                              const Paths *P, int *budget,
                              DefensePlan *out_plan) {
  DefensePlan plan = compute_defense_plan(S, M, P, a, *budget);
  if (out_plan != NULL) *out_plan = plan;
  if (!plan.triggered) return 0;

  int recalled = 0;

  /* Training happens before combat, so it is the cheapest immediate defense.
     It is reserved before recall/economy actions consume budget. */
  a->train_n = plan.train_now;
  *budget -= TRAIN_COST * a->train_n;

  DefenseKeep *def_keep = NULL;
  int def_keep_len = build_defense_keep_plan(S, M, a, a->train_n, &def_keep);

  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->region == M->my_hq) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;

    int h = path_hops_between(P, M->my_hq, w->region);
    if (h >= INF_HOPS) continue;
    if (h > plan.recall_radius) continue;

    /* In soft defense, keep a lone builder sitting on an unbuilt stronghold so
       expansion is not accidentally cancelled.  In hard defense, survival has
       priority and that unit is recalled too. */
    if (!plan.hard && is_stronghold(M, w->region) &&
        find_building_const(S, w->region) == NULL)
      continue;

    /* Even in hard defense, do not recall every worker.  Keep the minimum
       number of workers needed to cover expected population upkeep, otherwise
       we can survive the attack but immediately lose warriors to hunger. */
    int keep_idx = defense_keep_slot_index(def_keep, def_keep_len, w->region);
    if (keep_idx >= 0 && def_keep[keep_idx].keep > 0) {
      --def_keep[keep_idx].keep;
      continue;
    }

    /* Recall commands to our HQ are free.  Allow issuing the command even from
       a contested source; the game will make the warrior wait until the region
       is no longer blocked, but the destination is already set to HQ. */
    if (add_move_action_ex(a, S, M, w->id, M->my_hq, budget, 1))
      ++recalled;
  }

  free(def_keep);
  return 1 + recalled;
}


/* Emergency guard that is deliberately NOT tied to ENABLE_HOME_DEFENSE.
   It only triggers when enemy warriors are explicitly targeting our HQ.  This
   keeps the tuned opening/economy parameters unchanged, but prevents the bot
   from continuing normal expansion while the opponent is already walking a
   stack into the headquarters.

   Hardcoded selective version:
   1) Count enemy warriors that are already on our HQ or directly moving to it.
   2) Count defenders already at our HQ, defenders already moving to HQ, and
      warriors that can be trained this turn.
   3) Recall only the remaining needed defenders, sorted from the closest owned
      base/HQ-side position first.  If the incoming attack is too large, this
      naturally recalls every available defender.

   No hyperparameters are added here by design. */
typedef struct {
  int count;
  int hp;
  int at_hq;
  int min_eta;
} DirectHqThreat;

typedef struct {
  WarriorId id;
  int hp;
  int eta;
  int region;
  int from_owned_building;
} DirectHqDefenderCandidate;

static int cmp_direct_hq_defender_candidate(const void *pa, const void *pb) {
  const DirectHqDefenderCandidate *a = (const DirectHqDefenderCandidate *)pa;
  const DirectHqDefenderCandidate *b = (const DirectHqDefenderCandidate *)pb;

  /* Prefer pulling from our bases/HQ-side infrastructure before isolated
     bodies.  Then choose the closest defender to HQ. */
  if (a->from_owned_building != b->from_owned_building)
    return b->from_owned_building - a->from_owned_building;
  if (a->eta != b->eta) return a->eta - b->eta;
  if (a->region != b->region) return a->region - b->region;
  if (a->id.side != b->id.side) return (int)a->id.side - (int)b->id.side;
  return a->id.num - b->id.num;
}

static DirectHqThreat direct_hq_threat_stats(const GameState *S,
                                             const GameMap *M,
                                             const Paths *P) {
  DirectHqThreat t;
  t.count = 0;
  t.hp = 0;
  t.at_hq = 0;
  t.min_eta = INF_HOPS;

  Side opp = opposite(M->my_side);
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != opp) continue;

    int hq_bound = 0;
    if (w->region == M->my_hq) {
      hq_bound = 1;
      ++t.at_hq;
    } else if (w->state == WSTATE_MOVING && w->target == M->my_hq) {
      hq_bound = 1;
    }
    if (!hq_bound) continue;

    int eta = path_hops_between(P, w->region, M->my_hq);
    if (eta < t.min_eta) t.min_eta = eta;
    ++t.count;
    t.hp += max_int(1, w->hp);
  }
  return t;
}

static int direct_hq_current_defender_stats(const GameState *S,
                                            const GameMap *M,
                                            const Actions *a,
                                            int *hp_out) {
  int cnt = 0;
  int hp = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (action_has_move_warrior(a, w->id)) continue;

    if (w->region == M->my_hq ||
        (w->state == WSTATE_MOVING && w->target == M->my_hq)) {
      ++cnt;
      hp += max_int(1, w->hp);
    }
  }
  if (hp_out) *hp_out = hp;
  return cnt;
}

static int direct_hq_train_hp(const GameState *S, const GameMap *M) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq != NULL && hq->side == M->my_side && hq->type == BTYPE_HQ)
    return HQ_LEVELS[hq->level].warrior_hp;
  return HQ_LEVELS[1].warrior_hp;
}

static int direct_hq_collect_recall_candidates(const GameState *S,
                                               const GameMap *M,
                                               const Paths *P,
                                               const Actions *a,
                                               DirectHqDefenderCandidate *out,
                                               int max_out) {
  int n = 0;
  for (int i = 0; i < S->warriors.len && n < max_out; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->region == M->my_hq) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;

    int eta = path_hops_between(P, w->region, M->my_hq);
    if (eta >= INF_HOPS) continue;

    const Building *b = find_building_const(S, w->region);
    DirectHqDefenderCandidate c;
    c.id = w->id;
    c.hp = max_int(1, w->hp);
    c.eta = eta;
    c.region = w->region;
    c.from_owned_building = (b != NULL && b->side == M->my_side) ? 1 : 0;
    out[n++] = c;
  }
  qsort(out, (size_t)n, sizeof(DirectHqDefenderCandidate),
        cmp_direct_hq_defender_candidate);
  return n;
}


/* Owned-base emergency defense.
   If an enemy wave will enter one of our BASE strongholds this morning, simulate
   the coming combat.  When the base falls without help but survives if nearby
   warriors step in this turn, pull exactly the needed nearby defenders.  This
   runs before zero-worker refill, because keeping an already-owned base alive
   is more important than filling a safe empty building. */
#ifndef ENABLE_OWNED_BASE_EMERGENCY_DEFENSE
#define ENABLE_OWNED_BASE_EMERGENCY_DEFENSE 1
#endif
#ifndef OWNED_BASE_CRISIS_RELEASE_RADIUS
#define OWNED_BASE_CRISIS_RELEASE_RADIUS 1
#endif
#ifndef OWNED_BASE_REINFORCE_SOURCE_RADIUS
#define OWNED_BASE_REINFORCE_SOURCE_RADIUS 7
#endif
#ifndef OWNED_BASE_MAX_EMERGENCY_PULL
#define OWNED_BASE_MAX_EMERGENCY_PULL 19
#endif
#ifndef OWNED_BASE_CRISIS_SIM_DAYS
#define OWNED_BASE_CRISIS_SIM_DAYS 36
#endif

static int g_owned_base_crisis[256];

static int enemy_near_region(const GameState *S, const GameMap *M,
                             const Paths *P, int region, int radius) {
  Side opp = opposite(M->my_side);
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != opp) continue;
    int h = path_hops_between(P, w->region, region);
    if (h <= radius) return 1;
    if (w->state == WSTATE_MOVING && w->target == region) return 1;
  }
  return 0;
}

static int enemy_eta_to_owned_base(const GameState *S, const GameMap *M,
                                   const Paths *P, const Warrior *w,
                                   int region) {
  (void)S;
  if (w == NULL || w->id.side != opposite(M->my_side)) return -1;
  if (w->region == region) return 0;
  if (w->state != WSTATE_MOVING || w->target != region) return -1;
  int h = path_hops_between(P, w->region, region);
  if (h < 0 || h >= 1000000) return -1;
  return max_int(0, h - 1);
}

static int my_eta_to_owned_base(const Paths *P, const Warrior *w, int region) {
  if (w == NULL) return -1;
  if (w->region == region) return 0;
  if (w->state == WSTATE_MOVING && w->target == region) {
    int h = path_hops_between(P, w->region, region);
    if (h < 0 || h >= 1000000) return -1;
    return max_int(0, h - 1);
  }
  return -1;
}

typedef struct {
  int hp;
  int num;
  int eta;
} CrisisCombatUnit;

static int crisis_alive_arrived_count(const CrisisCombatUnit *u, int n, int day) {
  int c = 0;
  for (int i = 0; i < n; ++i)
    if (u[i].eta <= day && u[i].hp > 0) ++c;
  return c;
}

static int crisis_future_arrivals(const CrisisCombatUnit *u, int n, int day) {
  for (int i = 0; i < n; ++i)
    if (u[i].eta > day && u[i].hp > 0) return 1;
  return 0;
}

static void crisis_deal_one_to_weakest(CrisisCombatUnit *u, int n, int day) {
  int best = -1;
  for (int i = 0; i < n; ++i) {
    if (u[i].eta > day || u[i].hp <= 0) continue;
    if (best < 0 || u[i].hp < u[best].hp ||
        (u[i].hp == u[best].hp && u[i].num < u[best].num))
      best = i;
  }
  if (best >= 0) --u[best].hp;
}

static void crisis_enemy_attack_one(CrisisCombatUnit *def, int def_n,
                                    int day, int *building_hp) {
  int best = -1;
  for (int i = 0; i < def_n; ++i) {
    if (def[i].eta > day || def[i].hp <= 0) continue;
    if (best < 0 || def[i].hp < def[best].hp ||
        (def[i].hp == def[best].hp && def[i].num < def[best].num))
      best = i;
  }
  if (best >= 0) {
    --def[best].hp;
  } else if (*building_hp > 0) {
    --(*building_hp);
  }
}

static int collect_enemy_base_attack_schedule(const GameState *S,
                                              const GameMap *M,
                                              const Paths *P, int region,
                                              CrisisCombatUnit *atk,
                                              int maxn) {
  Side opp = opposite(M->my_side);
  int n = 0;
  for (int i = 0; i < S->warriors.len && n < maxn; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != opp) continue;
    int eta = enemy_eta_to_owned_base(S, M, P, w, region);
    if (eta < 0 || eta >= OWNED_BASE_CRISIS_SIM_DAYS) continue;
    atk[n].hp = max_int(1, w->hp);
    atk[n].num = w->id.num;
    atk[n].eta = eta;
    ++n;
  }
  return n;
}

static int collect_my_base_defense_schedule(const GameState *S,
                                            const GameMap *M,
                                            const Paths *P,
                                            const Actions *a, int region,
                                            const WarriorId *extra_ids,
                                            const int *extra_eta,
                                            int extra_n,
                                            CrisisCombatUnit *def,
                                            int maxn) {
  int n = 0;
  for (int i = 0; i < S->warriors.len && n < maxn; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    int eta = my_eta_to_owned_base(P, w, region);
    if (eta < 0 || eta >= OWNED_BASE_CRISIS_SIM_DAYS) continue;
    def[n].hp = max_int(1, w->hp);
    def[n].num = w->id.num;
    def[n].eta = eta;
    ++n;
  }

  for (int i = 0; i < a->moves.len && n < maxn; ++i) {
    if (a->moves.data[i].target != region) continue;
    const Warrior *w = find_warrior_const(S, a->moves.data[i].id);
    if (w == NULL || w->id.side != M->my_side) continue;
    int h = path_hops_between(P, w->region, region);
    if (h < 0 || h >= 1000000) continue;
    int eta = max_int(0, h - 1);
    if (eta >= OWNED_BASE_CRISIS_SIM_DAYS) continue;
    def[n].hp = max_int(1, w->hp);
    def[n].num = w->id.num;
    def[n].eta = eta;
    ++n;
  }

  for (int i = 0; i < extra_n && n < maxn; ++i) {
    const Warrior *w = find_warrior_const(S, extra_ids[i]);
    if (w == NULL || w->id.side != M->my_side) continue;
    int eta = extra_eta != NULL ? extra_eta[i] : 0;
    if (eta < 0 || eta >= OWNED_BASE_CRISIS_SIM_DAYS) continue;
    def[n].hp = max_int(1, w->hp);
    def[n].num = w->id.num;
    def[n].eta = eta;
    ++n;
  }
  return n;
}

static int owned_base_holds_scheduled_combat(const GameState *S,
                                             const GameMap *M,
                                             const Paths *P,
                                             const Actions *a,
                                             int region,
                                             const WarriorId *extra_ids,
                                             const int *extra_eta,
                                             int extra_n) {
  const Building *b = find_building_const(S, region);
  if (b == NULL || b->side != M->my_side || b->type != BTYPE_BASE) return 1;

  CrisisCombatUnit atk[MAX_COMBAT_SIM_UNITS];
  CrisisCombatUnit def[MAX_COMBAT_SIM_UNITS];
  int atk_n = collect_enemy_base_attack_schedule(S, M, P, region, atk,
                                                  MAX_COMBAT_SIM_UNITS);
  if (atk_n <= 0) return 1;
  int def_n = collect_my_base_defense_schedule(S, M, P, a, region,
                                                extra_ids, extra_eta, extra_n,
                                                def, MAX_COMBAT_SIM_UNITS);
  int building_hp = max_int(0, b->hp);
  int turret = building_turret_power_const(b);

  for (int day = 0; day < OWNED_BASE_CRISIS_SIM_DAYS; ++day) {
    int atk_attacks = crisis_alive_arrived_count(atk, atk_n, day);
    int def_attacks = crisis_alive_arrived_count(def, def_n, day);

    if (atk_attacks <= 0 && !crisis_future_arrivals(atk, atk_n, day)) return 1;
    if (atk_attacks <= 0) continue;

    for (int k = 0; k < turret; ++k)
      crisis_deal_one_to_weakest(atk, atk_n, day);

    /* Warrior attack counts are fixed by the number of warriors present in the
       region at the warrior-combat step.  A warrior reduced to 0 HP earlier in
       the same combat still contributes its attack count, so atk_attacks and
       def_attacks are intentionally measured before any combat damage below. */
    for (int k = 0; k < atk_attacks; ++k)
      crisis_enemy_attack_one(def, def_n, day, &building_hp);
    for (int k = 0; k < def_attacks; ++k)
      crisis_deal_one_to_weakest(atk, atk_n, day);

    if (building_hp <= 0) return 0;
    if (crisis_alive_arrived_count(atk, atk_n, day) <= 0 &&
        !crisis_future_arrivals(atk, atk_n, day))
      return 1;
  }

  return building_hp > 0 && crisis_alive_arrived_count(atk, atk_n,
           OWNED_BASE_CRISIS_SIM_DAYS - 1) <= 0;
}

typedef struct {
  WarriorId id;
  int hp;
  int region;
  int eta;
  int source_keep_tier;
  double dist;
} BaseDefenseCandidate;

static int cmp_base_defense_candidate(const void *pa, const void *pb) {
  const BaseDefenseCandidate *a = (const BaseDefenseCandidate *)pa;
  const BaseDefenseCandidate *b = (const BaseDefenseCandidate *)pb;
  if (a->eta != b->eta) return a->eta - b->eta;
  if (a->source_keep_tier != b->source_keep_tier)
    return a->source_keep_tier - b->source_keep_tier;
  if (a->dist < b->dist) return -1;
  if (a->dist > b->dist) return 1;
  if (a->region != b->region) return a->region - b->region;
  return a->id.num - b->id.num;
}

static void update_owned_base_crisis_flags(const GameState *S,
                                           const GameMap *M,
                                           const Paths *P) {
  for (int r = 0; r < 256; ++r) {
    if (g_owned_base_crisis[r] && !enemy_near_region(S, M, P, r,
            OWNED_BASE_CRISIS_RELEASE_RADIUS))
      g_owned_base_crisis[r] = 0;
  }
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side || b->type != BTYPE_BASE) continue;
    int r = b->region;
    if (r < 0 || r >= 256) continue;
    CrisisCombatUnit atk[MAX_COMBAT_SIM_UNITS];
    int atk_n = collect_enemy_base_attack_schedule(S, M, P, r, atk,
                                                    MAX_COMBAT_SIM_UNITS);
    if (atk_n > 0 || enemy_near_region(S, M, P, r, OWNED_BASE_CRISIS_RELEASE_RADIUS))
      g_owned_base_crisis[r] = 1;
    else
      g_owned_base_crisis[r] = 0;
  }
}

static int collect_owned_base_defense_candidates(const GameState *S,
                                                 const GameMap *M,
                                                 const Paths *P,
                                                 const Actions *a,
                                                 int target,
                                                 BaseDefenseCandidate *out,
                                                 int max_out) {
  int n = 0;
  for (int bi = 0; bi < S->buildings.len && n < max_out; ++bi) {
    const Building *src = &S->buildings.data[bi];
    if (src->side != M->my_side || src->type != BTYPE_BASE) continue;
    if (src->region == target) continue;
    if (src->region < 0 || src->region >= P->N) continue;
    if (src->region < 256 && g_owned_base_crisis[src->region]) continue;
    if (enemy_near_region(S, M, P, src->region, OWNED_BASE_CRISIS_RELEASE_RADIUS)) continue;

    int hops = path_hops_between(P, src->region, target);
    if (hops <= 0 || hops > OWNED_BASE_REINFORCE_SOURCE_RADIUS) continue;
    int eta = hops - 1;
    if (eta >= OWNED_BASE_CRISIS_SIM_DAYS) continue;
    if (full_path_enemy_blocked(S, M, P, src->region, target, 1)) continue;

    int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, src->region);
    int cap = planned_work_cap_at(S, M, a, src->region);
    int can_send_tier0 = max_int(0, remaining - max_int(1, cap));
    int can_send_tier1 = max_int(0, remaining - 1) - can_send_tier0;
    if (can_send_tier0 + can_send_tier1 <= 0) continue;

    int used_from_source = 0;
    for (int wi = 0; wi < S->warriors.len && n < max_out; ++wi) {
      const Warrior *w = &S->warriors.data[wi];
      if (w->id.side != M->my_side || w->region != src->region) continue;
      if (w->state != WSTATE_STATIONARY) continue;
      if (action_has_move_warrior(a, w->id)) continue;
      if (region_has_enemy_warrior(S, M, w->region)) continue;
      if (used_from_source >= can_send_tier0 + can_send_tier1) break;

      BaseDefenseCandidate c;
      c.id = w->id;
      c.hp = max_int(1, w->hp);
      c.region = w->region;
      c.eta = eta;
      c.source_keep_tier = used_from_source < can_send_tier0 ? 0 : 1;
      c.dist = P->dist[src->region][target];
      out[n++] = c;
      ++used_from_source;
    }
  }
  qsort(out, (size_t)n, sizeof(BaseDefenseCandidate), cmp_base_defense_candidate);
  return n;
}

static int choose_owned_base_crisis_target(const GameState *S,
                                           const GameMap *M,
                                           const Paths *P,
                                           const Actions *a,
                                           int *target_out) {
  int best = -1;
  double best_score = -INFINITY;
  update_owned_base_crisis_flags(S, M, P);

  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side || b->type != BTYPE_BASE) continue;
    int r = b->region;
    CrisisCombatUnit atk[MAX_COMBAT_SIM_UNITS];
    int atk_n = collect_enemy_base_attack_schedule(S, M, P, r, atk,
                                                    MAX_COMBAT_SIM_UNITS);
    if (atk_n <= 0) continue;
    if (owned_base_holds_scheduled_combat(S, M, P, a, r, NULL, NULL, 0)) continue;

    BaseDefenseCandidate cands[MAX_COMBAT_SIM_UNITS];
    int cn = collect_owned_base_defense_candidates(S, M, P, a, r, cands,
                                                   MAX_COMBAT_SIM_UNITS);
    WarriorId picked[OWNED_BASE_MAX_EMERGENCY_PULL];
    int picked_eta[OWNED_BASE_MAX_EMERGENCY_PULL];
    int picked_n = 0;
    int savable = 0;
    for (int i = 0; i < cn && picked_n < OWNED_BASE_MAX_EMERGENCY_PULL; ++i) {
      picked[picked_n] = cands[i].id;
      picked_eta[picked_n] = cands[i].eta;
      ++picked_n;
      if (owned_base_holds_scheduled_combat(S, M, P, a, r, picked,
                                            picked_eta, picked_n)) {
        savable = 1;
        break;
      }
    }
    if (!savable) continue;

    double score = 1000.0 * atk_n + 100.0 * b->level +
                   (double)(building_current_hp(b) - b->hp) -
                   0.01 * P->dist[M->my_hq][r];
    if (score > best_score) {
      best_score = score;
      best = r;
    }
  }
  if (best < 0) return 0;
  *target_out = best;
  return 1;
}

static int issue_owned_base_emergency_defense(Actions *a, const GameState *S,
                                              const GameMap *M, const Paths *P,
                                              int *budget) {
#if ENABLE_OWNED_BASE_EMERGENCY_DEFENSE
  int target = -1;
  if (!choose_owned_base_crisis_target(S, M, P, a, &target)) return 0;
  if (target >= 0 && target < 256) g_owned_base_crisis[target] = 1;

  BaseDefenseCandidate cands[MAX_COMBAT_SIM_UNITS];
  int cn = collect_owned_base_defense_candidates(S, M, P, a, target, cands,
                                                 MAX_COMBAT_SIM_UNITS);
  WarriorId picked[OWNED_BASE_MAX_EMERGENCY_PULL];
  int picked_eta[OWNED_BASE_MAX_EMERGENCY_PULL];
  int picked_n = 0;
  int issued = 0;

  for (int i = 0; i < cn && picked_n < OWNED_BASE_MAX_EMERGENCY_PULL; ++i) {
    picked[picked_n] = cands[i].id;
    picked_eta[picked_n] = cands[i].eta;
    ++picked_n;
    int flags = MOVE_FLAG_ALLOW_DANGER_TARGET | MOVE_FLAG_IGNORE_STACK_GUARD;
    if (add_move_action_ex_stack_flags(a, S, M, P, cands[i].id, target,
                                       budget, flags)) {
      ++issued;
      if (owned_base_holds_scheduled_combat(S, M, P, a, target, NULL, NULL, 0))
        break;
    }
  }

  update_owned_base_crisis_flags(S, M, P);
  return issued;
#else
  (void)a; (void)S; (void)M; (void)P; (void)budget;
  return 0;
#endif
}

static int issue_direct_hq_attack_guard(Actions *a, const GameState *S,
                                        const GameMap *M, const Paths *P,
                                        int *budget) {
  DirectHqThreat threat = direct_hq_threat_stats(S, M, P);
  if (threat.count <= 0) return 0;

  /* One enemy merely walking to HQ can be a stray.  Two or more direct HQ
     attackers is the rush pattern from the logs; any enemy already at HQ is
     urgent even alone. */
  if (threat.count < 2 && threat.at_hq == 0) return 0;

  int def_hp = 0;
  int def_cnt = direct_hq_current_defender_stats(S, M, a, &def_hp);

  /* Required defenders are exactly based on incoming warrior count and HP.
     We do not add tuning margins here because this is deliberately hardcoded. */
  int required_cnt = threat.count;
  int required_hp = threat.hp;

  int issued = 0;

  /* First use immediate HQ training, but only as much as the deficit actually
     needs.  Trained warriors appear at HQ this turn and participate in combat. */
  int train_hp = direct_hq_train_hp(S, M);
  int can_train = min_int(planned_train_cap(S, M, a), *budget / TRAIN_COST);
  int need_cnt = max_int(0, required_cnt - def_cnt);
  int need_hp = max_int(0, required_hp - def_hp);
  int train_by_hp = (need_hp + train_hp - 1) / train_hp;
  int train_n = min_int(can_train, max_int(need_cnt, train_by_hp));
  if (train_n > 0) {
    a->train_n = train_n;
    *budget -= TRAIN_COST * train_n;
    def_cnt += train_n;
    def_hp += train_hp * train_n;
    issued += train_n;
  }

  /* Then recall only the remaining needed defenders, closest first.  HQ recall
     to our own HQ is free, and stack guard is ignored because this is defense. */
  int max_cands = S->warriors.len;
  DirectHqDefenderCandidate *cands =
      (DirectHqDefenderCandidate *)malloc((size_t)max_cands * sizeof(*cands));
  int cand_n = 0;
  if (cands != NULL)
    cand_n = direct_hq_collect_recall_candidates(S, M, P, a, cands, max_cands);

  for (int i = 0; i < cand_n && (def_cnt < required_cnt || def_hp < required_hp); ++i) {
    int flags = MOVE_FLAG_ALLOW_CONTESTED_SOURCE | MOVE_FLAG_IGNORE_STACK_GUARD;
    if (add_move_action_ex_stack_flags(a, S, M, P, cands[i].id, M->my_hq,
                                       budget, flags)) {
      ++issued;
      ++def_cnt;
      def_hp += cands[i].hp;
    }
  }
  free(cands);

  /* Return positive even if no command was needed; otherwise the normal economy
     code may move away the defenders we just counted as sufficient. */
  return issued > 0 ? issued : 1;
}

static int conservative_expected_income(const GameState *S, const GameMap *M,
                                        const Actions *a, int train_n) {
  int income = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    int r = b->region;
    int cap = planned_work_cap_at(S, M, a, r);
    int workers = planned_workers_physically_remaining_at(S, a, M->my_side, r);
    if (r == M->my_hq) workers += train_n;
    income += WORK_INCOME * min_int(workers, cap);
  }
  for (int i = 0; i < a->upgrades.len; ++i) {
    int r = a->upgrades.data[i];
    if (find_building_const(S, r) == NULL) {
      int workers = planned_workers_physically_remaining_at(S, a, M->my_side, r);
      income += WORK_INCOME * min_int(workers, BASE_LEVELS[1].work_cap);
    }
  }
  return income;
}


static int final_hq5_repair_reserve(const GameState *S, const GameMap *M,
                                    const Actions *a, int turn) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (g_hq5_repair_reserve_budget_excluded) return 0;
  if (hq->level < HQ_MAX_LEVEL) return 0;
  if (action_has_upgrade(a, M->my_hq)) return 0;

  /* On turn 200, if HQ is already full, there is no later morning on which the
     saved 1000 gold can be converted into HP.  Before turn 200, reserve the
     repair money even when HQ is currently full, because the opponent may still
     damage it before the final construction phase. */
  if (turn >= MAX_TURN) {
    return (hq->hp < HQ_LEVELS[HQ_MAX_LEVEL].hp) ? FINAL_HQ5_REPAIR_RESERVE_GOLD : 0;
  }
  return FINAL_HQ5_REPAIR_RESERVE_GOLD;
}

static int choose_final_hq5_cash_dump_extra_train(const GameState *S,
                                                  const GameMap *M,
                                                  const Actions *a,
                                                  int budget, int turn) {
  if (turn < FINAL_HQ5_CASH_DUMP_START_TURN) return 0;
  if (budget < FINAL_HQ5_MIN_TRAIN_GOLD) return 0;

  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (hq->level < HQ_MAX_LEVEL) return 0;

  int cap = planned_train_cap(S, M, a);
  if (cap <= a->train_n) return 0;

  int repair_reserve = final_hq5_repair_reserve(S, M, a, turn);
  int remaining_supply_days = MAX_TURN - turn + 1;
  if (remaining_supply_days < 1) remaining_supply_days = 1;

  int base_income = conservative_expected_income(S, M, a, a->train_n);

  for (int total = cap; total > a->train_n; --total) {
    int extra = total - a->train_n;
    int after_gold = budget - TRAIN_COST * extra;
    if (after_gold < 0) continue;

    int alive_after = count_side_warriors(S, M->my_side) + total;
    int income = conservative_expected_income(S, M, a, total);

    /* Before hard dump starts, keep the check slightly conservative by not
       assuming income growth from newly trained HQ workers beyond the current
       conservative income.  From 185 onward, there is little time left for gold
       to compound, so use the planned income with the trained bodies included. */
    if (turn < FINAL_HQ5_HARD_DUMP_START_TURN)
      income = min_int(income, base_income);

    long long projected = (long long)after_gold +
                          (long long)income * remaining_supply_days -
                          (long long)UPKEEP_PER_WARRIOR * alive_after * remaining_supply_days;
    if (projected >= repair_reserve)
      return extra;
  }
  return 0;
}


static int issue_hq5_excess_train(Actions *a, const GameState *S,
                                  const GameMap *M, int *budget, int turn) {
  (void)turn;
  if (a->train_n > 0) return 0;
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->level < HQ_MAX_LEVEL) return 0;

  int reserve = g_hq5_repair_reserve_budget_excluded ? 0 : HQ_HEAL_COST;
  if (*budget < reserve + TRAIN_COST) return 0;

  int cap = planned_train_cap(S, M, a);
  int n = min_int(cap, (*budget - reserve) / TRAIN_COST);
  if (n <= 0) return 0;
  a->train_n = n;
  *budget -= TRAIN_COST * n;
  return n;
}

static int issue_final_hq5_cash_dump_train(Actions *a, const GameState *S,
                                           const GameMap *M, int *budget,
                                           int turn) {
  int extra = choose_final_hq5_cash_dump_extra_train(S, M, a, *budget, turn);
  if (extra <= 0) return 0;
  a->train_n += extra;
  *budget -= TRAIN_COST * extra;
  return extra;
}

static int choose_train_count(const GameState *S, const GameMap *M,
                              const Actions *a, int budget,
                              int upgrade_mode, int turn) {
  int train_cap = planned_train_cap(S, M, a);
  if (train_cap <= 0) return 0;
  int alive = count_side_warriors(S, M->my_side);
  int work_cap = planned_total_work_cap(S, M, a);

  int cash_dump_train = 0;
  int final_hq5_cash_dump = (turn >= FINAL_HQ5_CASH_DUMP_START_TURN &&
                            my_hq_level(S, M) >= HQ_MAX_LEVEL);
  if (final_hq5_cash_dump) {
    cash_dump_train = 1;
  } else if (turn >= CASH_DUMP_TRAIN_START_TURN && budget >= CASH_DUMP_MIN_GOLD &&
             !late_hq_tiebreak_should_save(S, M, a, budget, turn)) {
    cash_dump_train = 1;
  }

  int limit_by_rule = train_cap;
  if (upgrade_mode && !cash_dump_train) {
    int missing_workers = max_int(0, work_cap - alive);
    limit_by_rule = min_int(limit_by_rule, missing_workers);
  }

  int reserve = (upgrade_mode || cash_dump_train) ? 0 : (BUILD_RESERVE + EXPANSION_MOVE_RESERVE);
  for (int n = limit_by_rule; n >= 0; --n) {
    int after_gold = budget - TRAIN_COST * n;
    if (after_gold < reserve) continue;

    int income = conservative_expected_income(S, M, a, n);
    int upkeep = UPKEEP_PER_WARRIOR * (alive + n);
    if (final_hq5_cash_dump) {
      int remaining_supply_days = MAX_TURN - turn + 1;
      if (remaining_supply_days < 1) remaining_supply_days = 1;
      int repair_reserve = final_hq5_repair_reserve(S, M, a, turn);
      long long projected = (long long)after_gold +
                            (long long)income * remaining_supply_days -
                            (long long)upkeep * remaining_supply_days;
      if (projected >= repair_reserve)
        return n;
    } else if (after_gold + income - upkeep >= 0) {
      return n;
    }
  }
  return 0;
}



static int hp_ratio_train_priority_active(const GameState *S, const GameMap *M,
                                          int turn) {
#if !ENABLE_HP_RATIO_TRAIN_PRIORITY
  (void)S;
  (void)M;
  (void)turn;
  return 0;
#else
  if (turn > HP_RATIO_TRAIN_PRIORITY_MAX_TURN) return 0;
  if (HP_RATIO_TRAIN_PRIORITY_X_DEN <= 0) return 0;

  int my_hp = side_total_warrior_hp(S, M->my_side);
  int enemy_hp = side_total_warrior_hp(S, opposite(M->my_side));
  if (enemy_hp < HP_RATIO_TRAIN_PRIORITY_MIN_ENEMY_HP) return 0;

  return (long long)my_hp * HP_RATIO_TRAIN_PRIORITY_X_DEN <=
         (long long)enemy_hp * HP_RATIO_TRAIN_PRIORITY_X_NUM;
#endif
}

static int choose_hp_ratio_priority_train_count(const GameState *S,
                                                const GameMap *M,
                                                const Actions *a,
                                                int budget, int turn) {
  if (!hp_ratio_train_priority_active(S, M, turn)) return 0;

  int cap = planned_train_cap(S, M, a);
  if (cap <= 0 || budget < TRAIN_COST) return 0;

  int limit = min_int(cap, budget / TRAIN_COST);
  limit = min_int(limit, HP_RATIO_TRAIN_PRIORITY_MAX_PER_TURN);

#if HP_RATIO_TRAIN_PRIORITY_KEEP_SOLVENT
  int alive = count_side_warriors(S, M->my_side);
  for (int n = limit; n >= 1; --n) {
    int after_gold = budget - TRAIN_COST * n;
    int income = conservative_expected_income(S, M, a, n);
    int upkeep = UPKEEP_PER_WARRIOR * (alive + n);
    if (after_gold + income - upkeep >= 0) return n;
  }
  return 0;
#else
  return limit;
#endif
}

static int issue_hp_ratio_train_priority(Actions *a, const GameState *S,
                                         const GameMap *M, int *budget,
                                         int turn) {
  if (a->train_n > 0) return 0;
  int n = choose_hp_ratio_priority_train_count(S, M, a, *budget, turn);
  if (n <= 0) return 0;
  a->train_n = n;
  *budget -= TRAIN_COST * n;
  return n;
}

static int thin_opening_pending_neutral_build_count(const GameState *S,
                                                    const GameMap *M,
                                                    const Actions *a);

static int thin_opening_before_enemy_rush_window(const GameMap *M,
                                                const Paths *P, int turn) {
#if ENABLE_THIN_OPENING_GRAB
  int h = path_hops_between(P, M->opp_hq, M->my_hq);
  if (h >= INF_HOPS) return 1;
  return turn + THIN_OPENING_RUSH_SAFETY_TURNS < h;
#else
  (void)M; (void)P; (void)turn;
  return 0;
#endif
}

static int thin_opening_should_continue(const GameState *S, const GameMap *M,
                                        const Paths *P, const Actions *a,
                                        int turn) {
#if ENABLE_THIN_OPENING_GRAB
  if (turn > THIN_OPENING_MAX_TURN) return 0;
  if (!opening_neutral_empty_remaining(S, M, a) &&
      thin_opening_pending_neutral_build_count(S, M, a) <= 0)
    return 0;
  if (opening_neutral_should_abort_for_pressure(S, M, P)) return 0;
  if (!thin_opening_before_enemy_rush_window(M, P, turn)) return 0;
  return 1;
#else
  (void)S; (void)M; (void)P; (void)a; (void)turn;
  return 0;
#endif
}

static int thin_opening_source_surplus_after_plan(const GameState *S,
                                                  const GameMap *M,
                                                  const Actions *a,
                                                  int region) {
  if (!planned_my_building(S, M, a, region)) return 0;
  if (region_has_enemy_warrior(S, M, region)) return 0;

  /* Thin opening rule: every owned HQ/base keeps exactly one body.  The normal
     work_cap+extra-garrison rule is intentionally suspended only in this phase. */
  int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, region);
  return max_int(0, remaining - 1);
}

static int pick_thin_opening_warrior_from_region(const GameState *S,
                                                 const GameMap *M,
                                                 const Actions *a,
                                                 int region,
                                                 WarriorId *out) {
  if (thin_opening_source_surplus_after_plan(S, M, a, region) <= 0) return 0;
  for (int i = S->warriors.len - 1; i >= 0; --i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side || w->region != region) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    *out = w->id;
    return 1;
  }
  return 0;
}

static int choose_thin_opening_source_for_target(const GameState *S,
                                                 const GameMap *M,
                                                 const Paths *P,
                                                 const Actions *a,
                                                 int target,
                                                 WarriorId *out) {
  double best = INFINITY;
  WarriorId best_id = {M->my_side, -1};

  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *src = &S->buildings.data[bi];
    if (src->side != M->my_side) continue;
    if (src->region == target) continue;
    if (P->nxt[src->region][target] == -1) continue;
    if (thin_opening_source_surplus_after_plan(S, M, a, src->region) <= 0) continue;

    WarriorId cand;
    if (!pick_thin_opening_warrior_from_region(S, M, a, src->region, &cand)) continue;
    double d = P->dist[src->region][target];
    if (d < best) {
      best = d;
      best_id = cand;
    }
  }

  if (best_id.num < 0) return 0;
  *out = best_id;
  return 1;
}

static int choose_thin_opening_neutral_target(const GameState *S,
                                              const GameMap *M,
                                              const Paths *P,
                                              const Actions *a,
                                              int *target_out,
                                              WarriorId *id_out) {
  double best_score = INFINITY;
  int best_target = -1;
  WarriorId best_id = {M->my_side, -1};

  for (int ti = 0; ti < M->strongholds.len; ++ti) {
    int target = M->strongholds.data[ti];
    if (find_building_const(S, target) != NULL) continue;
    if (neutral_target_already_claimed(S, M, a, target)) continue;
    if (region_has_enemy_warrior(S, M, target)) continue;
    if (P != NULL && enemy_projected_stack_count_at(S, M, P, target) > 0) continue;

    WarriorId id;
    if (!choose_thin_opening_source_for_target(S, M, P, a, target, &id)) continue;
    const Warrior *w = find_warrior_const(S, id);
    if (w == NULL) continue;

    double hq_dist = P->dist[M->my_hq][target];
    double source_dist = P->dist[w->region][target];
    double forward = M->my_side == SIDE_LEFT ? target : (M->N - 1 - target);
    double score = 1000000000.0 * hq_dist + 1000.0 * forward + source_dist;
    if (score < best_score) {
      best_score = score;
      best_target = target;
      best_id = id;
    }
  }

  if (best_target < 0) return 0;
  *target_out = best_target;
  *id_out = best_id;
  return 1;
}

static int thin_opening_pending_neutral_build_count(const GameState *S,
                                                    const GameMap *M,
                                                    const Actions *a) {
  int cnt = 0;
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (action_has_upgrade(a, r)) continue;
    if (neutral_target_already_claimed(S, M, a, r))
      ++cnt;
  }
  return cnt;
}

static int thin_opening_train_count(const GameState *S, const GameMap *M,
                                    const Paths *P, const Actions *a,
                                    int budget, int turn) {
  (void)turn;
  if (!opening_neutral_empty_remaining(S, M, a)) return 0;
  int cap = planned_train_cap(S, M, a);
  if (cap <= 0) return 0;

  /* For neutral expansion, train at most one body and only when the HQ->target
     travel time plus current income means the base construction gold is ready
     as the warrior arrives.  This avoids paying upkeep for a soldier that will
     stand on an empty stronghold waiting for 300 gold. */
  int after_gold = budget - TRAIN_COST;
  if (after_gold < 0) return 0;
  int target = choose_trainable_timed_neutral_train_target(S, M, P, a, budget, 1);
  if (target < 0) return 0;
  return 1;
}

static int issue_thin_opening_grab_phase(Actions *a, const GameState *S,
                                         const GameMap *M, const Paths *P,
                                         int *budget, int turn) {
#if ENABLE_THIN_OPENING_GRAB
  (void)turn;
  int did = 0;

  /* Build only new neutral bases.  Existing HQ/base level-ups are deliberately
     skipped while this phase is active. */
  while (*budget >= BASE_LEVELS[1].cost) {
    int best_region = -1;
    double best_score = INFINITY;
    for (int i = 0; i < M->strongholds.len; ++i) {
      int r = M->strongholds.data[i];
      if (action_has_upgrade(a, r)) continue;
      if (!legal_build_neutral_now(S, M, r)) continue;
      double d = P->dist[M->my_hq][r];
      double forward = M->my_side == SIDE_LEFT ? r : (M->N - 1 - r);
      double score = d + 0.001 * forward;
      if (score < best_score) {
        best_score = score;
        best_region = r;
      }
    }
    if (best_region < 0) break;
    if (!add_upgrade_action(a, best_region)) break;
    *budget -= BASE_LEVELS[1].cost;
    did = 1;
  }

  /* Pre-position one body per empty neutral stronghold without reserving the
     future 300-gold construction cost.  This is the main v29 change. */
  while (*budget >= MOVE_COST) {
    int target = -1;
    WarriorId id;
    if (!choose_thin_opening_neutral_target(S, M, P, a, &target, &id)) break;
    if (!add_move_action(a, S, M, id, target, budget)) break;
    did = 1;
  }

  if (a->train_n == 0) {
    int n = thin_opening_train_count(S, M, P, a, *budget, turn);
    if (n > 0) {
      a->train_n = n;
      *budget -= TRAIN_COST * n;
      did = 1;
    }
  }

  if (!opening_neutral_empty_remaining(S, M, a) &&
      thin_opening_pending_neutral_build_count(S, M, a) <= 0)
    g_opening_neutral_done = 1;

  return did;
#else
  (void)a; (void)S; (void)M; (void)P; (void)budget; (void)turn;
  return 0;
#endif
}

/* ---- Hardcoded advantage conversion: stop drawing won games ----
   When we already own the map, the normal GA-tuned policy can keep training or
   walking to side objectives until the turn limit.  This block is deliberately
   not parameterized.  It does three things:
     1) build any captured stronghold immediately;
     2) when ahead, save gold for HQ/base level-ups and train only the minimum
        needed for safety;
     3) clear remaining enemy bases and, once the path is clear, gather the
        exactly needed attackers at one sticky middle base before attacking the
        enemy HQ. */

typedef struct {
  WarriorId id;
  int region;
  int hp;
  int eta;
  int from_owned;
} AdvWarriorPick;

static int cmp_adv_pick_stage(const void *pa, const void *pb) {
  const AdvWarriorPick *a = (const AdvWarriorPick *)pa;
  const AdvWarriorPick *b = (const AdvWarriorPick *)pb;
  if (a->eta != b->eta) return a->eta - b->eta;
  if (a->from_owned != b->from_owned) return b->from_owned - a->from_owned;
  if (a->hp != b->hp) return b->hp - a->hp;
  return a->id.num - b->id.num;
}

static int cmp_adv_pick_launch(const void *pa, const void *pb) {
  const AdvWarriorPick *a = (const AdvWarriorPick *)pa;
  const AdvWarriorPick *b = (const AdvWarriorPick *)pb;
  if (a->hp != b->hp) return b->hp - a->hp;
  return a->id.num - b->id.num;
}

static int hard_side_base_count(const GameState *S, Side side) {
  int cnt = 0;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side == side && b->type == BTYPE_BASE) ++cnt;
  }
  return cnt;
}

static int hard_side_building_count(const GameState *S, Side side) {
  int cnt = 0;
  for (int i = 0; i < S->buildings.len; ++i)
    if (S->buildings.data[i].side == side) ++cnt;
  return cnt;
}

static int hard_side_stationary_count(const GameState *S, Side side, int region) {
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == side && w->state == WSTATE_STATIONARY && w->region == region)
      ++cnt;
  }
  return cnt;
}

static int hard_advantage_phase_active(const GameState *S, const GameMap *M,
                                       int turn) {
  int my_bases = hard_side_base_count(S, M->my_side);
  int enemy_bases = hard_side_base_count(S, opposite(M->my_side));
  int my_cnt = count_side_warriors(S, M->my_side);
  int enemy_cnt = count_side_warriors(S, opposite(M->my_side));
  int my_hp = side_total_warrior_hp(S, M->my_side);
  int enemy_hp = side_total_warrior_hp(S, opposite(M->my_side));

  /* Hardcoded conversion starts earlier than the generic endgame: once the
     opponent's normal bases are gone, do not drift into TRAIN/neutral loops. */
  if (turn >= 105 && enemy_bases == 0 && my_bases >= 2) return 1;
  if (turn >= 120 && enemy_bases <= 1 && my_bases >= enemy_bases + 4) return 1;
  if (turn >= 135 && my_bases >= enemy_bases + 5 && my_cnt >= enemy_cnt + 5) return 1;
  if (turn >= 150 && my_bases >= enemy_bases + 4 && my_hp >= enemy_hp + 20) return 1;
  if (turn >= 165 && my_bases >= enemy_bases + 3 && my_cnt >= enemy_cnt + 3) return 1;
  return 0;
}

static int issue_hard_build_occupied_neutral(Actions *a, const GameState *S,
                                             const GameMap *M, int *budget) {
  int best = -1;
  double best_score = INFINITY;
  for (int i = 0; i < M->strongholds.len; ++i) {
    int r = M->strongholds.data[i];
    if (find_building_const(S, r) != NULL) continue;
    if (action_has_upgrade(a, r)) continue;
    if (count_warriors_at(S, M->my_side, r) <= 0) continue;
    if (region_has_enemy_warrior(S, M, r)) continue;
    double score = fabs((double)r - (double)M->opp_hq);
    if (score < best_score) {
      best_score = score;
      best = r;
    }
  }
  if (best < 0 || *budget < BASE_LEVELS[1].cost) return 0;
  add_upgrade_action(a, best);
  *budget -= BASE_LEVELS[1].cost;
  g_advantage_stage_region = best;
  g_advantage_stage_target = -1;
  return 1;
}

static int issue_hard_hq_priority_if_possible(Actions *a, const GameState *S,
                                              const GameMap *M, const Paths *P,
                                              int *budget, int turn) {
  int issued = 0;
  ensure_hq_upgrade_worker(a, S, M, P, budget);
  ensure_late_hq_tiebreak_worker(a, S, M, P, budget, turn);
  if (issue_hq_upgrade_if_affordable(a, S, M, budget)) issued = 1;
  if (!issued && issue_late_hq_tiebreak_if_affordable(a, S, M, budget, turn)) issued = 1;
  return issued;
}

static int hard_should_save_for_hq_now(const GameState *S, const GameMap *M,
                                       const Actions *a, int budget, int turn) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (hq->level < HQ_MAX_LEVEL) {
    int cost = building_upgrade_cost(hq);
    int income = conservative_expected_income(S, M, a, 0);
    int upkeep = UPKEEP_PER_WARRIOR * count_side_warriors(S, M->my_side);
    int net = income - upkeep;
    if (budget >= cost) return 1;
    if (net > 0 && turn <= 190 && budget + 8 * net >= cost) return 1;
  }
  if (hq->level == HQ_MAX_LEVEL && hq->hp < HQ_LEVELS[hq->level].hp) {
    int income = conservative_expected_income(S, M, a, 0);
    int upkeep = UPKEEP_PER_WARRIOR * count_side_warriors(S, M->my_side);
    int net = income - upkeep;
    if (budget >= HQ_HEAL_COST) return 1;
    if (net > 0 && turn <= 195 && budget + 6 * net >= HQ_HEAL_COST) return 1;
  }
  return 0;
}

static int issue_hard_limited_train(Actions *a, const GameState *S,
                                    const GameMap *M, int *budget) {
  if (a->train_n > 0) return 0;
  if (hq5_base2_savings_needed_before_train(S, M, a, *budget)) return 0;
  int my_cnt = count_side_warriors(S, M->my_side);
  int enemy_cnt = count_side_warriors(S, opposite(M->my_side));
  int my_hp = side_total_warrior_hp(S, M->my_side);
  int enemy_hp = side_total_warrior_hp(S, opposite(M->my_side));
  int desired;
  if (enemy_normal_bases_cleared_now(S, M)) {
    /* After all enemy ordinary bases are gone, preserve gold for HQ level-up
       and keep only enough population to cover the remaining enemy warriors. */
    desired = enemy_cnt + 1;
    if (enemy_hp > my_hp) desired += 1;
    if (desired < 3) desired = 3;
  } else {
    desired = enemy_cnt + 2;
    if (enemy_hp > my_hp) desired += 2;
    if (desired < 4) desired = 4;
  }
  if (my_cnt >= desired) return 0;
  int need = desired - my_cnt;
  int cap = planned_train_cap(S, M, a);
  int n = min_int(need, min_int(cap, *budget / TRAIN_COST));
  if (n <= 0) return 0;
  a->train_n = n;
  *budget -= TRAIN_COST * n;
  return n;
}

static int hard_valid_stage(const GameState *S, const GameMap *M,
                            const Paths *P, int stage) {
  if (stage < 0 || stage >= M->N) return 0;
  const Building *b = find_building_const(S, stage);
  if (b == NULL || b->side != M->my_side) return 0;
  if (region_has_enemy_warrior(S, M, stage)) return 0;
  if (P->nxt[stage][M->opp_hq] == -1) return 0;
  return 1;
}

static int choose_hard_stage_base(const GameState *S, const GameMap *M,
                                  const Paths *P) {
  if (hard_valid_stage(S, M, P, g_advantage_stage_region))
    return g_advantage_stage_region;

  int best = -1;
  double best_score = INFINITY;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side != M->my_side) continue;
    if (b->type == BTYPE_HQ && hard_side_base_count(S, M->my_side) > 0) continue;
    if (!hard_valid_stage(S, M, P, b->region)) continue;
    double d_opp = P->dist[b->region][M->opp_hq];
    double d_home = P->dist[M->my_hq][b->region];
    int st = hard_side_stationary_count(S, M->my_side, b->region);
    double score = d_opp - 0.20 * d_home - 500.0 * st;
    if (score < best_score) {
      best_score = score;
      best = b->region;
    }
  }
  if (best >= 0) g_advantage_stage_region = best;
  return best;
}

static int choose_hard_stage_target(const GameState *S, const GameMap *M,
                                    const Paths *P, int stage) {
  Side opp = opposite(M->my_side);
  if (g_advantage_stage_target >= 0) {
    const Building *tb = find_building_const(S, g_advantage_stage_target);
    if (tb != NULL && tb->side == opp && tb->type == BTYPE_BASE &&
        attack_path_first_enemy_is_target(S, M, P, stage, g_advantage_stage_target))
      return g_advantage_stage_target;
    if (g_advantage_stage_target == M->opp_hq) {
      const Building *hq = find_building_const(S, M->opp_hq);
      if (hq != NULL && hq->side == opp && attack_path_first_enemy_is_target(S, M, P, stage, M->opp_hq))
        return M->opp_hq;
    }
  }

  int first = first_enemy_building_on_path(S, M, P, stage, M->opp_hq);
  if (first >= 0) {
    g_advantage_stage_target = first;
    return first;
  }

  int best = -1;
  double best_score = INFINITY;
  for (int i = 0; i < S->buildings.len; ++i) {
    const Building *b = &S->buildings.data[i];
    if (b->side != opp || b->type != BTYPE_BASE) continue;
    if (!attack_path_first_enemy_is_target(S, M, P, stage, b->region)) continue;
    double score = P->dist[stage][b->region] + 0.001 * P->dist[b->region][M->opp_hq];
    if (score < best_score) {
      best_score = score;
      best = b->region;
    }
  }
  if (best < 0) best = M->opp_hq;
  g_advantage_stage_target = best;
  return best;
}

static int collect_stage_stationary(const GameState *S, const GameMap *M,
                                    const Actions *a, int stage,
                                    AdvWarriorPick *out, int maxn) {
  int n = 0;
  for (int i = 0; i < S->warriors.len && n < maxn; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->region != stage || w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    AdvWarriorPick p;
    p.id = w->id;
    p.region = w->region;
    p.hp = max_int(1, w->hp);
    p.eta = 0;
    p.from_owned = 1;
    out[n++] = p;
  }
  qsort(out, (size_t)n, sizeof(AdvWarriorPick), cmp_adv_pick_launch);
  return n;
}

static int count_inbound_to_stage(const GameState *S, const GameMap *M,
                                  int stage) {
  int cnt = 0;
  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side == M->my_side && w->state == WSTATE_MOVING && w->target == stage)
      ++cnt;
  }
  return cnt;
}

static int collect_stage_candidates(const GameState *S, const GameMap *M,
                                    const Paths *P, const Actions *a,
                                    int stage, int turn, AdvWarriorPick *out, int maxn) {
  int n = 0;
  for (int i = 0; i < S->warriors.len && n < maxn; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (w->region == stage) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (region_has_enemy_warrior(S, M, w->region)) continue;
    const Building *src = find_building_const(S, w->region);
    if (src == NULL || src->side != M->my_side) continue;
    int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, w->region);
    int keep = min_int(building_work_cap(src), remaining);
    if (turn >= 175) keep = min_int(1, remaining); /* late: finish the game */
    (void)keep;
    int already_taken = 0;
    for (int j = 0; j < n; ++j) if (out[j].region == w->region) ++already_taken;
    if (remaining - already_taken <= keep) continue;
    int eta = path_hops_between(P, w->region, stage);
    if (eta >= INF_HOPS) continue;
    AdvWarriorPick p;
    p.id = w->id;
    p.region = w->region;
    p.hp = max_int(1, w->hp);
    p.eta = eta;
    p.from_owned = 1;
    out[n++] = p;
  }
  qsort(out, (size_t)n, sizeof(AdvWarriorPick), cmp_adv_pick_stage);
  return n;
}

static int minimal_attackers_to_win_from_stage(const GameState *S,
                                               const GameMap *M,
                                               int target,
                                               const AdvWarriorPick *stage,
                                               int stage_n) {
  int hp[MAX_COMBAT_SIM_UNITS];
  int lim = min_int(stage_n, MAX_COMBAT_SIM_UNITS);
  for (int k = 1; k <= lim; ++k) {
    for (int i = 0; i < k; ++i) hp[i] = stage[i].hp;
    if (combat_simulation_win(S, M, target, hp, k)) {
      if (target == M->opp_hq) {
        /* HQ finish must not be exact-edge.  Add a fixed two-body margin and
           never launch a tiny HQ wave. */
        int need = k + 2;
        if (need < 8) need = 8;
        if (need > lim) need = lim;
        return need;
      }
      return max_int(k, MIN_ATTACK_WAVE_UNITS);
    }
  }
  return 1000000;
}

static int target_is_hq_allowed_now(const GameState *S, const GameMap *M,
                                    const Actions *a, int turn) {
  const Building *my_hq = find_building_const(S, M->my_hq);
  const Building *opp_hq = find_building_const(S, M->opp_hq);
  if (my_hq == NULL || opp_hq == NULL) return 0;
  if (hard_side_base_count(S, opposite(M->my_side)) > 0) return 0;
  if (my_hq->level < HQ_MAX_LEVEL) return 0;
  if (hard_should_save_for_hq_now(S, M, a, 0, turn)) return 0;
  if (turn < 145) return 0;
  return 1;
}

static int issue_hard_staged_attack(Actions *a, const GameState *S,
                                    const GameMap *M, const Paths *P,
                                    int *budget, int turn) {
#if ENABLE_ANCHOR_ROUTE_ATTACKS && ANCHOR_ROUTE_STRICT_OFFENSE_ONLY
  /* Strict anchor mode: do not use the separate hard-stage attacker.  It can
     launch from a non-anchor stage and violate the single-anchor/min-stack
     rule; enemy-base attacks are routed through issue_enemy_base_captures(). */
  (void)a; (void)S; (void)M; (void)P; (void)budget; (void)turn;
  return 0;
#else
  int stage = choose_hard_stage_base(S, M, P);
  if (stage < 0) return 0;
  int target = choose_hard_stage_target(S, M, P, stage);
  if (target < 0) return 0;

  const Building *tb = find_building_const(S, target);
  if (tb == NULL || tb->side == M->my_side) {
    g_advantage_stage_target = -1;
    return 0;
  }
  if (tb->type == BTYPE_HQ && !target_is_hq_allowed_now(S, M, a, turn))
    return 0;

  AdvWarriorPick stage_picks[MAX_COMBAT_SIM_UNITS];
  int stage_n = collect_stage_stationary(S, M, a, stage, stage_picks, MAX_COMBAT_SIM_UNITS);
  int need = minimal_attackers_to_win_from_stage(S, M, target, stage_picks, stage_n);

  /* If the current stack cannot win, estimate from the best possible assembled
     stack.  This determines how many bodies should gather; it prevents 30+ units
     from being pulled to the middle when 9 or 12 are enough. */
  if (need >= 1000000) {
    AdvWarriorPick all[MAX_COMBAT_SIM_UNITS];
    int all_n = 0;
    for (int i = 0; i < stage_n && all_n < MAX_COMBAT_SIM_UNITS; ++i) all[all_n++] = stage_picks[i];
    AdvWarriorPick cands[MAX_COMBAT_SIM_UNITS];
    int cand_n = collect_stage_candidates(S, M, P, a, stage, turn, cands, MAX_COMBAT_SIM_UNITS);
    for (int i = 0; i < cand_n && all_n < MAX_COMBAT_SIM_UNITS; ++i) all[all_n++] = cands[i];
    qsort(all, (size_t)all_n, sizeof(AdvWarriorPick), cmp_adv_pick_launch);
    need = minimal_attackers_to_win_from_stage(S, M, target, all, all_n);
    if (need >= 1000000) return 0;
  }

  int inbound = count_inbound_to_stage(S, M, stage);
  if (stage_n >= need) {
    if (*budget < need * MOVE_COST) return 0;
    int sent = 0;
    for (int i = 0; i < stage_n && sent < need; ++i) {
      if (add_move_action_ex_stack_flags(a, S, M, P, stage_picks[i].id, target,
                                         budget, MOVE_FLAG_ALLOW_DANGER_TARGET))
        ++sent;
    }
    if (sent == need) {
      g_advantage_stage_target = target;
      return sent;
    }
    return 0;
  }

  if (stage_n + inbound >= need) return 1; /* wait; do not overstack */

  int missing = need - stage_n - inbound;
  AdvWarriorPick cands[MAX_COMBAT_SIM_UNITS];
  int cand_n = collect_stage_candidates(S, M, P, a, stage, turn, cands, MAX_COMBAT_SIM_UNITS);
  int moved = 0;
  for (int i = 0; i < cand_n && moved < missing; ++i) {
    if (add_move_action_ex_stack_flags(a, S, M, P, cands[i].id, stage, budget,
                                       MOVE_FLAG_ALLOW_CONTESTED_SOURCE |
                                           MOVE_FLAG_IGNORE_STACK_GUARD))
      ++moved;
  }
  return moved > 0 ? moved : 1;
#endif
}


static int action_plan_changed_since(const Actions *a,
                                     int moves_before,
                                     int upgrades_before,
                                     int train_before) {
  return a->moves.len != moves_before ||
         a->upgrades.len != upgrades_before ||
         a->train_n != train_before;
}

static int hard_hq5_exclude_repair_reserve_for_surplus(const GameState *S,
                                                       const GameMap *M,
                                                       const Actions *a,
                                                       int *budget,
                                                       int min_action_gold) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (hq->level < HQ_MAX_LEVEL) return 0;

  /* If an HQ repair/upgrade was already issued this turn, its cost has already
     been removed from budget.  The remaining gold can be used normally. */
  if (action_has_upgrade(a, M->my_hq)) {
    g_allow_base_upgrades_after_enemy_baseclear = 1;
    return *budget >= min_action_gold;
  }

  if (g_hq5_repair_reserve_budget_excluded) {
    g_allow_base_upgrades_after_enemy_baseclear = 1;
    return *budget >= min_action_gold;
  }

  if (*budget < FINAL_HQ5_REPAIR_RESERVE_GOLD + min_action_gold) return 0;
  *budget -= FINAL_HQ5_REPAIR_RESERVE_GOLD;
  g_hq5_repair_reserve_budget_excluded = 1;
  g_allow_base_upgrades_after_enemy_baseclear = 1;
  return 1;
}

static int issue_base_level1_to_2_upgrades(Actions *a, const GameState *S,
                                             const GameMap *M, int *budget) {
  int did = 0;
  while (1) {
    int best_region = -1;
    int best_score = 1000000000;
    for (int bi = 0; bi < S->buildings.len; ++bi) {
      const Building *b = &S->buildings.data[bi];
      if (b->side != M->my_side) continue;
      if (b->type != BTYPE_BASE) continue;
      if (b->level != 1) continue;
      if (action_has_upgrade(a, b->region)) continue;
      if (!legal_upgrade_or_build_now(S, M, b->region)) continue;
      int cost = building_upgrade_cost(b);
      if (cost > *budget) continue;

      int forward = M->my_side == SIDE_LEFT ? b->region : (M->N - 1 - b->region);
      int have = planned_workers_committed_to_region_for_labor(S, M, a, M->my_side, b->region);
      int cap = planned_work_cap_at(S, M, a, b->region);
      int fill_bonus = (have >= cap) ? -100000 : 0;
      int score = fill_bonus + cost * 10 + forward;
      if (score < best_score) {
        best_score = score;
        best_region = b->region;
      }
    }
    if (best_region < 0) break;
    const Building *b = find_building_const(S, best_region);
    if (b == NULL) break;
    int cost = building_upgrade_cost(b);
    if (cost > *budget) break;
    if (!add_upgrade_action(a, best_region)) break;
    *budget -= cost;
    did = 1;
  }
  return did;
}

static int level1_base_upgrade_candidate_exists(const GameState *S,
                                                const GameMap *M,
                                                const Actions *a) {
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    if (b->type != BTYPE_BASE) continue;
    if (b->level != 1) continue;
    if (action_has_upgrade(a, b->region)) continue;
    if (!legal_upgrade_or_build_now(S, M, b->region)) continue;
    return 1;
  }
  return 0;
}

static int hq5_base2_savings_needed_before_train(const GameState *S,
                                                  const GameMap *M,
                                                  const Actions *a,
                                                  int budget) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (hq->level < HQ_MAX_LEVEL) return 0;
  if (!level1_base_upgrade_candidate_exists(S, M, a)) return 0;
  int spendable = budget;
  if (!g_hq5_repair_reserve_budget_excluded && !action_has_upgrade(a, M->my_hq))
    spendable -= FINAL_HQ5_REPAIR_RESERVE_GOLD;
  if (spendable < 0) spendable = 0;
  return spendable < BASE_LEVELS[2].cost;
}

static int issue_sustainable_hq5_surplus_train(Actions *a, const GameState *S,
                                               const GameMap *M, int *budget,
                                               int turn, int extra_reserve) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (hq->level < HQ_MAX_LEVEL) return 0;
  int cap = planned_train_cap(S, M, a);
  if (cap <= a->train_n) return 0;

  int remaining_days = MAX_TURN - turn + 1;
  if (remaining_days < 1) remaining_days = 1;
  int reserve = final_hq5_repair_reserve(S, M, a, turn);

  for (int total = cap; total > a->train_n; --total) {
    int extra = total - a->train_n;
    int after_gold = *budget - TRAIN_COST * extra;
    if (after_gold < extra_reserve) continue;

    int alive_after = count_side_warriors(S, M->my_side) + total;
    int income = conservative_expected_income(S, M, a, total);
    long long projected = (long long)after_gold +
                          (long long)income * remaining_days -
                          (long long)UPKEEP_PER_WARRIOR * alive_after * remaining_days;
    if (projected >= reserve) {
      a->train_n += extra;
      *budget -= TRAIN_COST * extra;
      return extra;
    }
  }
  return 0;
}

static int issue_hard_hq5_surplus_economy(Actions *a, const GameState *S,
                                          const GameMap *M, const Paths *P,
                                          int *budget, int turn) {
  int min_action_gold = TRAIN_COST;
  if (!hard_hq5_exclude_repair_reserve_for_surplus(S, M, a, budget, min_action_gold))
    return 0;

  int did = 0;

  /* User-requested priority under HQ5 surplus:
       1) fill existing worker slots (including HQ training if needed),
       2) upgrade owned BASE level 1 -> 2,
       3) opportunistically attack if the existing combat check passes,
       4) train only as much as upkeep can sustain.
     Do not spend surplus on generic/base-3 upgrades before the worker and
     base-2 priorities are exhausted. */
  did |= issue_underfilled_building_worker_support(a, S, M, P, budget, 0);

  int before_m = a->moves.len, before_u = a->upgrades.len, before_t = a->train_n;
  issue_base_level1_to_2_upgrades(a, S, M, budget);
  if (action_plan_changed_since(a, before_m, before_u, before_t)) did = 1;

  before_m = a->moves.len; before_u = a->upgrades.len; before_t = a->train_n;
  issue_priority_enemy_hq_attack(a, S, M, P, budget);
  issue_enemy_base_captures(a, S, M, P, budget);
  if (action_plan_changed_since(a, before_m, before_u, before_t)) did = 1;

  int train_reserve = level1_base_upgrade_candidate_exists(S, M, a) ? BASE_LEVELS[2].cost : 0;
  int trained = issue_sustainable_hq5_surplus_train(a, S, M, budget, turn, train_reserve);
  if (trained > 0) did = 1;

  return did;
}

static int issue_hard_advantage_conversion(Actions *a, const GameState *S,
                                           const GameMap *M, const Paths *P,
                                           int *budget, int turn) {
  if (!hard_advantage_phase_active(S, M, turn)) return 0;

  int did = 0;
  did |= issue_hard_build_occupied_neutral(a, S, M, budget);

  /* Step 1: enemy normal bases first.  Do not spend the conversion phase on HQ
     upgrades while an enemy base is still capturable from the current stage. */
  int before_m = a->moves.len, before_u = a->upgrades.len, before_t = a->train_n;
  int st = issue_hard_staged_attack(a, S, M, P, budget, turn);
  int st_issued_action = action_plan_changed_since(a, before_m, before_u, before_t);
  if (st > 0 && hard_side_base_count(S, opposite(M->my_side)) > 0)
    return st;

  /* Step 2: after bases are gone or no staged base attack is possible, prioritize
     HQ level/repair over producing extra population. */
  if (issue_hard_hq_priority_if_possible(a, S, M, P, budget, turn)) {
    /* If HQ5 is already secured after this action, the remaining surplus may be
       spent this same turn.  This avoids paying/holding the HQ reserve twice. */
    issue_hard_hq5_surplus_economy(a, S, M, P, budget, turn);
    update_previous_snapshot(S);
    return 1;
  }

  if (hard_should_save_for_hq_now(S, M, a, *budget, turn)) {
    int econ = issue_hard_hq5_surplus_economy(a, S, M, P, budget, turn);
    int tr = issue_hard_limited_train(a, S, M, budget);
    if (econ || tr) return econ + tr;
    return 1;
  }

  if (st > 0 && st_issued_action) return st;

  /* With the map already won, train only enough to stay ahead of remaining
     enemies; otherwise preserve cash for HQ/base upgrades. */
  did |= issue_hard_limited_train(a, S, M, budget);

  /* v13: the old hard-conversion path returned 1 even when it did nothing.
     Because it runs before the normal economy pipeline, that made turns after
     HQ5 look like a permanent save-lock.  Once HQ5 exists, keep the 1000-gold
     repair reserve and spend surplus on workers/upgrades/training. */
  did |= issue_hard_hq5_surplus_economy(a, S, M, P, budget, turn);

  if (!did) {
    if (enemy_normal_bases_cleared_now(S, M)) {
      /* Enemy bases are gone and there is no spendable HQ5 surplus: preserve
         tiebreak money, but report no-op only after the surplus check above. */
    } else if (!affordable_existing_upgrade_exists(S, M, a, *budget)) {
      /* no-op: save gold */
    } else {
      issue_existing_upgrades(a, S, M, budget);
      did = 1;
    }
  }

  return did ? did : 1;
}


static int is_endgame_turn(int turn) {
  return turn >= MAX_TURN - ENDGAME_ATTACK_TURNS + 1;
}

static int empty_owned_building_after_plan(const GameState *S, const GameMap *M,
                                           const Paths *P, const Actions *a,
                                           int region) {
  const Building *b = find_building_const(S, region);
  if (b == NULL || b->side != M->my_side) return 0;
  if (move_target_has_enemy_projected(S, M, P, region)) return 0;
  return planned_workers_committed_to_region(S, a, M->my_side, region) <= 0;
}

static int choose_refill_worker_for_empty_building(const GameState *S,
                                                   const GameMap *M,
                                                   const Paths *P,
                                                   const Actions *a,
                                                   int target,
                                                   WarriorId *out) {
  double best_score = INFINITY;
  WarriorId best_id = {M->my_side, -1};

  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (action_has_move_warrior(a, w->id)) continue;
    if (w->region == target) continue;
    if (region_has_enemy_warrior(S, M, w->region)) continue;
    if (is_stronghold(M, w->region) && find_building_const(S, w->region) == NULL)
      continue;
    if (P->nxt[w->region][target] == -1) continue;
    if (full_path_enemy_blocked(S, M, P, w->region, target, 0)) continue;

    const Building *src_b = find_building_const(S, w->region);
    int source_priority = 2;
    if (src_b != NULL && src_b->side == M->my_side) {
      int remaining = planned_workers_physically_remaining_at(S, a, M->my_side, w->region);
      if (remaining <= 1) continue;
      int cap = planned_work_cap_at(S, M, a, w->region);
      source_priority = remaining > cap ? 0 : 1;
    } else {
      source_priority = 1;
    }

    double score = 1000000.0 * source_priority + P->dist[w->region][target] +
                   0.001 * w->id.num;
    if (score < best_score) {
      best_score = score;
      best_id = w->id;
    }
  }

  if (best_id.num < 0) return 0;
  *out = best_id;
  return 1;
}

static int issue_empty_owned_building_refill(Actions *a, const GameState *S,
                                             const GameMap *M, const Paths *P,
                                             int *budget) {
  int did = 0;
  int still_empty = 0;

  for (int pass = 0; pass < S->buildings.len; ++pass) {
    int best_target = -1;
    double best_score = INFINITY;
    for (int bi = 0; bi < S->buildings.len; ++bi) {
      const Building *b = &S->buildings.data[bi];
      if (b->side != M->my_side) continue;
      if (!empty_owned_building_after_plan(S, M, P, a, b->region)) continue;
      double score = P->dist[M->my_hq][b->region];
      if (score < best_score) {
        best_score = score;
        best_target = b->region;
      }
    }
    if (best_target < 0) break;

    WarriorId id;
    if (!choose_refill_worker_for_empty_building(S, M, P, a, best_target, &id)) {
      still_empty = 1;
      break;
    }
    if (!add_move_action_ex_stack_flags(a, S, M, P, id, best_target, budget,
                                        MOVE_FLAG_IGNORE_STACK_GUARD)) {
      still_empty = 1;
      break;
    }
    did = 1;
  }

  if (still_empty && a->train_n == 0 && *budget >= TRAIN_COST) {
    int cap = planned_train_cap(S, M, a);
    if (cap > 0) {
      a->train_n = 1;
      *budget -= TRAIN_COST;
      did = 1;
    }
  }
  return did;
}

typedef struct {
  int region;
  int cap;
  int stationary;
  int keep;
} KeepSlot;

static int cmp_keep_slot(const void *pa, const void *pb) {
  const KeepSlot *a = (const KeepSlot *)pa;
  const KeepSlot *b = (const KeepSlot *)pb;
  /* Prefer keeping workers where more workers can actually produce income. */
  if (a->cap != b->cap) return b->cap - a->cap;
  return a->region - b->region;
}

static int collect_keep_slots(const GameState *S, const GameMap *M,
                              KeepSlot **out) {
  int cnt = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side == M->my_side) ++cnt;
  }
  if (cnt == 0) {
    *out = NULL;
    return 0;
  }

  KeepSlot *slots = (KeepSlot *)calloc((size_t)cnt, sizeof(KeepSlot));
  int idx = 0;
  for (int bi = 0; bi < S->buildings.len; ++bi) {
    const Building *b = &S->buildings.data[bi];
    if (b->side != M->my_side) continue;
    slots[idx].region = b->region;
    slots[idx].cap = building_work_cap(b);
    slots[idx].stationary = count_stationary_warriors_at(S, M->my_side, b->region);
    slots[idx].keep = 0;
    ++idx;
  }
  qsort(slots, (size_t)cnt, sizeof(KeepSlot), cmp_keep_slot);
  *out = slots;
  return cnt;
}

static int find_keep_slot_index(const KeepSlot *slots, int len, int region) {
  for (int i = 0; i < len; ++i)
    if (slots[i].region == region) return i;
  return -1;
}

static int assign_endgame_keep_workers(const GameState *S, const GameMap *M,
                                       KeepSlot *slots, int slot_len) {
  int alive = count_side_warriors(S, M->my_side);
  int needed = (UPKEEP_PER_WARRIOR * alive + WORK_INCOME - 1) / WORK_INCOME;
  needed += DEFENSE_KEEP_EXTRA_WORKERS;

  int total_possible = 0;
  for (int i = 0; i < slot_len; ++i)
    total_possible += min_int(slots[i].cap, slots[i].stationary);
  needed = min_int(needed, total_possible);

  int kept = 0;
  while (kept < needed) {
    int progressed = 0;
    for (int i = 0; i < slot_len && kept < needed; ++i) {
      if (slots[i].keep >= slots[i].cap) continue;
      if (slots[i].keep >= slots[i].stationary) continue;
      ++slots[i].keep;
      ++kept;
      progressed = 1;
    }
    if (!progressed) break;
  }
  return kept;
}

static int endgame_keep_income(const KeepSlot *slots, int slot_len) {
  int kept = 0;
  for (int i = 0; i < slot_len; ++i)
    kept += slots[i].keep;
  return WORK_INCOME * kept;
}

static int issue_endgame_hq_attack(Actions *a, const GameState *S,
                                   const GameMap *M, const Paths *P,
                                   int *budget, int turn) {
  KeepSlot *slots = NULL;
  int slot_len = collect_keep_slots(S, M, &slots);
  assign_endgame_keep_workers(S, M, slots, slot_len);

  int alive = count_side_warriors(S, M->my_side);
  int income = endgame_keep_income(slots, slot_len);
  int upkeep = UPKEEP_PER_WARRIOR * alive;
  int max_cost_preserving_upkeep = S->gold + income - upkeep - ENDGAME_GOLD_SAFETY;
  if (max_cost_preserving_upkeep < 0) max_cost_preserving_upkeep = 0;

  int attack_budget = min_int(*budget, max_cost_preserving_upkeep);
  int initial_attack_budget = attack_budget;
  int sent = 0;
  int sync_eta = -1;

  if (turn >= ENDGAME_SYNC_START_TURN && turn <= ENDGAME_SYNC_ARRIVAL_TURN)
    sync_eta = ENDGAME_SYNC_ARRIVAL_TURN - turn + 1;

  for (int i = 0; i < S->warriors.len; ++i) {
    const Warrior *w = &S->warriors.data[i];
    if (w->id.side != M->my_side) continue;
    if (w->state != WSTATE_STATIONARY) continue;
    if (w->region == M->opp_hq) continue;
    if (region_has_enemy_warrior(S, M, w->region)) continue;

    int h = path_hops_between(P, w->region, M->opp_hq);
    if (h >= INF_HOPS) continue;
    if (!attack_path_first_enemy_is_target(S, M, P, w->region, M->opp_hq)) continue;

    /* Turns 190..195 are synchronized so that issued warriors arrive on the
       enemy HQ in turn 195.  After 195, send all remaining available warriors
       immediately because synchronization is no longer useful. */
    if (sync_eta >= 0 && h != sync_eta) continue;

    int keep_idx = find_keep_slot_index(slots, slot_len, w->region);
    if (keep_idx >= 0 && slots[keep_idx].keep > 0) {
      --slots[keep_idx].keep;
      continue;
    }

    if (attack_budget < MOVE_COST) break;
    if (add_move_action(a, S, M, w->id, M->opp_hq, &attack_budget))
      ++sent;
  }

  *budget -= (initial_attack_budget - attack_budget);
  free(slots);
  return sent;
}


static int hq5_save_lock_can_release_for_economy(const GameState *S,
                                                 const GameMap *M,
                                                 const Actions *a,
                                                 int budget) {
  const Building *hq = find_building_const(S, M->my_hq);
  if (hq == NULL || hq->side != M->my_side || hq->type != BTYPE_HQ) return 0;
  if (hq->level < HQ_MAX_LEVEL) return 0;
  if (action_has_upgrade(a, M->my_hq)) return 0;

  int reserve = FINAL_HQ5_REPAIR_RESERVE_GOLD;
  if (budget < reserve + TRAIN_COST) return 0;
  return 1;
}

static int hq5_apply_repair_reserve_if_releasing_save_lock(const GameState *S,
                                                           const GameMap *M,
                                                           const Actions *a,
                                                           int *budget) {
  if (!hq5_save_lock_can_release_for_economy(S, M, a, *budget)) return 0;
  *budget -= FINAL_HQ5_REPAIR_RESERVE_GOLD;
  g_hq5_repair_reserve_budget_excluded = 1;
  g_allow_base_upgrades_after_enemy_baseclear = 1;
  return 1;
}

static Actions decide(const GameState *S, const GameMap *M, const Paths *P,
                      int turn) {
  Actions a;
  memset(&a, 0, sizeof(a));
  strategy_prev_init();

  g_stack_guard_paths = P;
  g_current_turn = turn;
  g_hq5_repair_reserve_budget_excluded = 0;
  g_allow_base_upgrades_after_enemy_baseclear = 0;

  int budget = S->gold;
  DefensePlan defense_plan;
  memset(&defense_plan, 0, sizeof(defense_plan));

  if (issue_direct_hq_attack_guard(&a, S, M, P, &budget) > 0) {
    sanitize_anchor_route_offensive_moves(&a, S, M);
    update_previous_snapshot(S);
    return a;
  }

  /* Owned-base crisis defense is below direct-HQ defense but above ordinary worker
     refill.  It pulls only from non-crisis owned bases, and its simulator adds
     each reinforcement from the day it actually reaches the threatened base. */
  issue_owned_base_emergency_defense(&a, S, M, P, &budget);

  /* If the opening policy trained a worker for a neutral stronghold on the
     previous turn, do not let it spend a turn at HQ.  This is still below real
     HQ/base crisis defense, but above ordinary zero-worker refill. */
  issue_immediate_hq_neutral_dispatch(&a, S, M, P, &budget, turn);

  /* A zero-worker owned building is an immediate income leak, but it should not
     steal the just-in-time neutral worker that must leave HQ now. */
  issue_empty_owned_building_refill(&a, S, M, P, &budget);

  /* Retreat from non-owned regions whose scheduled combat is a strict loss. */
  issue_losing_fight_retreats(&a, S, M, P, &budget);

  /* Hardcoded conversion for won-but-drawn games: if we already own the map,
     stop overtraining, prioritize HQ/base levels, and attack only as one staged
     stack from a single middle base. */
  if (issue_hard_advantage_conversion(&a, S, M, P, &budget, turn) > 0) {
    sanitize_anchor_route_offensive_moves(&a, S, M);
    update_previous_snapshot(S);
    return a;
  }

  /* Last turns are still allowed to all-in, but not while an actual inbound
     home attack is already detected.  Otherwise the same over-commit failure
     appears: we race the enemy HQ while our own HQ is being sieged. */
  if (is_endgame_turn(turn)) {
    DefensePlan end_def = compute_defense_plan(S, M, P, &a, budget);
    if (end_def.triggered) {
      /* Do not let defense mode waste a turn-limit tiebreak.  In the previous
         version, a repeated inbound warning from turn ~190 made this branch
         return before issuing an affordable HQ upgrade/repair, leaving large
         amounts of gold unused and losing by HQ HP.  Construction happens
         before movement/training, so perform any legal HQ tiebreak action now
         and still recall/train for defense with the remaining budget. */
      ensure_late_hq_tiebreak_worker(&a, S, M, P, &budget, turn);
      issue_late_hq_tiebreak_if_affordable(&a, S, M, &budget, turn);
      issue_home_defense(&a, S, M, P, &budget, &end_def);
      issue_final_hq5_cash_dump_train(&a, S, M, &budget, turn);
    } else {
      /* v12: before spending endgame movement gold, try to secure HQ5 or a
         max-level HQ repair for turn-limit tiebreaks.  If the action is not
         affordable yet but can be reached before the end, reserve that gold and
         only spend the remainder on the synchronized HQ attack. */
      ensure_late_hq_tiebreak_worker(&a, S, M, P, &budget, turn);
      issue_late_hq_tiebreak_if_affordable(&a, S, M, &budget, turn);
      int reserve = late_hq_tiebreak_reserved_gold(S, M, &a, budget, turn);
      (void)reserve;
      /* Do not scatter individual warriors at the enemy HQ in the final turns.
         The hardcoded conversion code either levels/repairs HQ or gathers a
         single staged stack and launches that stack together. */
      issue_hard_advantage_conversion(&a, S, M, P, &budget, turn);
      if (!hard_advantage_phase_active(S, M, turn))
        issue_final_hq5_cash_dump_train(&a, S, M, &budget, turn);
    }
    sanitize_anchor_route_offensive_moves(&a, S, M);
    update_previous_snapshot(S);
    return a;
  }

  /* 0. ETA-based home defense.  Only enemies that are moving closer to our HQ
     or are already very close count as inbound threats.  Recall+training is
     reserved before any economy/capture action consumes budget or units. */
  int defense_issued = issue_home_defense(&a, S, M, P, &budget, &defense_plan);
  int hard_defense = defense_plan.triggered && defense_plan.hard;


  /* Army HP catch-up: if our total warrior HP is too low compared with the
     opponent, buy population before any non-defense gold spending. */
  if (!defense_plan.triggered)
    issue_hp_ratio_train_priority(&a, S, M, &budget, turn);

  /* Stack-only patch: when an enemy mass is projected onto an HQ<->base supply
     path, send a synchronized superior cleanup wave before ordinary expansion
     or staging can feed single workers into it. */
  if (!defense_plan.triggered) {
    int cleaned = issue_stack_cleanup(&a, S, M, P, &budget);
    if (cleaned > 0) {
      if (a.train_n == 0)
        a.train_n = choose_train_count(S, M, &a, budget, 0, turn);
      update_previous_snapshot(S);
      return a;
    }
  }

  /* v29 thin opening grab: until empty neutral spots are exhausted or the
     opponent can plausibly rush, run a dedicated opening that keeps exactly
     one body on each owned HQ/base, sends surplus bodies to neutral spots even
     without construction gold, and forbids HQ/base level-ups.  Return even if
     it issues no command, so the normal upgrade/capture policy cannot break
     the intended opening discipline on a temporarily resource-bound turn. */
  if (!defense_plan.triggered &&
      thin_opening_should_continue(S, M, P, &a, turn)) {
    issue_thin_opening_grab_phase(&a, S, M, P, &budget, turn);
    sanitize_anchor_route_offensive_moves(&a, S, M);
    update_previous_snapshot(S);
    return a;
  }

  /* Opening-only neutral slice: before normal v12 priorities, spend only a
     small early phase taking nearby empty strongholds from our HQ outward.
     The phase ends permanently after its quota or max turn, so early attacks
     and the original v12 economy can resume instead of waiting for every
     neutral stronghold on the map. */
  if (!defense_plan.triggered &&
      issue_opening_neutral_first_phase(&a, S, M, P, &budget, turn)) {
    if (a.train_n == 0)
      a.train_n = choose_train_count(S, M, &a, budget, 0, turn);
    sanitize_anchor_route_offensive_moves(&a, S, M);
    update_previous_snapshot(S);
    return a;
  }

  /* If the opening-neutral quota is still unfinished, do not start enemy HQ/base
     attacks just because the opening routine could not issue a command this
     turn (for example no surplus unit, no movement budget, or a one-turn
     inbound-pressure pause).  The intended priority is: defend if needed,
     then finish the nearby empty-stronghold opening slice, and only then attack. */
  int opening_neutral_blocks_attack =
      opening_neutral_unfinished(S, M, &a, turn) && !defense_plan.triggered;

  /* If the opening neutral quota is still unfinished, do not let early HQ2/HQ3
     timing steal the gold and workers needed to grab nearby empty strongholds.
     This is the main reason player B lagged behind: the HQ kept an extra guard
     and then spent the first surplus gold on HQ upgrade before enough neutrals
     were claimed. */
  int opening_neutral_priority_lock =
      opening_neutral_blocks_attack &&
      turn <= OPENING_DELAY_HQ_UPGRADE_UNTIL_TURN &&
      opening_neutral_handled_count(S, M, &a) < OPENING_DELAY_HQ_UPGRADE_MIN_HANDLED;

#if OPENING_FORCE_TRAIN_IF_STUCK
  if (opening_neutral_priority_lock && a.train_n == 0 && budget >= TRAIN_COST) {
    int tmp_target = -1;
    WarriorId tmp_id = {M->my_side, -1};
    if (!choose_opening_neutral_closest(S, M, P, &a, &tmp_target, &tmp_id)) {
      int n = choose_train_count(S, M, &a, budget, 0, turn);
      if (n <= 0) n = min_int(planned_train_cap(S, M, &a), budget / TRAIN_COST);
      if (n > 0) {
        a.train_n = n;
        budget -= TRAIN_COST * n;
      }
    }
  }
#endif

  /* v8/v12: HQ upgrades are so important that an overdue HQ upgrade gets
     handled before any ordinary economy or attack movement.  Exception: during
     the early neutral-first quota, delay HQ upgrades/save-lock so expansion can
     catch up to opponents that claim two nearby strongholds immediately. */
  int did_late_tiebreak = 0;
  int did_hq_priority_upgrade = 0;
  int hq_save_lock = 0;
  if (!opening_neutral_priority_lock) {
    ensure_hq_upgrade_worker(&a, S, M, P, &budget);
    ensure_late_hq_tiebreak_worker(&a, S, M, P, &budget, turn);
    did_late_tiebreak = issue_late_hq_tiebreak_if_affordable(&a, S, M, &budget, turn);
    did_hq_priority_upgrade = did_late_tiebreak ||
        issue_hq_upgrade_if_affordable(&a, S, M, &budget);
    hq_save_lock = should_save_for_hq_upgrade(S, M, &a, budget) ||
        late_hq_tiebreak_should_save(S, M, &a, budget, turn);

    /* If HQ is already level 5, the remaining save-lock is usually just the
       1000-gold repair/tiebreak reserve.  Do not let that freeze all economy:
       keep exactly that reserve out of the planner budget, and allow BASE
       upgrades/training with the surplus above it. */
    if (hq_save_lock && hq5_apply_repair_reserve_if_releasing_save_lock(S, M, &a, &budget))
      hq_save_lock = 0;
  }

  /* In hard defense, do not start new expansion/capture/staging moves this
     turn.  HQ upgrade/worker recall above is still allowed, but extra movement
     is what caused the observed scattering. */
  if (hard_defense && HARD_DEFENSE_SKIPS_ECONOMY_MOVES) {
    sanitize_anchor_route_offensive_moves(&a, S, M);
    update_previous_snapshot(S);
    return a;
  }

  /* Anchor-route post-capture cleanup.  If the previous anchor attack emptied
     a stronghold, build it first; after it becomes our BASE, leave one warrior
     there and return the rest to the sticky anchor. */
  if (!defense_plan.triggered && issue_anchor_route_capture_cleanup(&a, S, M, P, &budget)) {
    sanitize_anchor_route_offensive_moves(&a, S, M);
    update_previous_snapshot(S);
    return a;
  }

  /* Fill existing work slots before neutral expansion, attacks, or ordinary
     base upgrades can consume the same surplus workers/gold.  Training remains
     below the HQ save-lock, but when the lock is off this has priority over
     saving for BASE upgrades. */
  issue_underfilled_building_worker_support(&a, S, M, P, &budget, hq_save_lock);

  /* 1. Winning attack: launch only after defense and HQ reservation.
     Material-advantage HQ pressure is disabled; HQ attacks require the
     simulation to pass against expected HQ training reinforcements. */
  int allgame_neutral_blocks_attack =
      !defense_plan.triggered && unclaimed_empty_neutral_exists(S, M, &a);

  const Building *my_hq_for_attack = find_building_const(S, M->my_hq);
  if (!defense_plan.triggered && !hq_save_lock && my_hq_for_attack != NULL &&
      my_hq_for_attack->side == M->my_side && my_hq_for_attack->level >= HQ_MAX_LEVEL) {
    /* At HQ5 with surplus economy, do not play pure tiebreak only.  If the
       existing HQ combat simulation says the enemy HQ can be taken even after
       its training response, launch it before neutral leftovers block attacks. */
    if (issue_priority_enemy_hq_attack(&a, S, M, P, &budget)) {
      issue_hq5_excess_train(&a, S, M, &budget, turn);
      sanitize_anchor_route_offensive_moves(&a, S, M);
      update_previous_snapshot(S);
      return a;
    }
  }

  if (!defense_plan.triggered && !hq_save_lock) {
    issue_empty_neutral_claims_one_by_one(&a, S, M, P, &budget);
    allgame_neutral_blocks_attack =
        !defense_plan.triggered && unclaimed_empty_neutral_exists(S, M, &a);
  }

  if (!defense_plan.triggered && !hq_save_lock &&
      !opening_neutral_blocks_attack && !allgame_neutral_blocks_attack) {
    if (issue_rally_stack_launch(&a, S, M, P, &budget, turn)) {
      sanitize_anchor_route_offensive_moves(&a, S, M);
      update_previous_snapshot(S);
      return a;
    }
  }

  /* 3. Upgrade existing owned bases/HQ as much as possible this turn, unless
     we are saving for an overdue HQ upgrade. */
  int upgrade_was_possible_initially =
      affordable_existing_upgrade_exists(S, M, &a, budget) || did_hq_priority_upgrade;
  int did_existing_upgrade = did_hq_priority_upgrade;
  if (!hq_save_lock) {
    did_existing_upgrade |= issue_existing_upgrades(&a, S, M, &budget);
  }

  /* 4. Expand/capture only if no HQ-saving lock is active.  During soft defense
     we still allow local builds/upgrades, but skip enemy captures and neutral
     expansion moves because they reduce future defense flexibility. */
  if (!hq_save_lock && !affordable_existing_upgrade_exists(S, M, &a, budget)) {
    int built = issue_neutral_builds(&a, S, M, &budget);
    (void)built;

    if (!defense_issued) {
      if (!opening_neutral_blocks_attack && !allgame_neutral_blocks_attack) {
        issue_priority_enemy_hq_attack(&a, S, M, P, &budget);
        issue_enemy_base_captures(&a, S, M, P, &budget);
        issue_enemy_occupied_neutral_captures(&a, S, M, P, &budget);
        issue_stronghold_claims_by_priority(&a, S, M, P, &budget);
      } else {
        issue_empty_neutral_claims_one_by_one(&a, S, M, P, &budget);
      }
    }
  }

  /* 5. Stage leftover surplus forward only when no home-defense warning and no
     HQ-saving lock was triggered.  v8 batches staging more aggressively by
     allowing multiple units per source. */
  if (!defense_issued && !hq_save_lock &&
      !opening_neutral_blocks_attack && !allgame_neutral_blocks_attack) {
    if (!issue_rally_stack_launch(&a, S, M, P, &budget, turn))
      issue_rally_stack_staging(&a, S, M, P, &budget, turn);
#if !ENABLE_RALLY_STACK_ATTACK
    issue_forward_staging_moves(&a, S, M, P, &budget);
#endif
  }

  /* 6. Train population.  Defense training, if issued, is never overwritten.
     If we are saving for an overdue HQ upgrade, train only when it is still
     consistent with the upkeep check and does not spend the exact HQ budget. */
  int upgrade_mode = upgrade_was_possible_initially || did_existing_upgrade || hq_save_lock;
  if (a.train_n == 0) {
    issue_hq5_excess_train(&a, S, M, &budget, turn);
  }
  if (a.train_n == 0) {
    issue_final_hq5_cash_dump_train(&a, S, M, &budget, turn);
  }
  if (a.train_n == 0) {
    a.train_n = choose_train_count(S, M, &a, budget, upgrade_mode, turn);
  }

  sanitize_anchor_route_offensive_moves(&a, S, M);
  update_previous_snapshot(S);
  return a;
}

int main(void) {
  GameMap M;
  GameState S;
  parse_init(&M, &S);              /* initialize the game */
  Paths P = calculate_paths(&M);   /* calculate the shortest paths */

  int turn;
  while (read_turn_start(&turn)) {
    Actions a = decide(&S, &M, &P, turn);
    emit_actions(&a);
    read_turn_result(&S, &M, &a);
    free(a.moves.data);
    free(a.upgrades.data);
  }
  return 0;
}
