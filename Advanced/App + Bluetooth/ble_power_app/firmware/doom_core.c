/*
 * doom_core.c  -  see doom_core.h
 *
 * Fixed-point conventions:
 *   positions  Q8.8 tiles   (256 = one map tile)
 *   angles     0..1023      (1024 = full turn, 0 = +x, increasing towards +y)
 *   sin/cos    Q14          (16384 = 1.0)
 */

#include "common.h"
#if ENABLE_DOOM


#include "doom_core.h"
#include "doom_tables.h"
#include "oled.h"
#include "font5x7.h"
#include "scratch.h"
#include <string.h>

#define MAP_N        16
#define N_DEMONS     4
#define WALL_K       ((uint32_t)(DOOM_DPX * 256.0f * 0.40f))  /* wall height scale  */
#define DEMON_K      ((uint32_t)(DOOM_DPX * 256.0f * 0.30f))  /* demon height scale */
#define MOVE_STEP    64            /* 0.25 tile per key press        */
#define TURN_STEP    32            /* 11.25 degrees per key press    */
#define WALL_MARGIN  48            /* keep 0.19 tile away from walls */
#define DEMON_SPEED  170           /* Q8.8 tiles per second (0.66)   */
#define DEMON_REACH  140           /* attack distance, Q8.8 (0.55 tile) */
#define HIT_DAMAGE   10
#define HIT_EVERY_MS 600
#define RESPAWN_MS   3000

static const char map_rows[MAP_N][MAP_N + 1] PROGMEM = {
	"################",
	"#......#.......#",
	"#.##...#..##...#",
	"#.#....#...#...#",
	"#.#..#####.#...#",
	"#............#.#",
	"###.###..###.#.#",
	"#.....#......#.#",
	"#.#...#..#.....#",
	"#.#####..#.###.#",
	"#........#...#.#",
	"#.###.####.#.#.#",
	"#...#......#...#",
	"#.#.#.####.###.#",
	"#.#............#",
	"################",
};

/* 8 x 8 demon, row 0 at the top, bit 7 = left */
static const uint8_t demon_bmp[8] PROGMEM = {
	0x81, /* #......# horns */
	0xC3, /* ##....## */
	0x7E, /* .######. */
	0xDB, /* ##.##.## eyes */
	0xFF, /* ######## */
	0xA5, /* #.#..#.# teeth */
	0x7E, /* .######. */
	0x66, /* .##..##. legs */
};


typedef struct {
	int16_t  x, y;          /* Q8.8 */
	uint8_t  alive;
	uint16_t respawn_ms;
} demon_t;

static int16_t  px, py, pa;
static int8_t   hp;
static uint16_t kills;
static uint8_t  dead, title, events;
static uint16_t flash_ms, hurt_ms, attack_ms;
static uint16_t rng;
static demon_t  dm[N_DEMONS];
static int8_t   path_px = -1, path_py = -1;   /* player tile the path map was built for */

/* Path map: steps from every tile to the player's tile (255 = wall/unreachable).
 * Kept in the shared scratch buffer (the ADC buffers are idle in DOOM mode). */
#define PATH ((uint8_t *)scratch_buf)

/* ---------------------------------------------------------------- maths */

static int16_t isin(int16_t a)
{
	a &= 1023;
	if (a < 256) return  (int16_t)pgm_read_word(&sin_q[a]);
	if (a < 512) return  (int16_t)pgm_read_word(&sin_q[512 - a]);
	if (a < 768) return -(int16_t)pgm_read_word(&sin_q[a - 512]);
	return              -(int16_t)pgm_read_word(&sin_q[1024 - a]);
}
static int16_t icos(int16_t a) { return isin((int16_t)(a + 256)); }

static uint16_t inv_abs_sin(int16_t a)
{
	a &= 1023;
	uint16_t i = (a < 256) ? (uint16_t)a : (a < 512) ? (uint16_t)(512 - a)
	           : (a < 768) ? (uint16_t)(a - 512) : (uint16_t)(1024 - a);
	return pgm_read_word(&inv_q[i]);
}
static uint16_t inv_abs_cos(int16_t a) { return inv_abs_sin((int16_t)(a + 256)); }

static uint16_t rnd(void)
{
	rng = (uint16_t)(rng * 25173u + 13849u);
	return rng;
}

static uint8_t wall(int16_t mx, int16_t my)
{
	if (mx < 0 || my < 0 || mx >= MAP_N || my >= MAP_N) return 1;
	return pgm_read_byte(&map_rows[my][mx]) == '#';
}

static uint8_t wall_at(int16_t x, int16_t y)   /* Q8.8 position */
{
	return wall(x >> 8, y >> 8);
}

/* Cast one ray. Returns the distance along the ray (Q8.8); *side = 0 for x walls. */
static uint16_t cast(int16_t ra, uint8_t *side)
{
	int16_t  cs = icos(ra), sn = isin(ra);
	uint16_t ddx = inv_abs_cos(ra), ddy = inv_abs_sin(ra);
	int8_t   mx = (int8_t)(px >> 8), my = (int8_t)(py >> 8);
	uint8_t  fx = (uint8_t)px, fy = (uint8_t)py;
	int8_t   sx, sy;
	uint32_t sdx, sdy;

	if (cs < 0) { sx = -1; sdx = ((uint32_t)fx * ddx) >> 8; }
	else        { sx =  1; sdx = ((uint32_t)(256 - fx) * ddx) >> 8; }
	if (sn < 0) { sy = -1; sdy = ((uint32_t)fy * ddy) >> 8; }
	else        { sy =  1; sdy = ((uint32_t)(256 - fy) * ddy) >> 8; }

	uint8_t s = 0;
	for (uint8_t n = 0; n < 32; n++) {
		if (sdx < sdy) { sdx += ddx; mx += sx; s = 0; }
		else           { sdy += ddy; my += sy; s = 1; }
		if (wall(mx, my)) break;
	}
	*side = s;
	uint32_t d = s ? sdy - ddy : sdx - ddx;
	return d > 65535u ? 65535u : (uint16_t)d;
}

/* ---------------------------------------------------------------- game */

/* Rebuild the path map when the player enters a new tile: breadth-first search
 * from the player's tile; the queue uses the second half of the scratch buffer. */
#define QUEUE (((uint8_t *)scratch_buf) + 256)
static void build_path(void)
{
	int8_t tx = (int8_t)(px >> 8), ty = (int8_t)(py >> 8);
	if (tx == path_px && ty == path_py) return;
	path_px = tx; path_py = ty;

	for (uint16_t i = 0; i < MAP_N * MAP_N; i++) PATH[i] = 255;
	uint8_t head = 0, tail = 0;
	uint8_t start = (uint8_t)(ty * MAP_N + tx);
	PATH[start] = 0;
	QUEUE[tail++] = start;
	while (head != tail) {
		uint8_t i = QUEUE[head++];
		uint8_t nd = (uint8_t)(PATH[i] + 1);
		uint8_t nb[4] = { (uint8_t)(i - 1), (uint8_t)(i + 1), (uint8_t)(i - MAP_N), (uint8_t)(i + MAP_N) };
		for (uint8_t k = 0; k < 4; k++) {
			uint8_t j = nb[k];
			if (PATH[j] != 255) continue;
			if (pgm_read_byte(&map_rows[j / MAP_N][j % MAP_N]) == '#') continue;
			PATH[j] = nd;
			QUEUE[tail++] = j;          /* at most 256 tiles, tail never laps head */
		}
	}
}

static void spawn(demon_t *d)
{
	for (uint8_t tries = 0; tries < 40; tries++) {
		int16_t mx = (int16_t)(1 + rnd() % (MAP_N - 2));
		int16_t my = (int16_t)(1 + rnd() % (MAP_N - 2));
		if (wall(mx, my)) continue;
		int16_t dx = (int16_t)(mx - (px >> 8)), dy = (int16_t)(my - (py >> 8));
		if (dx * dx + dy * dy < 16) continue;        /* at least 4 tiles away */
		d->x = (int16_t)((mx << 8) + 128);
		d->y = (int16_t)((my << 8) + 128);
		d->alive = 1;
		return;
	}
	d->respawn_ms = 500;                             /* try again shortly */
}

static void new_game(void)
{
	px = (1 << 8) + 128; py = (1 << 8) + 128; pa = 0;     /* top-left room, facing east */
	path_px = path_py = -1;
	hp = 100; kills = 0; dead = 0;
	flash_ms = hurt_ms = attack_ms = 0;
	for (uint8_t k = 0; k < N_DEMONS; k++) { dm[k].alive = 0; dm[k].respawn_ms = 0; spawn(&dm[k]); }
}

void doom_reset(uint16_t seed)
{
	rng = seed | 1;
	new_game();
	title = 1;
	events = 0;
}

static void try_move(int16_t dx, int16_t dy)
{
	int16_t nx = (int16_t)(px + dx), ny = (int16_t)(py + dy);
	int16_t mxk = (int16_t)(nx + (dx > 0 ? WALL_MARGIN : dx < 0 ? -WALL_MARGIN : 0));
	int16_t myk = (int16_t)(ny + (dy > 0 ? WALL_MARGIN : dy < 0 ? -WALL_MARGIN : 0));
	if (!wall_at(mxk, py)) px = nx;                  /* slide along walls */
	if (!wall_at(px, myk)) py = ny;
}

/* depth (along view) and lateral offset of a point, both Q8.8 */
static void to_view(int16_t x, int16_t y, int16_t *depth, int16_t *lat)
{
	int32_t dx = x - px, dy = y - py;
	int16_t ca = icos(pa), sa = isin(pa);
	*depth = (int16_t)((dx * ca + dy * sa) >> 14);
	*lat   = (int16_t)((-dx * sa + dy * ca) >> 14);
}

static void fire(void)
{
	uint8_t side;
	uint16_t wall_d = cast(pa, &side);
	int8_t best = -1;
	int16_t best_depth = 0x7FFF;

	flash_ms = 120;
	for (uint8_t k = 0; k < N_DEMONS; k++) {
		if (!dm[k].alive) continue;
		int16_t depth, lat;
		to_view(dm[k].x, dm[k].y, &depth, &lat);
		if (depth < 64 || (uint16_t)depth > wall_d) continue;
		/* hit if the demon's centre is within ~0.3 tile of the aim line */
		int16_t al = lat < 0 ? (int16_t)-lat : lat;
		if (al < 80 && depth < best_depth) { best = (int8_t)k; best_depth = depth; }
	}
	if (best >= 0) {
		dm[best].alive = 0;
		dm[best].respawn_ms = RESPAWN_MS;
		kills++;
		events |= DOOM_EV_KILL;
	}
}

void doom_key(char k)
{
	if (k >= 'A' && k <= 'Z') k = (char)(k - 'A' + 'a');
	if (title) { title = 0; return; }
	if (dead)  { if (k == 'f') new_game(); return; }

	int16_t ca = icos(pa), sa = isin(pa);
	switch (k) {
		case 'w': try_move((int16_t)(( (int32_t)ca * MOVE_STEP) >> 14), (int16_t)(( (int32_t)sa * MOVE_STEP) >> 14)); break;
		case 's': try_move((int16_t)((-(int32_t)ca * MOVE_STEP) >> 14), (int16_t)((-(int32_t)sa * MOVE_STEP) >> 14)); break;
		case 'q': try_move((int16_t)(( (int32_t)sa * MOVE_STEP) >> 14), (int16_t)((-(int32_t)ca * MOVE_STEP) >> 14)); break;
		case 'e': try_move((int16_t)((-(int32_t)sa * MOVE_STEP) >> 14), (int16_t)(( (int32_t)ca * MOVE_STEP) >> 14)); break;
		case 'a': pa = (int16_t)((pa - TURN_STEP) & 1023); break;
		case 'd': pa = (int16_t)((pa + TURN_STEP) & 1023); break;
		case 'f': fire(); break;
		default: break;
	}
}

void doom_update(uint16_t dt)
{
	flash_ms = flash_ms > dt ? flash_ms - dt : 0;
	hurt_ms  = hurt_ms  > dt ? hurt_ms  - dt : 0;
	if (title || dead) return;

	int16_t step = (int16_t)(((uint32_t)DEMON_SPEED * dt) / 1000u);
	build_path();
	uint8_t touching = 0;

	for (uint8_t k = 0; k < N_DEMONS; k++) {
		demon_t *d = &dm[k];
		if (!d->alive) {
			if (d->respawn_ms > dt) d->respawn_ms -= dt;
			else { d->respawn_ms = 0; spawn(d); }
			continue;
		}
		/* head for the neighbouring tile that is closer to the player,
		 * or straight at the player once in the same or next tile */
		int16_t tx = px, ty = py;
		uint8_t i = (uint8_t)((d->y >> 8) * MAP_N + (d->x >> 8));
		if (PATH[i] == 255) continue;                 /* can't reach the player */
		if (PATH[i] > 1) {
			uint8_t best = PATH[i], bi = i;
			if (PATH[i - 1]     < best) { best = PATH[i - 1];     bi = (uint8_t)(i - 1); }
			if (PATH[i + 1]     < best) { best = PATH[i + 1];     bi = (uint8_t)(i + 1); }
			if (PATH[i - MAP_N] < best) { best = PATH[i - MAP_N]; bi = (uint8_t)(i - MAP_N); }
			if (PATH[i + MAP_N] < best) { best = PATH[i + MAP_N]; bi = (uint8_t)(i + MAP_N); }
			tx = (int16_t)(((bi % MAP_N) << 8) + 128);
			ty = (int16_t)(((bi / MAP_N) << 8) + 128);
		}
		int16_t dx = (int16_t)(tx - d->x), dy = (int16_t)(ty - d->y);
		int16_t ax = dx < 0 ? (int16_t)-dx : dx, ay = dy < 0 ? (int16_t)-dy : dy;
		int16_t dist = (int16_t)(ax > ay ? ax + ay / 2 : ay + ax / 2);   /* ~ distance */
		int16_t pdx = (int16_t)(px - d->x), pdy = (int16_t)(py - d->y);
		int16_t near = (int16_t)((pdx < 0 ? -pdx : pdx) + (pdy < 0 ? -pdy : pdy));
		if (near > DEMON_REACH && dist > 0) {
			int16_t mx = (int16_t)((int32_t)dx * step / dist);
			int16_t my = (int16_t)((int32_t)dy * step / dist);
			if (mx > dx || mx < -dx) mx = dx;                            /* don't overshoot */
			if (my > dy || my < -dy) my = dy;
			d->x += mx;
			d->y += my;
		} else {
			touching = 1;
		}
	}

	if (touching) {
		attack_ms += dt;
		if (attack_ms >= HIT_EVERY_MS) {
			attack_ms -= HIT_EVERY_MS;
			hp = (int8_t)(hp - HIT_DAMAGE);
			hurt_ms = 150;
			events |= DOOM_EV_HURT;
			if (hp <= 0) { hp = 0; dead = 1; events |= DOOM_EV_DEAD; }
		}
	} else {
		attack_ms = 0;
	}
}

/* ---------------------------------------------------------------- drawing */

static void text_screen(const char *l0, const char *l1, const char *l2, const char *l3)
{
	const char *lines[4] = { l0, l1, l2, l3 };
	uint8_t len[4], off[4];
	for (uint8_t p = 0; p < 4; p++) {
		uint8_t n = 0;
		while (lines[p][n]) n++;
		len[p] = n;
		off[p] = (uint8_t)((OLED_W - n * 6 + 1) / 2);
	}
	oled_frame_begin();
	for (uint8_t x = 0; x < OLED_W; x++) {
		uint32_t col = 0;
		for (uint8_t p = 0; p < 4; p++) {
			if (x >= off[p]) col |= (uint32_t)font_text_col(lines[p], len[p], (uint8_t)(x - off[p])) << (8 * p);
		}
		oled_column(col);
	}
	oled_frame_end();
}

typedef struct {
	int16_t  left, size;     /* screen pixels */
	uint16_t inv;            /* (8 << 8) / size: bitmap column = (x - left) * inv >> 8 */
	uint16_t depth;          /* Q8.8 */
	uint32_t area;           /* rows the sprite covers (cleared to black) */
	uint32_t colmask[8];     /* lit rows for each of the 8 bitmap columns */
} sprite_t;

static uint32_t span(int16_t y0, int16_t y1)   /* rows y0..y1-1, clipped */
{
	if (y0 < 0) y0 = 0;
	if (y1 > OLED_H) y1 = OLED_H;
	if (y1 <= y0) return 0;
	return pgm_read_dword(&lowmask[y1]) & ~pgm_read_dword(&lowmask[y0]);
}

void doom_render(void)
{
	if (title) {
		char a[22], b[22], c[22], d[22];             /* copied from flash to save RAM */
		strcpy_P(a, PSTR("DOOM MODE"));
		strcpy_P(b, PSTR("W/S MOVE  A/D TURN"));
		strcpy_P(c, PSTR("F FIRE  Q/E STRAFE"));
		strcpy_P(d, PSTR("ANY KEY TO START"));
		text_screen(a, b, c, d);
		return;
	}
	if (dead) {
		char k[13], dg[5], a[10], d[14];
		uint8_t nd = 0, i = 7;
		uint16_t n = kills;
		strcpy_P(k, PSTR("KILLS: "));
		do { dg[nd++] = (char)('0' + n % 10); n /= 10; } while (n && nd < 5);
		while (nd) k[i++] = dg[--nd];
		k[i] = '\0';
		strcpy_P(a, PSTR("YOU DIED"));
		strcpy_P(d, PSTR("F TO RESTART"));
		text_screen(a, "", k, d);
		return;
	}

	/* project the live demons, nearest last so it is drawn on top */
	sprite_t sp[N_DEMONS];
	uint8_t ns = 0;
	for (uint8_t k = 0; k < N_DEMONS; k++) {
		if (!dm[k].alive) continue;
		int16_t depth, lat;
		to_view(dm[k].x, dm[k].y, &depth, &lat);
		if (depth < 48) continue;                      /* behind or inside the player */
		sprite_t *s = &sp[ns];
		int16_t size = (int16_t)(DEMON_K / (uint16_t)depth);
		if (size > 96) size = 96;
		if (size < 2) size = 2;
		int16_t cx = (int16_t)(64 + (int32_t)lat * (int32_t)DOOM_DPX / depth);
		int16_t wall_h = (int16_t)(WALL_K / (uint16_t)depth);
		int16_t floor_y = (int16_t)(16 + wall_h / 2);  /* stand on the floor */
		int16_t top = (int16_t)(floor_y - size);
		s->size = size;
		s->left = (int16_t)(cx - size / 2);
		s->depth = (uint16_t)depth;
		if (s->left >= OLED_W || s->left + size <= 0) continue;
		s->inv  = (uint16_t)((8u << 8) / (uint16_t)size);
		s->area = span(top, (int16_t)(top + size));
		for (uint8_t u = 0; u < 8; u++) s->colmask[u] = 0;
		for (uint8_t v = 0; v < 8; v++) {
			uint32_t rows = span((int16_t)(top + (v * size) / 8), (int16_t)(top + ((v + 1) * size) / 8));
			uint8_t bits = pgm_read_byte(&demon_bmp[v]);
			for (uint8_t u = 0; u < 8; u++)
				if (bits & (0x80 >> u)) s->colmask[u] |= rows;
		}
		/* insertion sort: farthest first */
		uint8_t j = ns++;
		while (j > 0 && sp[j - 1].depth < sp[j].depth) {
			sprite_t t = sp[j]; sp[j] = sp[j - 1]; sp[j - 1] = t; j--;
		}
	}

	uint8_t hp_px = (uint8_t)((uint16_t)(hp < 0 ? 0 : hp) * OLED_W / 100);

	uint32_t prev_perp = 0;
	uint8_t  prev_side = 2;

	oled_frame_begin();
	for (uint8_t c = 0; c < DOOM_COLS; c++) {
		uint8_t side;
		int16_t ra = (int16_t)(pa + (int16_t)pgm_read_word(&col_off[c]));
		uint16_t d = cast(ra, &side);
		uint32_t perp = ((uint32_t)d * pgm_read_word(&col_cos[c])) >> 15;
		if (perp < 24) perp = 24;
		uint16_t h = (perp <= WALL_K / OLED_H) ? OLED_H
		           : (uint16_t)((uint16_t)WALL_K / (uint16_t)perp);    /* 16-bit divide */
		int16_t top = (int16_t)((OLED_H - h) / 2);
		uint32_t wallm = span(top, (int16_t)(top + h));
		uint32_t rims  = span(top, (int16_t)(top + 1)) | span((int16_t)(top + h - 1), (int16_t)(top + h));
		uint32_t below = ~span(0, (int16_t)(top + h));
		/* a black edge where the wall jumps in depth or turns a corner */
		uint32_t dp = perp > prev_perp ? perp - prev_perp : prev_perp - perp;
		uint8_t edge = (side != prev_side) || (dp > (perp >> 3));
		prev_perp = perp; prev_side = side;

		for (uint8_t half = 0; half < 2; half++) {
			uint8_t x = (uint8_t)(2 * c + half);
			uint32_t col = wallm;

			if (perp > 7 * 256) {                         /* far walls: sparse */
				col &= (x & 1) ? 0x22222222UL : 0x88888888UL;
			} else if (side) {                            /* y-facing walls: striped */
				col &= 0x55555555UL;
			} else if (perp > 4 * 256) {                  /* mid distance: dithered */
				col &= (x & 1) ? 0xAAAAAAAAUL : 0x55555555UL;
			}
			col |= wallm & rims;
			if (edge && half == 0 && c > 0) col = 0;

			/* floor: two rows of dots below the walls */
			uint32_t floor = ((x & 7) == 0 ? (1UL << 31) : 0) | ((x & 15) == 4 ? (1UL << 27) : 0);
			col |= floor & below;

			/* demons */
			for (uint8_t k = 0; k < ns; k++) {
				sprite_t *s = &sp[k];
				if (x < s->left || x >= s->left + s->size || s->depth >= perp) continue;
				uint8_t u = (uint8_t)(((uint16_t)(x - s->left) * s->inv) >> 8);
				if (u > 7) u = 7;
				col = (col & ~s->area) | s->colmask[u];
			}

			/* gun (bottom centre) and muzzle flash */
			if (x >= 56 && x < 72) {
				col = (col & 0x001FFFFFUL) | pgm_read_dword(&gun_cols[x - 56]);   /* clear rows 21-31 */
				if (flash_ms && x >= 60 && x < 68)
					col = (col & ~0x001FC000UL) | ((x & 1) ? 0x00154000UL : 0x001A8000UL);
			}

			/* crosshair */
			if (x == 64)            col ^= (1UL << 13) | (1UL << 15);
			if (x == 63 || x == 65) col ^= (1UL << 14);

			/* health bar along the top edge */
			if (x < hp_px) col ^= 1UL;

			if (hurt_ms) col = ~col;                      /* flash when hit */
			oled_column(col);
		}
	}
	oled_frame_end();
}

uint8_t doom_take_events(void)
{
	uint8_t e = events;
	events = 0;
	return e;
}

uint16_t doom_kills(void)  { return kills; }

#ifdef DOOM_TEST
uint32_t doom_test_cast_all(void)
{
	uint32_t sum = 0; uint8_t side;
	for (uint8_t c = 0; c < DOOM_COLS; c++) sum += cast((int16_t)(pa + (int16_t)pgm_read_word(&col_off[c])), &side);
	return sum;
}
void doom_test_place(uint8_t k, int16_t x, int16_t y) { dm[k].x = x; dm[k].y = y; dm[k].alive = 1; }
void doom_test_pose(int16_t x, int16_t y, int16_t a) { px = x; py = y; pa = a; }
#endif
int8_t   doom_health(void) { return hp; }

#endif /* ENABLE_DOOM */
