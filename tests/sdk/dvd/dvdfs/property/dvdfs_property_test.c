/*
 * dvdfs_property_test.c — Property-style parity test for dvdfs path logic.
 *
 * Compares:
 * - oracle: derived from MP4 decomp dvdfs.c (slightly adapted to not OSPanic)
 * - port:   independent reimplementation of the same algorithm
 *
 * This is a host-only semantic parity harness. It does NOT replace Dolphin
 * expected.bin tests.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "dvdfs_oracle.h"
#include "gc_mem.h"

typedef uint8_t  u8;
typedef uint32_t u32;
typedef int32_t  s32;
typedef int      BOOL;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* --------------------------------------------------------------------------
 * Deterministic PRNG
 * -------------------------------------------------------------------------- */

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void die(const char *msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(2);
}

static void fail_case(uint32_t seed, int run_idx, int step_idx, const char *op, const char *msg) {
    fprintf(stderr, "[FAIL] seed=0x%08X run=%d step=%d op=%s %s\n", seed, run_idx, step_idx, op, msg);
}

/* --------------------------------------------------------------------------
 * Shared FST/BootInfo model for host-only property tests
 * -------------------------------------------------------------------------- */

typedef struct OSBootInfoHost_s {
    u8  _pad0[0x24];
    void *arenaLo;
    void *arenaHi;
    void *FSTLocation;
    u32  FSTMaxLength;
} OSBootInfoHost;

typedef struct FSTEntryHost_s {
    u32 isDirAndStringOff;
    u32 parentOrPosition;
    u32 nextEntryOrLength;
} FSTEntryHost;

#define entryIsDir_h(fst, i) ((((fst)[(i)].isDirAndStringOff) & 0xff000000u) == 0u ? FALSE : TRUE)
#define stringOff_h(fst, i)  ((fst)[(i)].isDirAndStringOff & ~0xff000000u)
#define parentDir_h(fst, i)  ((fst)[(i)].parentOrPosition)
#define nextDir_h(fst, i)    ((fst)[(i)].nextEntryOrLength)

/* sdk_port functions under test */
extern void __DVDFSInit(void);
extern s32 DVDConvertPathToEntrynum(char *pathPtr);
extern int DVDChangeDir(char *dirName);
extern int DVDGetCurrentDir(char *path, u32 maxlen);
extern void gc_dvd_test_reset_paths(void);

/* --------------------------------------------------------------------------
 * Test data generation
 * -------------------------------------------------------------------------- */

#define MEM_SIZE      (1u << 20) /* 1 MiB */
#define BOOT_OFF      0x0000u
#define FST_OFF       0x1000u
#define MAX_ENTRIES   512
#define MAX_STR       32768

static u8 g_mem[MEM_SIZE];
static u8 g_mem_sdk[MEM_SIZE];
static char g_str_tmp[MAX_STR];
static u32 g_parent[MAX_ENTRIES];

typedef struct {
    FSTEntryHost *fst;
    char *str;
    u32 str_len;
    u32 n_entries;
} FstBuild;

static u32 str_add(FstBuild *b, const char *s) {
    u32 off = b->str_len;
    size_t n = strlen(s) + 1;
    if (b->str_len + (u32)n >= MAX_STR) return 0;
    memcpy(b->str + b->str_len, s, n);
    b->str_len += (u32)n;
    return off;
}

static void make_name_83(char *out, uint32_t *rng, int want_illegal) {
    static const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    if (want_illegal) {
        /* Force an illegal pattern: long base or space. */
        int mode = (int)(xorshift32(rng) % 2u);
        if (mode == 0) {
            /* base > 8 */
            int n = 10 + (int)(xorshift32(rng) % 4u);
            for (int i = 0; i < n; i++) out[i] = alpha[xorshift32(rng) % (sizeof(alpha) - 1)];
            out[n] = '\0';
        } else {
            strcpy(out, "BAD NAME");
        }
        return;
    }

    int base = 1 + (int)(xorshift32(rng) % 8u);
    int ext = (xorshift32(rng) % 3u) == 0u ? 0 : (1 + (int)(xorshift32(rng) % 3u));

    int pos = 0;
    for (int i = 0; i < base; i++) out[pos++] = alpha[xorshift32(rng) % (sizeof(alpha) - 1)];
    if (ext > 0) {
        out[pos++] = '.';
        for (int i = 0; i < ext; i++) out[pos++] = alpha[xorshift32(rng) % (sizeof(alpha) - 1)];
    }
    out[pos] = '\0';
}

static int name_taken_ci(FstBuild *b, u32 parent, const char *cand) {
    for (u32 i = 1; i < b->n_entries; i++) {
        if (g_parent[i] != parent) continue;
        const char *n = b->str + stringOff_h(b->fst, i);
        const unsigned char *a = (const unsigned char *)n;
        const unsigned char *c = (const unsigned char *)cand;
        while (*a && *c) {
            if (tolower(*a) != tolower(*c)) break;
            a++; c++;
        }
        if (*a == '\0' && *c == '\0') return 1;
    }
    return 0;
}

static int build_simple_tree(uint32_t seed, FstBuild *out) {
    uint32_t rng = seed ? seed : 1;
    memset(g_mem, 0, sizeof(g_mem));

    OSBootInfoHost *bi = (OSBootInfoHost *)(g_mem + BOOT_OFF);
    FSTEntryHost *fst = (FSTEntryHost *)(g_mem + FST_OFF);

    FstBuild b;
    b.fst = fst;
    b.str = g_str_tmp;
    b.str_len = 0;
    b.n_entries = 1; /* root */
    memset(g_str_tmp, 0, MAX_STR);

    /* Root directory entry (index 0). Name is empty string at offset 0. */
    (void)str_add(&b, "");
    fst[0].isDirAndStringOff = 0x01000000u | 0u;
    fst[0].parentOrPosition = 0;
    fst[0].nextEntryOrLength = 1;
    memset(g_parent, 0, sizeof(g_parent));
    g_parent[0] = 0;

    /* Create 1-4 top dirs, each with 0-6 children (files/dirs) and maybe nested. */
    int n_top = 1 + (int)(xorshift32(&rng) % 4u);

    for (int t = 0; t < n_top; t++) {
        u32 dir_index = b.n_entries++;
        char name[32];
        for (int tries = 0; tries < 50; tries++) {
            make_name_83(name, &rng, 0);
            if (!name_taken_ci(&b, 0, name)) break;
        }
        u32 off = str_add(&b, name);
        fst[dir_index].isDirAndStringOff = 0x01000000u | off;
        fst[dir_index].parentOrPosition = 0;
        fst[dir_index].nextEntryOrLength = dir_index + 1; /* placeholder */
        g_parent[dir_index] = 0;

        int n_child = (int)(xorshift32(&rng) % 6u);
        for (int c = 0; c < n_child; c++) {
            int make_dir = ((xorshift32(&rng) % 5u) == 0u);
            u32 idx = b.n_entries++;
            if (idx >= MAX_ENTRIES - 1) break;

            for (int tries = 0; tries < 50; tries++) {
                make_name_83(name, &rng, 0);
                if (!name_taken_ci(&b, dir_index, name)) break;
            }
            off = str_add(&b, name);

            if (make_dir) {
                fst[idx].isDirAndStringOff = 0x01000000u | off;
                fst[idx].parentOrPosition = dir_index;
                fst[idx].nextEntryOrLength = idx + 1; /* placeholder */
                g_parent[idx] = dir_index;

                int n_grand = (int)(xorshift32(&rng) % 4u);
                for (int g = 0; g < n_grand; g++) {
                    u32 gidx = b.n_entries++;
                    if (gidx >= MAX_ENTRIES - 1) break;
                    for (int tries = 0; tries < 50; tries++) {
                        make_name_83(name, &rng, 0);
                        if (!name_taken_ci(&b, idx, name)) break;
                    }
                    off = str_add(&b, name);
                    fst[gidx].isDirAndStringOff = 0x00000000u | off;
                    fst[gidx].parentOrPosition = 0;
                    fst[gidx].nextEntryOrLength = 0;
                    g_parent[gidx] = idx;
                }

                fst[idx].nextEntryOrLength = b.n_entries;
            } else {
                fst[idx].isDirAndStringOff = 0x00000000u | off;
                fst[idx].parentOrPosition = 0;
                fst[idx].nextEntryOrLength = 0;
                g_parent[idx] = dir_index;
            }
        }

        fst[dir_index].nextEntryOrLength = b.n_entries;
    }

    /* Entry 0 stores max entry count in nextEntryOrLength. */
    fst[0].nextEntryOrLength = b.n_entries;

    /* Place the string table immediately after the last entry (matches real FST layout). */
    char *str_final = (char *)(fst + b.n_entries);
    if ((u8*)str_final + b.str_len + 1 >= (g_mem + MEM_SIZE)) return 1;
    memcpy(str_final, g_str_tmp, b.str_len + 1);

    bi->FSTLocation = fst;
    bi->FSTMaxLength = 0;

    out->fst = fst;
    out->str = str_final;
    out->str_len = b.str_len;
    out->n_entries = b.n_entries;
    return 0;
}

static inline void store_u32be_mem(u8 *mem, u32 off, u32 v) {
    mem[off + 0] = (u8)(v >> 24);
    mem[off + 1] = (u8)(v >> 16);
    mem[off + 2] = (u8)(v >> 8);
    mem[off + 3] = (u8)(v >> 0);
}

/* Serialize host-endian synthetic tree to big-endian MEM1 image for sdk_port. */
static int build_sdk_mem_image(const FstBuild *fb) {
    memset(g_mem_sdk, 0, sizeof(g_mem_sdk));

    const u32 fst_cached_addr = 0x80000000u + FST_OFF;
    const u32 str_off = FST_OFF + fb->n_entries * 12u;
    const u32 str_cached_addr = 0x80000000u + str_off;

    if (str_off + fb->str_len + 1 >= MEM_SIZE) return 1;

    /* OSBootInfo.FSTLocation at offset 0x30 (big-endian pointer). */
    store_u32be_mem(g_mem_sdk, 0x30, fst_cached_addr);
    store_u32be_mem(g_mem_sdk, 0x34, 0);

    /* FST entries in big-endian. */
    for (u32 i = 0; i < fb->n_entries; i++) {
        const FSTEntryHost *e = &fb->fst[i];
        const u32 base = FST_OFF + i * 12u;
        store_u32be_mem(g_mem_sdk, base + 0, e->isDirAndStringOff);
        store_u32be_mem(g_mem_sdk, base + 4, e->parentOrPosition);
        store_u32be_mem(g_mem_sdk, base + 8, e->nextEntryOrLength);
    }

    memcpy(g_mem_sdk + str_off, fb->str, fb->str_len + 1);
    (void)str_cached_addr;
    return 0;
}

/* Build canonical absolute path for an entry (dirs end with /). */
static int build_canonical_path(FSTEntryHost *fst, char *str, u32 entry, char *out, u32 maxlen) {
    u32 stack[64];
    u32 sp = 0;
    u32 e = entry;

    /* Use the synthetic parent map for files (FST's second field is file position). */
    while (e != 0 && sp < 64) {
        stack[sp++] = e;
        e = g_parent[e];
    }

    u32 pos = 0;
    out[pos++] = '/';
    for (u32 i = sp; i > 0; i--) {
        u32 idx = stack[i - 1];
        const char *name = str + stringOff_h(fst, idx);
        size_t n = strlen(name);
        if (pos + (u32)n + 2 >= maxlen) return 1;
        memcpy(out + pos, name, n);
        pos += (u32)n;

        /* Add slash between segments. For directories, also add a trailing slash. */
        if (i > 1 || entryIsDir_h(fst, entry)) {
            out[pos++] = '/';
        }
    }
    out[pos] = '\0';
    return 0;
}

/* --------------------------------------------------------------------------
 * Property runner
 * -------------------------------------------------------------------------- */

static void usage(const char *argv0) {
    fprintf(stderr,
        "usage: %s [--seed=N] [--num-runs=N] [--steps=N] [--op=L0|L1|L2|L3|L4|MIX] [-v]\n"
        "default: --num-runs=200 --steps=200\n",
        argv0);
}

static int op_enabled(const char *opt_op, const char *name, const char *alias) {
    if (!opt_op) return 1;
    return strstr(name, opt_op) != NULL || (alias && strstr(alias, opt_op) != NULL);
}

int main(int argc, char **argv) {
    uint32_t seed = 0;
    int num_runs = 200;
    int steps = 200;
    int verbose = 0;
    const char *opt_op = NULL;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--seed=", 7) == 0) {
            seed = (uint32_t)strtoul(argv[i] + 7, NULL, 0);
        } else if (strncmp(argv[i], "--num-runs=", 11) == 0) {
            num_runs = (int)strtol(argv[i] + 11, NULL, 0);
        } else if (strncmp(argv[i], "--steps=", 8) == 0) {
            steps = (int)strtol(argv[i] + 8, NULL, 0);
        } else if (strncmp(argv[i], "--op=", 5) == 0) {
            opt_op = argv[i] + 5;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    for (int run = 0; run < (seed ? 1 : num_runs); run++) {
        uint32_t s = seed ? seed : (0x9E3779B9u ^ (uint32_t)run * 0x85EBCA6Bu);
        uint32_t rng = s ? s : 1;

        FstBuild fb;
        if (build_simple_tree(s, &fb) != 0) die("tree build failed");
        if (build_sdk_mem_image(&fb) != 0) die("sdk mem image build failed");

        /* init both */
        dvdfs_oracle_init(g_mem);
        gc_mem_set(0x80000000u, MEM_SIZE, g_mem_sdk);
        gc_dvd_test_reset_paths();
        __DVDFSInit();
        (void)DVDChangeDir("/");

        /* L0: canonical sweep (entry <-> path invariants). */
        if (op_enabled(opt_op, "L0", "CANON")) {
            for (u32 e = 0; e < fb.n_entries; e++) {
                char path[512];
                if (build_canonical_path(fb.fst, fb.str, e, path, sizeof(path)) != 0) continue;

                s32 oe = dvdfs_oracle_path_to_entry(path);
                s32 pe = DVDConvertPathToEntrynum(path);
                if (oe != pe || oe != (s32)e) {
                    fail_case(s, run, (int)e, "canon_path_to_entry", path);
                    fprintf(stderr, "  entry=%u oracle=%d port=%d path='%s'\n", e, oe, pe, path);
                    return 1;
                }

                if (entryIsDir_h(fb.fst, e)) {
                    char o_out[512], p_out[512];
                    BOOL ok1 = dvdfs_oracle_entry_to_path((s32)e, o_out, sizeof(o_out));
                    BOOL ok2 = (BOOL)DVDChangeDir(path);
                    if (ok1 != ok2) {
                        fail_case(s, run, (int)e, "entry_to_path", "mismatch");
                        fprintf(stderr, "  oracle='%s' chdir_ok=%d port_chdir_ok=%d\n", o_out, ok1, ok2);
                        return 1;
                    }
                    if (!DVDGetCurrentDir(p_out, sizeof(p_out))) {
                        fail_case(s, run, (int)e, "entry_to_path", "DVDGetCurrentDir failed");
                        return 1;
                    }
                    if (strcmp(o_out, p_out) != 0) {
                        fail_case(s, run, (int)e, "entry_to_path", "path mismatch");
                        fprintf(stderr, "  oracle='%s' port='%s'\n", o_out, p_out);
                        return 1;
                    }
                }
            }
        }

        for (int step = 0; step < steps; step++) {
            uint32_t r = xorshift32(&rng);
            int op = (int)(r % 4u); /* MIX: random among L1..L4 */
            if (opt_op) {
                if (strstr("L1", opt_op) || strstr("PATH", opt_op)) op = 0;
                else if (strstr("L2", opt_op) || strstr("ENTRY", opt_op)) op = 1;
                else if (strstr("L3", opt_op) || strstr("CHDIR", opt_op)) op = 2;
                else if (strstr("L4", opt_op) || strstr("ILLEGAL", opt_op)) op = 3;
                else if (strstr("MIX", opt_op)) op = (int)(r % 4u);
                else continue; /* L0-only run */
            }

            if (op == 0) {
                /* random canonical path -> entry */
                u32 e = xorshift32(&rng) % fb.n_entries;
                char path[512];
                if (build_canonical_path(fb.fst, fb.str, e, path, sizeof(path)) != 0) continue;

                s32 oe = dvdfs_oracle_path_to_entry(path);
                s32 pe = DVDConvertPathToEntrynum(path);
                if (oe != pe) {
                    fail_case(s, run, step, "path_to_entry", "mismatch");
                    fprintf(stderr, "  path='%s' oracle=%d port=%d\n", path, oe, pe);
                    return 1;
                }
            } else if (op == 1) {
                /* entry -> path for directories */
                u32 tries = 0;
                u32 e;
                do {
                    e = xorshift32(&rng) % fb.n_entries;
                    tries++;
                } while (tries < 20 && !entryIsDir_h(fb.fst, e));
                if (!entryIsDir_h(fb.fst, e)) {
                    continue;
                }
                char o_out[256], p_out[256];
                BOOL ok1 = dvdfs_oracle_entry_to_path((s32)e, o_out, sizeof(o_out));
                BOOL ok2 = (BOOL)DVDChangeDir(o_out);
                if (ok1 != ok2) {
                    fail_case(s, run, step, "entry_to_path", "mismatch");
                    fprintf(stderr, "  entry=%u oracle_ok=%d port_ok=%d path='%s'\n", e, ok1, ok2, o_out);
                    return 1;
                }
                if (ok2) {
                    if (!DVDGetCurrentDir(p_out, sizeof(p_out))) {
                        fail_case(s, run, step, "entry_to_path", "DVDGetCurrentDir failed");
                        return 1;
                    }
                    if (strcmp(o_out, p_out) != 0) {
                        fail_case(s, run, step, "entry_to_path", "path mismatch");
                        fprintf(stderr, "  oracle='%s' port='%s'\n", o_out, p_out);
                        return 1;
                    }
                }
            } else if (op == 2) {
                /* chdir to a directory (pick a dir entry, or root) */
                u32 tries = 0;
                u32 e;
                do {
                    e = xorshift32(&rng) % fb.n_entries;
                    tries++;
                } while (tries < 20 && !entryIsDir_h(fb.fst, e));

                char path[256];
                if (build_canonical_path(fb.fst, fb.str, e, path, sizeof(path)) != 0) continue;

                BOOL ok1 = dvdfs_oracle_chdir(path);
                BOOL ok2 = (BOOL)DVDChangeDir(path);
                if (ok1 != ok2) {
                    fail_case(s, run, step, "chdir", "mismatch");
                    fprintf(stderr, "  path='%s' oracle_ok=%d port_ok=%d\n", path, ok1, ok2);
                    return 1;
                }
            } else {
                /* illegal 8.3 segment should fail when long_names==0 */
                char bad[64];
                make_name_83(bad, &rng, 1);
                s32 oe = dvdfs_oracle_path_to_entry(bad);
                s32 pe = DVDConvertPathToEntrynum(bad);
                if (oe != pe) {
                    fail_case(s, run, step, "illegal_83", "mismatch");
                    fprintf(stderr, "  path='%s' oracle=%d port=%d\n", bad, oe, pe);
                    return 1;
                }
            }
        }

        if (verbose && (run % 50) == 0) {
            fprintf(stderr, "[OK] %d/%d\n", run, seed ? 1 : num_runs);
        }
    }

    if (verbose) fprintf(stderr, "[OK] all runs\n");
    return 0;
}
