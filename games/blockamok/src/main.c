#include <eadk.h>
#include <stdbool.h>
#include <stdint.h>
#include "../../common/epsilon_app.h"
#include "../../common/epsilon_files.h"
#include "font.h"

#ifdef __ELF__
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Blockamok";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#endif

typedef uint16_t px;

#define SW 320
#define SH 240
#define BH 30
#define NB (SH / BH)
#define CMAX 1620
#define IMAX 800
#define DEPTH 150.0f
#define PROJ 579.4113f
#define MAXSPEED 1500.0f
#define SPEEDUP 2.2f
#define HISCORE0 1000
#define BASE_BOUNDS 12.0f
#define SAVE_NAME "blockamok.sav"

enum { O_FREQ, O_SIZE, O_LIVES, O_SPAWN, O_BG, O_CUBE, O_OVL, O_TRANS, O_SPEED, NOPT };
enum { S_START, S_TITLE, S_MAIN, S_GAME, S_VISUAL, S_INSTR, S_CREDITS, S_RESET, S_QUIT, S_PLAY, S_PAUSE, S_OVER };
enum { K_UP = 1, K_DOWN = 2, K_LEFT = 4, K_RIGHT = 8, K_OK = 16, K_BACK = 32, K_EXE = 64, K_DEL = 128 };

typedef struct {
  float x, y, z;
} Cube;

typedef struct {
  int16_t x[8], y[8];
  int16_t y0, y1;
  uint8_t n, f[3], a[3], l[3];
} Item;

typedef struct {
  const char *name;
  const char *const *ch;
  uint8_t opt, n;
  const char *d0, *d1, *d2;
} Line;

typedef struct {
  uint8_t magic, version, opt[NOPT], pad;
  uint32_t hi;
} Save;

static const char *const N_FREQ[] = {"Low", "Medium", "High", "Very High", "Intense"};
static const char *const N_SIZE[] = {"Normal", "Large", "Very Large", "Giant"};
static const char *const N_LIVES[] = {"1", "2", "3"};
static const char *const N_SPAWN[] = {"Lowest", "Lower", "Low", "Default", "High", "Higher", "Highest"};
static const char *const N_BG[] = {"Lava Red", "Harvest Orange", "Golden Brass", "Electric Green",
                                   "Space Blue", "Ocean Blue", "Royal Violet", "Void Black"};
static const char *const N_CUBE[] = {"Lightning", "Plant", "Charcoal", "Snow", "Fire", "Stone"};
static const char *const N_OVL[] = {"Lava Red", "Harvest Orange", "Golden Brass", "Electric Green", "Space Blue",
                                    "Ocean Blue", "Royal Violet", "Void Black", "Pitch Black", "Off", "Match BG"};
static const char *const N_ONOFF[] = {"On", "Off"};
static const char *const N_OFFON[] = {"Off", "On"};

static const Line GAME_LINES[] = {
  {"Block Frequency", N_FREQ, O_FREQ, 5, "Change the number of obstacles.", 0, 0},
  {"Block Size", N_SIZE, O_SIZE, 4, "Change the size of the", "incoming obstacles.", 0},
  {"Lives", N_LIVES, O_LIVES, 3, "Change how many hits you can take.", 0, 0},
  {"Spawn Area", N_SPAWN, O_SPAWN, 7, "[Advanced] Change spawn area of blocks.", "Default is preferred, but a small area",
   "may improve frame rate on weak devices."},
};
static const Line VIS_LINES[] = {
  {"BG Color", N_BG, O_BG, 8, "Change the background color.", 0, 0},
  {"Block Color", N_CUBE, O_CUBE, 6, "Change the color of obstacles.", 0, 0},
  {"Overlay", N_OVL, O_OVL, 11, "Enable the overlay and set its color.", 0, 0},
  {"Transparency", N_ONOFF, O_TRANS, 2, "Toggle the fade-in transparency", "effect on distant blocks.", 0},
  {"Speedometer", N_OFFON, O_SPEED, 2, "Show your speed in the", "bottom-right corner of the screen.", 0},
};
static const char *const MAIN_NAMES[] = {"Game Options", "Visuals", "Instructions", "Credits",
                                         "Reset High Score", "Reset Settings", "Quit"};
static const int8_t MAIN_GOTO[] = {S_GAME, S_VISUAL, S_INSTR, S_CREDITS, S_RESET, -1, S_QUIT};

static const uint8_t OPT_N[NOPT] = {5, 4, 3, 7, 8, 6, 11, 2, 2};
static const uint8_t OPT_DEF[NOPT] = {1, 0, 2, 3, 3, 0, 9, 0, 1};
static const uint32_t PAL[9] = {0xC82323, 0xFF8C1E, 0xFFD25A, 0x0FFF9B, 0x0F009B, 0x002DFF, 0x6432B4, 0x0F0F0F, 0x000000};
static const uint32_t CUBE_FRONT[6] = {0xC8FA78, 0x00FF96, 0xFF6464, 0xAAC3FF, 0xFF9600, 0x8C8C8C};
static const uint32_t CUBE_SIDE[6] = {0x6464C8, 0x323232, 0x465A50, 0x7887B4, 0xB46900, 0x5A5A5A};
static const float SIZES[4] = {0.5f, 0.625f, 0.75f, 0.875f};
static const float LIMITS[4] = {0.5f, 0.532f, 0.554f, 0.578f};
static const int FREQ[4] = {400, 500, 600, 700};
static const uint8_t FACES[5][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {4, 0, 3, 7}, {5, 1, 2, 6}, {3, 2, 6, 7}};
static const uint8_t RESET_SEQ[8] = {K_UP, K_DOWN, K_LEFT, K_RIGHT, K_UP, K_DOWN, K_LEFT, K_RIGHT};
static const uint8_t DEBUG_SEQ[11] = {K_UP, K_UP, K_UP, K_DOWN, K_DOWN, K_DOWN, K_LEFT, K_RIGHT, K_LEFT, K_RIGHT, K_LEFT};

typedef struct {
  const char *s;
  uint8_t color, sc;
} Credit;
static const Credit CREDITS[] = {
  {"BLOCKAMOK REMIX V1.21", 1, 2}, {"", 0, 1}, {"Mode8fx", 2, 1}, {"Remix and ports", 2, 1},
  {"github.com/Mode8fx/blockamok", 0, 1}, {"", 0, 1}, {"Carl Riis", 3, 1}, {"Original game", 3, 1},
  {"github.com/carltheperson/blockamok", 0, 1}, {"", 0, 1}, {"THANKS FOR PLAYING!", 1, 2},
};

static Save V;
static Cube cubes[CMAX];
static Item items[IMAX];
static px buf[SW * BH];
static px bgrow[SH];
static float xlo[BH], xhi[BH];
static uint8_t gmap[128];
static uint32_t rs = 2463534242u;

static int ncube, head, nitems, state, sel, lives;
static float speed, score, bounds, base = BASE_BOUNDS, csize = 0.5f, climit = 0.5f;
static uint32_t now, game_start, invince_start, credits_start, start_tick;
static int hi;
static bool new_hi, debug, used_debug, show_cursor = true, speeding;
static uint8_t keys_now, keys_old, reset_idx, debug_idx;
static px c_bg, c_front, c_side, c_ovl;
static bool overlay_on, trans_on;
static int by0g;

static uint32_t rnd(void) {
  rs ^= rs << 13;
  rs ^= rs >> 17;
  rs ^= rs << 5;
  return rs;
}
static float randf(float a, float b) { return a + (b - a) * (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static float absf(float v) { return v < 0 ? -v : v; }
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

#ifdef __clang__
static inline int ceil_half(float v) { return (int)__builtin_ceilf(v - 0.5f); }
#else
static inline int ceil_half(float v) { return (int)__builtin_lceilf(v - 0.5f); }
#endif

static px rgb(uint32_t c) { return (px)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x1F)); }

static inline px mix(px d, px s, uint32_t a) {
  uint32_t dd = (d | d << 16) & 0x07E0F81Fu, ss = (s | s << 16) & 0x07E0F81Fu;
  uint32_t r = (dd + (((ss - dd) * a) >> 5)) & 0x07E0F81Fu;
  return (px)(r | r >> 16);
}

static uint32_t a32(uint32_t a8) { return (a8 * 33) >> 8; }

static void save(void) {
  V.magic = 'B';
  V.version = 1;
  V.hi = (uint32_t)hi;
  ef_write(SAVE_NAME, &V, sizeof V);
}

static void colors(void) {
  int b = V.opt[O_BG];
  uint32_t bg = PAL[b];
  c_bg = rgb(bg);
  c_front = rgb(CUBE_FRONT[V.opt[O_CUBE]]);
  c_side = rgb(CUBE_SIDE[V.opt[O_CUBE]]);
  int o = V.opt[O_OVL];
  overlay_on = o != 9;
  c_ovl = o == 10 ? c_bg : rgb(PAL[o < 9 ? o : 0]);
  trans_on = V.opt[O_TRANS] == 0;
  for (int y = 0; y < SH; y++) {
    float w = absf((float)y + 0.5f - 120.0f) / 360.0f, a = 83.0f * (1.0f - w) / 255.0f, g = 255.0f * w;
    uint32_t ch[3];
    for (int k = 0; k < 3; k++) {
      float v = (float)((bg >> (16 - 8 * k)) & 255);
      ch[k] = (uint32_t)(v * (1.0f - a) + g * a + 0.5f);
    }
    bgrow[y] = rgb(ch[0] << 16 | ch[1] << 8 | ch[2]);
  }
}

static void make_cubes(void) {
  float s = csize, half = s * 0.5f;
  speed = 100.0f;
  head = 0;
  for (int i = 0; i < ncube; i++) {
    float z = DEPTH / (float)ncube * (float)i;
    float x = randf(-bounds, bounds), y = randf(-bounds, bounds);
    if (z <= 5.0f) {
      for (int t = 0; t < 5 && (absf(x) < 2.0f || absf(y) < 2.0f); t++) {
        x = randf(-bounds, bounds);
        y = randf(-bounds, bounds);
      }
    }
    Cube *c = &cubes[ncube - 1 - i];
    c->x = x - half;
    c->y = y - half;
    c->z = z + s;
  }
}

static void setup(void) {
  csize = SIZES[V.opt[O_SIZE]];
  climit = LIMITS[V.opt[O_SIZE]];
  bounds = base * (1.0f + ((float)V.opt[O_SPAWN] - 3.0f) * 0.1f);
  float mult = bounds * bounds / (12.0f * 12.0f);
  int limit = (int)(800.0f * mult);
  int f = V.opt[O_FREQ];
  ncube = f < 4 ? (int)((float)FREQ[f] * mult) : limit;
  if (ncube > CMAX) ncube = CMAX;
  if (ncube < 1) ncube = 1;
  make_cubes();
}

static void resort(void) {
  int n = ncube;
  for (int pass = 0; pass < 3; pass++) {
    int a = pass == 0 ? 0 : pass == 1 ? head : 0, b = pass == 0 ? head - 1 : n - 1;
    for (; a < b; a++, b--) {
      Cube t = cubes[a];
      cubes[a] = cubes[b];
      cubes[b] = t;
    }
  }
  head = 0;
  for (int i = 1; i < n; i++) {
    Cube t = cubes[i];
    int j = i - 1;
    while (j >= 0 && (cubes[j].z < t.z || (cubes[j].z == t.z && cubes[j].x < t.x))) {
      cubes[j + 1] = cubes[j];
      j--;
    }
    cubes[j + 1] = t;
  }
}

static bool step(uint32_t dt) {
  float k = (float)dt / 12000.0f;
  speed += k * 350.0f * (speeding ? 3.0f : 1.0f);
  if (speed > MAXSPEED) speed = MAXSPEED;
  float zs = speed * k, xd = 0, yd = 0, turn = (30.0f + speed / 50.0f) * k;
  if (keys_now & K_UP) yd = turn;
  if (keys_now & K_DOWN) yd = -turn;
  if (keys_now & K_LEFT) xd = turn;
  if (keys_now & K_RIGHT) xd = -turn;
  if (speeding) zs *= SPEEDUP;
  bool inv = now - invince_start <= 1000 || debug;
  float half = csize * 0.5f, b2 = bounds * 2.0f;
  int resets = 0;
  for (int i = 0; i < ncube; i++) {
    int j = head + i;
    if (j >= ncube) j -= ncube;
    Cube *c = &cubes[j];
    bool rst = c->z - zs < 1.5f;
    if (c->x < -bounds) c->x += b2;
    else if (c->x > bounds) c->x -= b2;
    if (c->y < -bounds) c->y += b2;
    else if (c->y > bounds) c->y -= b2;
    c->x += xd;
    c->y += yd;
    c->z -= zs;
    if (c->z < 2.0f && !inv && absf(c->x + half) < climit && absf(c->y + half) < climit) {
      if (--lives > 0) {
        speed = (speed < MAXSPEED ? speed : MAXSPEED) - MAXSPEED * 0.3f;
        if (speed < 100.0f) speed = 100.0f;
        invince_start = now;
      } else {
        if (rst) c->z = 1.5f;
        score += zs;
        if (score > (float)hi) {
          new_hi = true;
          if (!used_debug) {
            hi = (int)score;
            save();
          }
        }
        head = head - resets;
        if (head < 0) head += ncube;
        resort();
        return true;
      }
    }
    if (rst) {
      c->x = randf(-bounds, bounds);
      c->y = randf(-bounds, bounds);
      c->z += DEPTH;
      resets++;
    }
  }
  score += zs;
  head -= resets;
  if (head < 0) head += ncube;
  return false;
}

static int clamp16(int v) { return v < -30000 ? -30000 : v > 30000 ? 30000 : v; }

static uint8_t fade8(float z, float x, float y, bool line) {
  float zz = z + 7.0f * absf(x) + 7.0f * absf(y);
  if (zz < 150.0f) return 255;
  float f = (zz - 150.0f) * (1.0f / 40.0f);
  if (line) f *= 1.5f;
  if (f >= 1.0f) return 0;
  return (uint8_t)(255.0f - f * 255.0f);
}

static void build(void) {
  nitems = 0;
  float s = csize, ex = 1.5f * s;
  for (int i = ncube - 1; i >= 0 && nitems < IMAX; i--) {
    int j = head + i;
    if (j >= ncube) j -= ncube;
    const Cube *c = &cubes[j];
    float z0 = c->z, zn = z0 - ex;
    if (zn < 0.05f) zn = 0.05f;
    float f0 = PROJ / z0, f1 = PROJ / zn, xa = c->x, xb = xa + s, ya = c->y, yb = ya + s;
    int X0 = (int)(xa * f0 + 160.0f), X1 = (int)(xb * f0 + 160.0f), X2 = (int)(xb * f1 + 160.0f), X3 = (int)(xa * f1 + 160.0f);
    int Y0 = (int)(ya * f0 + 120.0f), Y2 = (int)(ya * f1 + 120.0f), Y4 = (int)(yb * f0 + 120.0f), Y6 = (int)(yb * f1 + 120.0f);
    int xmin = imin(imin(X0, X1), imin(X2, X3)), xmax = imax(imax(X0, X1), imax(X2, X3));
    int ymin = imin(imin(Y0, Y2), imin(Y4, Y6)), ymax = imax(imax(Y0, Y2), imax(Y4, Y6));
    if (xmax < 0 || xmin >= SW || ymax < 0 || ymin >= SH) continue;
    Item *it = &items[nitems];
    int xs[8] = {X0, X1, X2, X3, X0, X1, X2, X3}, ys[8] = {Y0, Y0, Y2, Y2, Y4, Y4, Y6, Y6};
    for (int p = 0; p < 8; p++) {
      it->x[p] = (int16_t)clamp16(xs[p]);
      it->y[p] = (int16_t)clamp16(ys[p]);
    }
    uint8_t fa[5], fl[5];
    if (trans_on) {
      float rz[5] = {z0, z0, z0, z0, zn}, rx[5] = {xa, xa, xa, xb, xa}, ry[5] = {ya, yb, yb, yb, ya};
      for (int f = 0; f < 5; f++) {
        fa[f] = fade8(rz[f], rx[f], ry[f], false);
        fl[f] = fade8(rz[f], rx[f], ry[f], true);
      }
    } else {
      for (int f = 0; f < 5; f++) fa[f] = fl[f] = 255;
    }
    int ord[5], last = 4, first = 0;
    ord[last--] = 4;
    for (int f = 0; f < 4; f++) {
      int p0 = FACES[f][0], p1 = FACES[f][1];
      bool o0 = xs[p0] < X3 || xs[p0] > X2 || ys[p0] < Y2 || ys[p0] > Y6;
      bool o1 = xs[p1] < X3 || xs[p1] > X2 || ys[p1] < Y2 || ys[p1] > Y6;
      if (o0 && o1) ord[last--] = f;
      else ord[first++] = f;
    }
    int n = 0;
    bool front_solid = fa[4] == 255 && fl[4] == 255;
    for (int k = 2; k < 5; k++) {
      int f = ord[k];
      if (a32(fa[f]) == 0 && a32(fl[f]) == 0) continue;
      if (k < 4 && front_solid && fa[f] == 255 && fl[f] == 255) {
        bool inside = true;
        for (int p = 0; p < 4; p++) {
          int q = FACES[f][p];
          if (xs[q] < X3 || xs[q] > X2 || ys[q] < Y2 || ys[q] > Y6) inside = false;
        }
        if (inside) continue;
      }
      it->f[n] = (uint8_t)f;
      it->a[n] = (uint8_t)a32(fa[f]);
      it->l[n] = (uint8_t)a32(fl[f]);
      n++;
    }
    if (!n) continue;
    it->n = (uint8_t)n;
    it->y0 = (int16_t)clamp16(ymin);
    it->y1 = (int16_t)clamp16(ymax);
    nitems++;
  }
}

static void edge(int xa, int ya, int xb, int yb) {
  if (ya == yb) return;
  if (ya > yb) {
    int t = xa;
    xa = xb;
    xb = t;
    t = ya;
    ya = yb;
    yb = t;
  }
  int r0 = imax(ya, by0g), r1 = imin(yb, by0g + BH);
  if (r0 >= r1) return;
  float sl = (float)(xb - xa) / (float)(yb - ya), x = (float)xa + ((float)r0 + 0.5f - (float)ya) * sl;
  for (int r = r0; r < r1; r++, x += sl) {
    int i = r - by0g;
    if (x < xlo[i]) xlo[i] = x;
    if (x > xhi[i]) xhi[i] = x;
  }
}

static void fill_face(const int *vx, const int *vy, px col, uint32_t a) {
  int ymin = vy[0], ymax = vy[0];
  for (int p = 1; p < 4; p++) {
    ymin = imin(ymin, vy[p]);
    ymax = imax(ymax, vy[p]);
  }
  int r0 = imax(ymin, by0g), r1 = imin(ymax, by0g + BH);
  if (r0 >= r1) return;
  for (int r = r0; r < r1; r++) {
    xlo[r - by0g] = 1e9f;
    xhi[r - by0g] = -1e9f;
  }
  for (int p = 0; p < 4; p++) edge(vx[p], vy[p], vx[(p + 1) & 3], vy[(p + 1) & 3]);
  for (int r = r0; r < r1; r++) {
    int i = r - by0g;
    if (xlo[i] > xhi[i]) continue;
    int a0 = imax(ceil_half(xlo[i]), 0), a1 = imin(ceil_half(xhi[i]), SW);
    px *row = buf + i * SW;
    if (a == 32) {
      for (int x = a0; x < a1; x++) row[x] = col;
    } else {
      for (int x = a0; x < a1; x++) row[x] = mix(row[x], col, a);
    }
  }
}

static void line(int x0, int y0, int x1, int y1, uint32_t a) {
  int lo = by0g, hi = by0g + BH - 1;
  int xmn = imin(x0, x1), xmx = imax(x0, x1), ymn = imin(y0, y1), ymx = imax(y0, y1);
  if (xmx < 0 || xmn >= SW || ymx < lo || ymn > hi) return;
  float ax = (float)x0, ay = (float)y0, bx = (float)x1, by = (float)y1;
  if (xmn < 0 || xmx >= SW || ymn < lo || ymx > hi) {
    float dx = bx - ax, dy = by - ay, t0 = 0, t1 = 1;
    float p[4] = {-dx, dx, -dy, dy};
    float q[4] = {ax, (float)(SW - 1) - ax, ay - (float)lo, (float)hi - ay};
    for (int i = 0; i < 4; i++) {
      if (p[i] == 0) {
        if (q[i] < 0) return;
      } else {
        float t = q[i] / p[i];
        if (p[i] < 0) {
          if (t > t1) return;
          if (t > t0) t0 = t;
        } else {
          if (t < t0) return;
          if (t < t1) t1 = t;
        }
      }
    }
    bx = ax + t1 * dx;
    by = ay + t1 * dy;
    ax += t0 * dx;
    ay += t0 * dy;
  }
  int fx = (int)(ax * 65536.0f) + 32768, fy = (int)(ay * 65536.0f) + 32768;
  int ex = (int)(bx * 65536.0f) + 32768, ey = (int)(by * 65536.0f) + 32768;
  int ddx = ex - fx, ddy = ey - fy;
  int n = imax(ddx < 0 ? -ddx : ddx, ddy < 0 ? -ddy : ddy) >> 16;
  int sx = 0, sy = 0;
  if (n) {
    sx = ddx / n;
    sy = ddy / n;
  }
  for (int i = 0; i <= n; i++, fx += sx, fy += sy) {
    int x = fx >> 16, y = fy >> 16;
    if (x < 0 || x >= SW || y < lo || y > hi) continue;
    px *d = &buf[(y - lo) * SW + x];
    *d = a == 32 ? 0 : mix(*d, 0, a);
  }
}

static void draw_cubes(void) {
  for (int i = nitems - 1; i >= 0; i--) {
    const Item *it = &items[i];
    if (it->y1 < by0g || it->y0 >= by0g + BH) continue;
    for (int k = 0; k < it->n; k++) {
      int vx[4], vy[4], f = it->f[k];
      for (int p = 0; p < 4; p++) {
        vx[p] = it->x[FACES[f][p]];
        vy[p] = it->y[FACES[f][p]];
      }
      if (it->a[k]) fill_face(vx, vy, f == 4 ? c_front : c_side, it->a[k]);
      if (it->l[k])
        for (int p = 0; p < 4; p++) line(vx[p], vy[p], vx[(p + 1) & 3], vy[(p + 1) & 3], it->l[k]);
    }
  }
}

static void rect(int x, int y, int w, int h, px c) {
  int y0 = imax(y, by0g), y1 = imin(y + h, by0g + BH), x0 = imax(x, 0), x1 = imin(x + w, SW);
  for (int r = y0; r < y1; r++)
    for (int q = x0; q < x1; q++) buf[(r - by0g) * SW + q] = c;
}

static void shade(int x0, int x1, uint32_t a) {
  for (int r = 0; r < BH; r++)
    for (int q = x0; q < x1; q++) buf[r * SW + q] = mix(buf[r * SW + q], 0, a);
}

static void glyph(int x, int y, int g, int sc, px c) {
  for (int r = 0; r < 7; r++) {
    uint8_t bits = FONT[g][r];
    for (int q = 0; q < 5; q++)
      if (bits >> (4 - q) & 1) rect(x + q * sc, y + r * sc, sc, sc, c);
  }
}

static int twidth(const char *s, int sc, int gap) {
  int n = 0;
  while (s[n]) n++;
  return n ? n * (5 * sc + gap) - gap : 0;
}

static void text(int x, int y, const char *s, int sc, int gap, px c, px oc) {
  int o = (sc + 1) / 2;
  if (y + 7 * sc + o <= by0g || y - o >= by0g + BH) return;
  for (int pass = 0; pass < 2; pass++) {
    int px0 = x;
    for (const char *p = s; *p; p++) {
      int g = gmap[(uint8_t)*p & 127];
      if (pass == 0) {
        for (int dy = -o; dy <= o; dy += o)
          for (int dx = -o; dx <= o; dx += o)
            if (dx || dy) glyph(px0 + dx, y + dy, g, sc, oc);
      } else {
        glyph(px0, y, g, sc, c);
      }
      px0 += 5 * sc + gap;
    }
  }
}

static void ctext(int cx, int cy, const char *s, int sc, int gap, px c, px oc) {
  text(cx - twidth(s, sc, gap) / 2, cy - 7 * sc / 2, s, sc, gap, c, oc);
}

static void rtext(int rx, int cy, const char *s, int sc, int gap, px c, px oc) {
  text(rx - twidth(s, sc, gap), cy - 7 * sc / 2, s, sc, gap, c, oc);
}

static char *utoa(uint32_t v, char *b) {
  char t[12];
  int n = 0;
  do t[n++] = (char)('0' + v % 10); while (v /= 10);
  int i = 0;
  while (n) b[i++] = t[--n];
  b[i] = 0;
  return b;
}

static void disc(int cx, int cy, int r, px c, px oc) {
  for (int y = -r - 1; y <= r + 1; y++)
    for (int x = -r - 1; x <= r + 1; x++) {
      int d = x * x + y * y;
      if (d <= (r + 1) * (r + 1)) rect(cx + x, cy + y, 1, 1, d <= r * r ? c : oc);
    }
}

#define WHITE rgb(0xFFFFFF)
#define BLACK 0
#define ORANGE rgb(0xFFA000)
#define GRAY rgb(0xD0D0D0)
#define RED rgb(0xFF5C5C)
#define BLUE rgb(0x8080FF)

static const px *tcolors(void) {
  static px t[5];
  t[0] = WHITE;
  t[1] = ORANGE;
  t[2] = BLUE;
  t[3] = RED;
  t[4] = GRAY;
  return t;
}

static void draw_hud(void) {
  char b[16];
  ctext(160, 12, utoa((uint32_t)score, b), 2, 1, WHITE, BLACK);
  uint32_t it = now - invince_start;
  if (it > 1000 || it / 111 % 2 == 1 || game_start == invince_start) {
    int rx = overlay_on ? 270 : 308;
    for (int i = 0; i < lives; i++) disc(rx - 16 * i, 12, 5, RED, BLUE);
  }
  if (V.opt[O_SPEED]) {
    float ps = speed * (speeding ? SPEEDUP : 1.0f);
    char t[24];
    utoa((uint32_t)ps, t);
    int n = 0;
    while (t[n]) n++;
    const char *mph = " MPH";
    for (int i = 0; i < 5; i++) t[n + i] = mph[i];
    rtext(overlay_on ? 270 : 306, 228, t, 1, 1, ps < (speeding ? MAXSPEED * SPEEDUP : MAXSPEED) ? WHITE : ORANGE, BLACK);
  }
  if (debug) {
    char t[24], u[12];
    utoa((uint32_t)(base * 10.0f), t);
    utoa((uint32_t)ncube, u);
    ctext(160, 216, t, 1, 1, WHITE, BLACK);
    ctext(160, 226, u, 1, 1, WHITE, BLACK);
  }
}

static void draw_cursor(void) {
  for (int i = -7; i <= 7; i++) {
    int y = 120 + i, x = 160 + i;
    if (y >= by0g && y < by0g + BH)
      for (int t = -1; t <= 0; t++) {
        px *d = &buf[(y - by0g) * SW + 160 + t];
        *d = mix(*d, WHITE, 8);
      }
    if (x >= 0 && x < SW)
      for (int t = -1; t <= 0; t++) {
        int yy = 120 + t;
        if (yy >= by0g && yy < by0g + BH) {
          px *d = &buf[(yy - by0g) * SW + x];
          *d = mix(*d, WHITE, 8);
        }
      }
  }
}

static void draw_page(const Line *lines, int n, const px *cols) {
  for (int i = 0; i < n; i++) {
    const Line *l = &lines[i];
    px c = i == sel ? ORANGE : WHITE;
    text(28, 24 + 18 * i - 7, l->name, 2, 1, c, BLACK);
    rtext(304, 24 + 18 * i, l->ch[V.opt[l->opt]], 2, 1, i == sel ? ORANGE : GRAY, BLACK);
  }
  text(12, 24 + 18 * sel - 7, ">", 2, 1, WHITE, BLACK);
  const Line *l = &lines[sel];
  const char *d[3] = {l->d0, l->d1, l->d2};
  for (int i = 0; i < 3; i++)
    if (d[i]) ctext(160, 190 + 10 * i, d[i], 1, 1, GRAY, BLACK);
  (void)cols;
}

static void draw_ui(void) {
  const px *cols = tcolors();
  switch (state) {
    case S_START:
    case S_TITLE:
      ctext(160, 96, "BLOCKAMOK", 5, 3, WHITE, BLACK);
      ctext(160, 126, "REMIX", 3, 2, BLACK, WHITE);
      if (state == S_TITLE) {
        char b[24], t[32] = "High Score: ";
        utoa((uint32_t)hi, b);
        int n = 12, i = 0;
        while (b[i]) t[n++] = b[i++];
        t[n] = 0;
        ctext(160, 156, "Press EXE to fly", 1, 1, WHITE, BLACK);
        ctext(160, 180, "Press BACK for options", 1, 1, WHITE, BLACK);
        ctext(160, 216, t, 2, 1, ORANGE, BLACK);
      }
      break;
    case S_MAIN:
      for (int i = 0; i < 7; i++) text(28, 24 + 18 * i - 7, MAIN_NAMES[i], 2, 1, i == sel ? ORANGE : WHITE, BLACK);
      text(12, 24 + 18 * sel - 7, ">", 2, 1, WHITE, BLACK);
      break;
    case S_GAME:
      draw_page(GAME_LINES, 4, cols);
      break;
    case S_VISUAL:
      draw_page(VIS_LINES, 5, cols);
      break;
    case S_INSTR:
      ctext(160, 36, "Dodge the incoming blocks!", 1, 1, ORANGE, BLACK);
      ctext(160, 54, "Hold OK or BACK to speed up.", 1, 1, GRAY, BLACK);
      ctext(160, 72, "Press DEL to toggle cursor.", 1, 1, GRAY, BLACK);
      ctext(160, 90, "Press EXE to pause.", 1, 1, GRAY, BLACK);
      ctext(160, 144, "Check the Options menu", 1, 1, ORANGE, BLACK);
      ctext(160, 162, "to customize your game!", 1, 1, ORANGE, BLACK);
      break;
    case S_CREDITS: {
      int off = (int)((now - credits_start) / 30) % 330;
      for (int i = 0; i < 11; i++) {
        int y = 250 - off + 18 * i;
        if (y > -20 && y < 260) ctext(160, y, CREDITS[i].s, CREDITS[i].sc, 1, cols[CREDITS[i].color], BLACK);
      }
      break;
    }
    case S_RESET:
      ctext(160, 84, "Are you sure you want to", 1, 1, WHITE, BLACK);
      ctext(160, 102, "reset your high score?", 1, 1, WHITE, BLACK);
      ctext(160, 138, "If so, press", 1, 1, WHITE, BLACK);
      ctext(160, 156, "Up Down Left Right", 1, 1, RED, BLACK);
      ctext(160, 174, "Up Down Left Right", 1, 1, RED, BLACK);
      break;
    case S_QUIT:
      ctext(160, 108, "Are you sure you want to quit?", 1, 1, WHITE, BLACK);
      ctext(160, 132, "Press OK to quit", 1, 1, RED, BLACK);
      break;
    case S_PLAY:
    case S_PAUSE:
    case S_OVER:
      draw_hud();
      if (state == S_PLAY && show_cursor) draw_cursor();
      if (state == S_PAUSE) {
        ctext(160, 120, "PAUSED", 5, 3, WHITE, BLACK);
        ctext(160, 156, "Press BACK to quit", 1, 1, WHITE, BLACK);
      }
      if (state == S_OVER) {
        ctext(160, 120, "GAME OVER", 5, 3, WHITE, BLACK);
        if (new_hi) ctext(160, 180, used_debug ? "Now try it without debug mode!" : "New High Score!", 1, 1, ORANGE, BLACK);
      }
      break;
  }
}

static void render(uint32_t fade) {
  bool dim = state == S_MAIN || state == S_GAME || state == S_INSTR || state == S_CREDITS || state == S_RESET || state == S_QUIT;
  for (int b = 0; b < NB; b++) {
    by0g = b * BH;
    for (int r = 0; r < BH; r++) {
      uint32_t c = bgrow[by0g + r] * 0x10001u;
      uint32_t *row = (uint32_t *)(void *)(buf + r * SW);
      for (int q = 0; q < SW / 2; q++) row[q] = c;
    }
    draw_cubes();
    if (overlay_on) {
      for (int r = 0; r < BH; r++) {
        px *row = buf + r * SW;
        for (int q = 0; q < 40; q++) row[q] = row[SW - 1 - q] = c_ovl;
        row[38] = row[39] = row[280] = row[281] = 0;
      }
    }
    if (dim) shade(overlay_on ? 40 : 0, overlay_on ? 280 : SW, 8);
    draw_ui();
    if (fade) shade(0, SW, fade);
    eadk_display_push_rect((eadk_rect_t){0, (uint16_t)by0g, SW, BH}, buf);
  }
}

static void load(void) {
  uint32_t n = 0;
  const uint8_t *d = ef_read(SAVE_NAME, &n);
  for (int i = 0; i < NOPT; i++) V.opt[i] = OPT_DEF[i];
  hi = HISCORE0;
  if (d && n == sizeof V && d[0] == 'B' && d[1] == 1) {
    for (uint32_t i = 0; i < n; i++) ((uint8_t *)&V)[i] = d[i];
    hi = (int)V.hi;
    for (int i = 0; i < NOPT; i++)
      if (V.opt[i] >= OPT_N[i]) V.opt[i] = OPT_DEF[i];
  }
}

static uint8_t read_keys(void) {
  uint64_t s = eadk_keyboard_scan();
  uint8_t k = 0;
#define B(key) (s >> (key) & 1)
  if (B(eadk_key_up) || B(eadk_key_eight)) k |= K_UP;
  if (B(eadk_key_down) || B(eadk_key_two)) k |= K_DOWN;
  if (B(eadk_key_left) || B(eadk_key_four)) k |= K_LEFT;
  if (B(eadk_key_right) || B(eadk_key_six)) k |= K_RIGHT;
  if (B(eadk_key_ok)) k |= K_OK;
  if (B(eadk_key_back)) k |= K_BACK;
  if (B(eadk_key_exe)) k |= K_EXE;
  if (B(eadk_key_backspace)) k |= K_DEL;
#undef B
  return k;
}

static void new_run(void) {
  score = 0;
  lives = V.opt[O_LIVES] + 1;
  game_start = now;
  invince_start = now;
  new_hi = false;
}

static void to_title(void) {
  new_hi = false;
  used_debug = false;
  setup();
  state = S_TITLE;
}

static void changed(int o) {
  if (o == O_FREQ || o == O_SIZE || o == O_SPAWN) setup();
  else colors();
}

static void menu(const Line *lines, int n, uint8_t pressed) {
  if (pressed & K_UP) sel = (sel + n - 1) % n;
  if (pressed & K_DOWN) sel = (sel + 1) % n;
  const Line *l = &lines[sel];
  if (pressed & K_LEFT) {
    V.opt[l->opt] = (uint8_t)((V.opt[l->opt] + l->n - 1) % l->n);
    changed(l->opt);
  }
  if (pressed & K_RIGHT) {
    V.opt[l->opt] = (uint8_t)((V.opt[l->opt] + 1) % l->n);
    changed(l->opt);
  }
}

int main(void) {
  np_app_begin();
  for (int i = 0; FONT_CHARS[i]; i++) {
    gmap[(uint8_t)FONT_CHARS[i]] = (uint8_t)i;
    if (FONT_CHARS[i] >= 'A' && FONT_CHARS[i] <= 'Z') gmap[(uint8_t)FONT_CHARS[i] + 32] = (uint8_t)i;
  }
  now = (uint32_t)eadk_timing_millis();
  rs = eadk_random() ^ now ^ 0x9E3779B9u;
  if (!rs) rs = 1;
  load();
  colors();
  setup();
  state = S_START;
  start_tick = now;
  uint32_t last = now;
  bool redraw = true, quit = false;
  while (!quit) {
    last = now;
    now = (uint32_t)eadk_timing_millis();
    uint32_t dt = now - last;
    if (dt > 100) dt = 100;
    keys_old = keys_now;
    keys_now = read_keys();
    uint8_t pressed = keys_now & (uint8_t)~keys_old;
    if (pressed) redraw = true;
    speeding = (keys_now & (K_OK | K_BACK)) != 0;
    uint32_t fade = 0;
    switch (state) {
      case S_START: {
        uint32_t e = now - start_tick;
        if (e > 2000) state = S_TITLE;
        else fade = a32(255 - 255 * e / 2000);
        redraw = true;
        break;
      }
      case S_TITLE:
        if (pressed & K_EXE) {
          new_run();
          state = S_PLAY;
        } else if (pressed & K_BACK) {
          state = S_MAIN;
          sel = 0;
        }
        break;
      case S_MAIN:
        if (pressed & K_UP) sel = (sel + 6) % 7;
        if (pressed & K_DOWN) sel = (sel + 1) % 7;
        if (pressed & (K_OK | K_EXE)) {
          int g = MAIN_GOTO[sel];
          if (sel == 3) credits_start = now;
          if (sel == 5) {
            for (int i = 0; i < NOPT; i++) V.opt[i] = OPT_DEF[i];
            colors();
            setup();
          } else {
            state = g;
            sel = 0;
            reset_idx = 0;
          }
        } else if (pressed & K_BACK) {
          save();
          state = S_TITLE;
        }
        break;
      case S_GAME:
        if (pressed & K_BACK) {
          state = S_MAIN;
          sel = 0;
        } else {
          menu(GAME_LINES, 4, pressed & ~K_BACK);
        }
        break;
      case S_VISUAL:
        if (pressed & K_BACK) {
          state = S_MAIN;
          sel = 1;
        } else {
          menu(VIS_LINES, 5, pressed & ~K_BACK);
        }
        break;
      case S_INSTR:
        if (pressed & K_BACK) {
          state = S_MAIN;
          sel = 2;
        }
        break;
      case S_CREDITS:
        redraw = true;
        if (pressed & K_BACK) {
          state = S_MAIN;
          sel = 3;
        }
        break;
      case S_RESET:
        if (pressed & RESET_SEQ[reset_idx]) {
          if (++reset_idx >= 8) {
            hi = HISCORE0;
            save();
            reset_idx = 0;
            state = S_MAIN;
            sel = 4;
          }
        } else if (pressed & K_BACK) {
          state = S_MAIN;
          sel = 4;
        } else if (pressed) {
          reset_idx = 0;
        }
        break;
      case S_QUIT:
        if (pressed & (K_OK | K_EXE)) {
          save();
          quit = true;
        } else if (pressed & K_BACK) {
          state = S_MAIN;
          sel = 6;
        }
        break;
      case S_PLAY:
        redraw = true;
        if (step(dt)) {
          state = S_OVER;
        } else if (pressed & K_EXE) {
          state = S_PAUSE;
        } else if (pressed & K_DEL) {
          show_cursor = !show_cursor;
        }
        break;
      case S_PAUSE:
        if (debug) {
          if ((pressed & K_LEFT) && base > 2.5f && debug_idx < 6) {
            base -= 0.1f;
            setup();
          } else if ((pressed & K_RIGHT) && base < 13.1f && debug_idx < 6) {
            base += 0.1f;
            setup();
          }
        }
        if (pressed & DEBUG_SEQ[debug_idx]) {
          if (++debug_idx >= 11) {
            debug = !debug;
            debug_idx = 0;
          }
        } else if (pressed) {
          debug_idx = 0;
        }
        if (pressed & K_EXE) state = S_PLAY;
        else if (pressed & K_BACK) to_title();
        break;
      case S_OVER:
        if (pressed & K_EXE) to_title();
        break;
    }
    if (quit) break;
    eadk_display_wait_for_vblank();
    if (redraw) {
      build();
      render(fade);
      redraw = false;
    }
  }
  return np_app_end();
}
