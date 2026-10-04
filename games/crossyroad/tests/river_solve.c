#include <eadk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/crossy.c"
#undef main

uint32_t eadk_random(void) {
  static uint32_t s = 2463534242u;
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

#define MAXROWS 400
#define DT 0.04f
#define HSTEPS 4
#define NT 1200
#define WROWS 8
#define KEYS 40

static Lane world[MAXROWS];
static uint8_t reach[NT + HSTEPS + 2][WROWS][KEYS];

static int is_water(const Lane *L) { return L->type == L_RIVER || L->type == L_LILY; }

static int free_cell(const Lane *L, int col) {
  if (L->type == L_GRASS) return L->cells[COL0 + col] == 0 || L->cells[COL0 + col] == O_PAD;
  return 1;
}

static uint16_t free_mask(const Lane *L) {
  uint16_t m = 0;
  for (int i = 0; i < 9; i++)
    if (free_cell(L, PLAY_MIN + i)) m |= 1 << i;
  return m;
}

static uint16_t spread_(uint16_t seed, uint16_t fr) {
  uint16_t m = seed & fr, old;
  do {
    old = m;
    m |= ((m << 1) | (m >> 1)) & fr;
  } while (m != old);
  return m;
}

static float log_cx(const Lane *L, int i, float t) {
  float c = L->obj[i].x + L->dir * L->speed * t;
  return c - L->period * floorf(c / L->period + 0.5f);
}

static int find_log_t(const Lane *L, float x, float t, float *slot, int *idx_out) {
  for (int i = 0; i < L->nobj; i++) {
    float cx = L->obj[i].x + L->dir * L->speed * t, half = L->obj[i].kind * 0.5f;
    float rel = x - cx;
    rel -= L->period * floorf(rel / L->period + 0.5f);
    if (rel < -half - 0.45f || rel > half + 0.45f) continue;
    int idx = (int)floorf(rel + half);
    if (idx < 0) idx = 0;
    if (idx >= L->obj[i].kind) idx = L->obj[i].kind - 1;
    *slot = x - rel - half + 0.5f + idx;
    *idx_out = idx;
    return i;
  }
  return -1;
}

static float state_x(const Lane *L, int key, float t) {
  if (L->type == L_RIVER) {
    int i = key >> 2, k = key & 3;
    return log_cx(L, i, t) - L->obj[i].kind * 0.5f + 0.5f + k;
  }
  return (float)(PLAY_MIN + key);
}

static int alive(const Lane *L, int key, float t) {
  if (L->type != L_RIVER) return 1;
  float x = state_x(L, key, t);
  if (x < PLAY_MIN - 0.5f && L->dir < 0) return 0;
  if (x > PLAY_MAX + 0.5f && L->dir > 0) return 0;
  return 1;
}

static uint16_t solve_run(const Lane *run, int len, uint16_t bank_reach, int *tmin) {
  memset(reach, 0, sizeof(reach));
  uint16_t exit_mask = 0;
  *tmin = -1;
  for (int i = 0; i < 9; i++)
    if (bank_reach >> i & 1) reach[0][0][i] = 1;
  static const int HX[4] = {0, -1, 1, 0}, HZ[4] = {1, 0, 0, -1};
  for (int s = 0; s < NT; s++) {
    float t = s * DT;
    for (int r = 0; r <= len; r++) {
      const Lane *L = &run[r];
      for (int key = 0; key < KEYS; key++) {
        if (!reach[s][r][key]) continue;
        if (r == 0 || alive(L, key, t + DT)) reach[s + 1][r][key] = 1;
        float x = r == 0 ? (float)(PLAY_MIN + key) : state_x(L, key, t);
        for (int d = 0; d < 4; d++) {
          int tr = r + HZ[d];
          if (tr < 0 || tr > len + 1) continue;
          if (r == 0 && d == 3) continue;
          int tcol = iround(x + HX[d]);
          if (tcol < PLAY_MIN || tcol > PLAY_MAX) continue;
          const Lane *T = &run[tr];
          int ns = s + HSTEPS;
          if (tr == 0 || tr == len + 1) {
            if (!free_cell(T, tcol)) continue;
            if (tr == len + 1) {
              exit_mask |= 1 << (tcol - PLAY_MIN);
              if (*tmin < 0) *tmin = ns;
            } else {
              reach[ns][0][tcol - PLAY_MIN] = 1;
            }
          } else if (T->type == L_RIVER) {
            float slot;
            int idx;
            int i = find_log_t(T, x + HX[d], t + HSTEPS * DT, &slot, &idx);
            if (i < 0) continue;
            int i2 = find_log_t(T, slot, t + HSTEPS * DT, &slot, &idx);
            if (i2 < 0) continue;
            reach[ns][tr][i2 * 4 + idx] = 1;
          } else {
            if (T->cells[COL0 + tcol] != O_PAD) continue;
            reach[ns][tr][tcol - PLAY_MIN] = 1;
          }
        }
      }
    }
  }
  return exit_mask;
}

static void describe(const Lane *run, int len) {
  for (int r = 0; r <= len + 1; r++) {
    const Lane *L = &run[r];
    if (L->type == L_RIVER) {
      printf("  row %d river dir %d speed %.2f period %.1f n %d lens", L->row, L->dir, L->speed, L->period, L->nobj);
      for (int i = 0; i < L->nobj; i++) printf(" %d", L->obj[i].kind);
      printf("\n");
    } else if (L->type == L_LILY) {
      printf("  row %d lily pads", L->row);
      for (int c = PLAY_MIN; c <= PLAY_MAX; c++) printf("%c", L->cells[COL0 + c] == O_PAD ? 'o' : '.');
      printf("\n");
    } else {
      printf("  row %d type %d free", L->row, L->type);
      for (int c = PLAY_MIN; c <= PLAY_MAX; c++) printf("%c", free_cell(L, c) ? '.' : '#');
      printf("\n");
    }
  }
}

int main(int argc, char **argv) {
  memcpy(&G.rom, &rom_init, sizeof(rom_init));
  int worlds = argc > 1 ? atoi(argv[1]) : 300, shown = 0;
  int runs = 0, bad = 0, lily_runs = 0, bad_lily = 0, mixed = 0, bad_mixed = 0, worlds_bad = 0;
  long slow = 0;
  for (int w = 0; w < worlds; w++) {
    rng_s = (uint32_t)(w * 2654435761u + 12345u) | 1;
    memset(lanes, 0, sizeof(lanes));
    gen_next = -10;
    set_left = 0;
    set_type = L_GRASS;
    prev_dir = 0;
    start_end = 1 + irand(2);
    reach_prev = ALLPLAY;
    int rows = 260;
    for (int i = 0; i < rows + 10; i++) {
      int row = gen_next;
      gen_lane();
      world[row + 10] = *lane_at(row);
    }
    uint16_t R = spread_(1 << 4, free_mask(&world[10]));
    int broke = 0;
    for (int r = 11; r < rows && !broke; r++) {
      const Lane *L = &world[r];
      if (!is_water(L)) {
        uint16_t fr = free_mask(L);
        R = spread_(R & fr, fr);
        if (!R) {
          printf("world %d: dead end at row %d\n", w, L->row);
          broke = 1;
        }
        continue;
      }
      int a = r, b = r;
      while (b + 1 < rows && is_water(&world[b + 1])) b++;
      int len = b - a + 1, hasl = 0, hasr = 0;
      for (int k = a; k <= b; k++) hasl |= world[k].type == L_LILY, hasr |= world[k].type == L_RIVER;
      int tmin;
      uint16_t ex = solve_run(&world[a - 1], len, R, &tmin);
      runs++;
      lily_runs += hasl;
      mixed += hasl && hasr;
      if (tmin > 40) slow++;
      if (!ex) {
        bad++;
        bad_lily += hasl;
        bad_mixed += hasl && hasr;
        if (shown < 6) {
          printf("world %d: impossible stretch, entry columns", w);
          for (int i = 0; i < 9; i++) printf("%c", R >> i & 1 ? 'x' : '.');
          printf("\n");
          describe(&world[a - 1], len);
          shown++;
        }
        broke = 1;
        break;
      }
      uint16_t fr = free_mask(&world[b + 1]);
      R = spread_(ex, fr);
      r = b;
      if (!R) broke = 1;
    }
    worlds_bad += broke;
  }
  printf("worlds %d, water stretches %d (with pads %d, mixed %d)\n", worlds, runs, lily_runs, mixed);
  printf("impossible stretches %d (with pads %d, mixed %d); worlds hitting one %d; slow crossings %ld\n", bad, bad_lily,
         bad_mixed, worlds_bad, slow);
  return 0;
}
