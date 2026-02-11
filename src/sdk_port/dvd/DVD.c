#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef uint32_t u32;
typedef int32_t s32;

// RAM-backed state (big-endian in MEM1) for dump comparability.
#include "../sdk_state.h"
#include "../gc_mem.h"

u32 gc_dvd_initialized;
u32 gc_dvd_drive_status;
u32 gc_dvd_open_calls;
u32 gc_dvd_read_calls;
u32 gc_dvd_close_calls;
u32 gc_dvd_last_read_len;
u32 gc_dvd_last_read_off;

void DVDInit(void) {
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_DVD_INITIALIZED, &gc_dvd_initialized, 1u);
    // Deterministic default for "idle" boot: no executing command block.
    // MP4 polls this during HuDvdErrorWatch.
    gc_sdk_state_store_u32_mirror(GC_SDK_OFF_DVD_DRIVE_STATUS, &gc_dvd_drive_status, 0u);
}

u32 DVDGetDriveStatus(void) {
    gc_dvd_drive_status = gc_sdk_state_load_u32_or(GC_SDK_OFF_DVD_DRIVE_STATUS, gc_dvd_drive_status);
    return gc_dvd_drive_status;
}

// Minimal DVD API surface for deterministic tests.
//
// We do not emulate the real drive/FST. Instead, tests inject a tiny "virtual
// disc" mapping from entrynum -> bytes so MP4 callsites can be exercised
// deterministically (e.g. HuDvdDataFastRead uses DVDFastOpen+DVDReadAsync).

typedef struct {
    volatile s32 state; // 0 = idle, 1 = busy
} DVDCommandBlock;

typedef struct {
    DVDCommandBlock cb;
    u32 startAddr; // token only; used by MP4 async callback matching
    u32 length;
    s32 entrynum;
} DVDFileInfo;

typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

typedef struct {
    uint8_t *data;
    u32 len;
} GcDvdTestFile;

static GcDvdTestFile g_dvd_test_files[512];

/* Minimal FST state (decomp-style). */
typedef struct {
    u32 isDirAndStringOff;
    u32 parentOrPosition;
    u32 nextEntryOrLength;
} GcFstEntry;

/* Cached-addressed MEM1 pointers (0x8xxxxxxx). */
static u32 g_fst_start_addr;
static u32 g_fst_string_start_addr;
static u32 g_fst_max_entry_num;
static u32 g_current_directory;
u32 __DVDLongFileNameFlag = 0;

// Forward decl (defined later in this file).
s32 DVDConvertPathToEntrynum(char *pathPtr);

static inline u32 dvd_load_u32be(u32 addr) {
    uint8_t *p = gc_mem_ptr(addr, 4);
    if (!p) return 0;
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static inline int dvd_load_u8(u32 addr) {
    uint8_t *p = gc_mem_ptr(addr, 1);
    if (!p) return -1;
    return (int)p[0];
}

static inline u32 fst_field(u32 entry, u32 off) {
    return dvd_load_u32be(g_fst_start_addr + entry * 12u + off);
}

static inline int fst_entry_is_dir(u32 entry) {
    return (fst_field(entry, 0) & 0xff000000u) != 0u;
}

static inline u32 fst_string_off(u32 entry) {
    return fst_field(entry, 0) & ~0xff000000u;
}

static inline u32 fst_parent_dir(u32 entry) {
    return fst_field(entry, 4);
}

static inline u32 fst_next_dir(u32 entry) {
    return fst_field(entry, 8);
}

static inline u32 fst_file_pos(u32 entry) {
    return fst_field(entry, 4);
}

static inline u32 fst_file_len(u32 entry) {
    return fst_field(entry, 8);
}

void __DVDFSInit(void) {
    /* OSBootInfo at cached 0x80000000, FSTLocation at offset 0x30 */
    u32 fst_loc = dvd_load_u32be(0x80000030u);
    g_fst_start_addr = fst_loc;
    g_fst_string_start_addr = 0;
    g_fst_max_entry_num = 0;
    g_current_directory = 0;

    if (g_fst_start_addr != 0u) {
        g_fst_max_entry_num = dvd_load_u32be(g_fst_start_addr + 8u); /* entry0.nextEntryOrLength */
        g_fst_string_start_addr = g_fst_start_addr + g_fst_max_entry_num * 12u;
    }
}

static int isSameMem(const char *path, u32 string_addr) {
    int ch;
    while ((ch = dvd_load_u8(string_addr++)) > 0) {
        if (tolower((unsigned char)*path++) != tolower((unsigned char)ch)) {
            return 0;
        }
    }

    if ((*path == '/') || (*path == '\0')) {
        return 1;
    }
    return 0;
}

static void gc_dvd_test_reset_files_internal(void) {
    for (u32 i = 0; i < (u32)(sizeof(g_dvd_test_files) / sizeof(g_dvd_test_files[0])); i++) {
        free(g_dvd_test_files[i].data);
        g_dvd_test_files[i].data = NULL;
        g_dvd_test_files[i].len = 0;
    }
}

void gc_dvd_test_reset_files(void) {
    gc_dvd_test_reset_files_internal();
}

void gc_dvd_test_set_file(s32 entrynum, const void *data, u32 len) {
    if (entrynum < 0) return;
    if ((u32)entrynum >= (u32)(sizeof(g_dvd_test_files) / sizeof(g_dvd_test_files[0]))) return;

    free(g_dvd_test_files[entrynum].data);
    g_dvd_test_files[entrynum].data = NULL;
    g_dvd_test_files[entrynum].len = 0;

    if (!data || len == 0) return;
    uint8_t *buf = (uint8_t *)malloc(len);
    if (!buf) return;
    memcpy(buf, data, len);
    g_dvd_test_files[entrynum].data = buf;
    g_dvd_test_files[entrynum].len = len;
}

int DVDGetCommandBlockStatus(DVDCommandBlock *block) {
    if (!block) return 0;
    return block->state;
}

int DVDFastOpen(s32 entrynum, DVDFileInfo *file) {
    if (!file) return 0;
    if (entrynum < 0) return 0;
    if ((u32)entrynum >= (u32)(sizeof(g_dvd_test_files) / sizeof(g_dvd_test_files[0])) &&
        (g_fst_max_entry_num == 0u || (u32)entrynum >= g_fst_max_entry_num)) return 0;

    if (g_fst_max_entry_num != 0u) {
        if ((u32)entrynum >= g_fst_max_entry_num) return 0;
        if (fst_entry_is_dir((u32)entrynum)) return 0;

        file->cb.state = 0;
        file->entrynum = entrynum;
        file->startAddr = fst_file_pos((u32)entrynum);
        file->length = fst_file_len((u32)entrynum);
        return 1;
    }

    /* fallback test backend */
    if (!g_dvd_test_files[entrynum].data) return 0;

    file->cb.state = 0;
    file->entrynum = entrynum;
    file->startAddr = (u32)entrynum; // stable token; not a real disc address
    file->length = g_dvd_test_files[entrynum].len;
    return 1;
}

int DVDOpen(const char *path, DVDFileInfo *file) {
    (void)path;
    // Deterministic test backend: use path->entrynum mapping when present.
    s32 entry = DVDConvertPathToEntrynum((char *)path);

    gc_dvd_open_calls = gc_sdk_state_load_u32_or(GC_SDK_OFF_DVD_OPEN_CALLS, gc_dvd_open_calls) + 1u;
    gc_sdk_state_store_u32be(GC_SDK_OFF_DVD_OPEN_CALLS, gc_dvd_open_calls);
    return DVDFastOpen(entry, file);
}

int DVDRead(DVDFileInfo *file, void *addr, int len, int offset) {
    if (!file || !addr) return -1;
    gc_dvd_read_calls = gc_sdk_state_load_u32_or(GC_SDK_OFF_DVD_READ_CALLS, gc_dvd_read_calls) + 1u;
    gc_sdk_state_store_u32be(GC_SDK_OFF_DVD_READ_CALLS, gc_dvd_read_calls);
    gc_dvd_last_read_len = (u32)len;
    gc_dvd_last_read_off = (u32)offset;
    gc_sdk_state_store_u32be(GC_SDK_OFF_DVD_LAST_READ_LEN, gc_dvd_last_read_len);
    gc_sdk_state_store_u32be(GC_SDK_OFF_DVD_LAST_READ_OFF, gc_dvd_last_read_off);

    // Synchronous read is implemented on top of our test file table.
    s32 entry = file->entrynum;
    if (entry < 0) return -1;
    if ((u32)entry >= (u32)(sizeof(g_dvd_test_files) / sizeof(g_dvd_test_files[0]))) return -1;
    if (!g_dvd_test_files[entry].data) return -1;
    if (offset < 0 || len < 0) return -1;

    u32 off = (u32)offset;
    u32 n = (u32)len;
    if (off > g_dvd_test_files[entry].len) return -1;
    if (off + n > g_dvd_test_files[entry].len) {
        n = g_dvd_test_files[entry].len - off;
    }
    memcpy(addr, g_dvd_test_files[entry].data + off, n);
    return (int)n;
}

// SDK signature: s32 DVDReadAsync(DVDFileInfo*, void*, s32, s32, DVDCallback)
s32 DVDReadAsync(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb) {
    if (!file || !addr) return 0;

    // Mark "busy", do the copy immediately, then mark idle.
    file->cb.state = 1;
    int n = DVDRead(file, addr, (int)len, (int)offset);
    file->cb.state = 0;

    if (cb) {
        cb((n < 0) ? (s32)-1 : (s32)n, file);
    }
    return (n < 0) ? 0 : 1;
}

s32 DVDReadPrio(DVDFileInfo *file, void *addr, s32 len, s32 offset, s32 prio) {
    (void)prio;
    return (DVDRead(file, addr, (int)len, (int)offset) < 0) ? -1 : 0;
}

s32 DVDReadAsyncPrio(DVDFileInfo *file, void *addr, s32 len, s32 offset, DVDCallback cb, s32 prio) {
    (void)prio;
    return DVDReadAsync(file, addr, len, offset, cb);
}

int DVDClose(DVDFileInfo *file) {
    (void)file;
    gc_dvd_close_calls = gc_sdk_state_load_u32_or(GC_SDK_OFF_DVD_CLOSE_CALLS, gc_dvd_close_calls) + 1u;
    gc_sdk_state_store_u32be(GC_SDK_OFF_DVD_CLOSE_CALLS, gc_dvd_close_calls);
    return 1;
}

// -----------------------------------------------------------------------------
// DVD filesystem path -> entrynum
//
// In the real SDK this walks the disc FST. We do not have a full disc model yet.
//
// For now we provide a tiny injectable map so we can write deterministic tests
// for MP4 callsites (e.g. HuDataInit validates that its data/*.bin paths exist).
// -----------------------------------------------------------------------------

static const char *g_dvd_test_paths[512];
static s32 g_dvd_test_path_count;

void gc_dvd_test_reset_paths(void) {
    for (u32 i = 0; i < (u32)g_dvd_test_path_count; i++) {
        g_dvd_test_paths[i] = 0;
    }
    g_dvd_test_path_count = 0;
}

void gc_dvd_test_set_paths(const char **paths, s32 count) {
    gc_dvd_test_reset_paths();
    if (!paths || count <= 0) {
        return;
    }
    if (count > (s32)(sizeof(g_dvd_test_paths) / sizeof(g_dvd_test_paths[0]))) {
        count = (s32)(sizeof(g_dvd_test_paths) / sizeof(g_dvd_test_paths[0]));
    }
    for (s32 i = 0; i < count; i++) {
        g_dvd_test_paths[i] = paths[i];
    }
    g_dvd_test_path_count = count;
}

// SDK signature (from decomp): s32 DVDConvertPathToEntrynum(char* pathPtr)
s32 DVDConvertPathToEntrynum(char *pathPtr) {
    if (!pathPtr) {
        return -1;
    }

    /* Preferred path: real FST walk (decomp-style). */
    if (g_fst_max_entry_num == 0u) {
        __DVDFSInit();
    }
    if (g_fst_max_entry_num != 0u) {
        const char *ptr;
        u32 dirLookAt = g_current_directory;
        const char *origPathPtr = pathPtr;
        (void)origPathPtr;

        while (1) {
            if (*pathPtr == '\0') {
                return (s32)dirLookAt;
            } else if (*pathPtr == '/') {
                dirLookAt = 0;
                pathPtr++;
                continue;
            } else if (*pathPtr == '.') {
                if (*(pathPtr + 1) == '.') {
                    if (*(pathPtr + 2) == '/') {
                        dirLookAt = fst_parent_dir(dirLookAt);
                        pathPtr += 3;
                        continue;
                    } else if (*(pathPtr + 2) == '\0') {
                        return (s32)fst_parent_dir(dirLookAt);
                    }
                } else if (*(pathPtr + 1) == '/') {
                    pathPtr += 2;
                    continue;
                } else if (*(pathPtr + 1) == '\0') {
                    return (s32)dirLookAt;
                }
            }

            if (__DVDLongFileNameFlag == 0u) {
                int extention = 0;
                int illegal = 0;
                const char *extentionStart = NULL;

                for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++) {
                    if (*ptr == '.') {
                        if ((ptr - pathPtr > 8) || extention) {
                            illegal = 1;
                            break;
                        }
                        extention = 1;
                        extentionStart = ptr + 1;
                    } else if (*ptr == ' ') {
                        illegal = 1;
                    }
                }

                if (extention && extentionStart && (ptr - extentionStart > 3)) {
                    illegal = 1;
                }

                if (illegal) {
                    /* decomp panics here; sdk_port returns not-found */
                    return -1;
                }
            } else {
                for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++) {
                    ;
                }
            }

            int isDir = (*ptr == '\0') ? 0 : 1;
            u32 length = (u32)(ptr - pathPtr);

            u32 i;
            for (i = dirLookAt + 1; i < fst_next_dir(dirLookAt);
                 i = fst_entry_is_dir(i) ? fst_next_dir(i) : (i + 1)) {
                if (!fst_entry_is_dir(i) && isDir) {
                    continue;
                }
                if (isSameMem(pathPtr, g_fst_string_start_addr + fst_string_off(i))) {
                    goto next_hier;
                }
            }
            return -1;

        next_hier:
            if (!isDir) {
                return (s32)i;
            }
            dirLookAt = i;
            pathPtr += length + 1;
        }
    }

    /* fallback test backend: path table */
    for (s32 i = 0; i < g_dvd_test_path_count; i++) {
        const char *p = g_dvd_test_paths[i];
        if (!p) continue;
        if (strcmp(p, pathPtr) == 0) return i;
    }
    return -1;
}

static u32 myStrncpyMem(char* dest, u32 src_addr, u32 maxlen) {
    u32 i = maxlen;
    while (i > 0) {
        int ch = dvd_load_u8(src_addr++);
        if (ch <= 0) break;
        *dest++ = (char)ch;
        i--;
    }
    return (maxlen - i);
}

static u32 entryToPathRec(u32 entry, char* path, u32 maxlen) {
    if (entry == 0) return 0;

    u32 loc = entryToPathRec(fst_parent_dir(entry), path, maxlen);
    if (loc == maxlen) return loc;

    path[loc++] = '/';
    loc += myStrncpyMem(path + loc, g_fst_string_start_addr + fst_string_off(entry), maxlen - loc);
    return loc;
}

static int DVDConvertEntrynumToPath(s32 entrynum, char* path, u32 maxlen) {
    if (!path || maxlen == 0) return 0;
    if (g_fst_max_entry_num == 0u) return 0;
    if (entrynum < 0 || (u32)entrynum >= g_fst_max_entry_num) return 0;

    u32 loc = entryToPathRec((u32)entrynum, path, maxlen);
    if (loc == maxlen) {
        path[maxlen - 1] = '\0';
        return 0;
    }

    if (fst_entry_is_dir((u32)entrynum)) {
        if (loc == maxlen - 1) {
            path[loc] = '\0';
            return 0;
        }
        path[loc++] = '/';
    }

    path[loc] = '\0';
    return 1;
}

int DVDGetCurrentDir(char* path, u32 maxlen) {
    if (g_fst_max_entry_num == 0u) __DVDFSInit();
    return DVDConvertEntrynumToPath((s32)g_current_directory, path, maxlen);
}

int DVDChangeDir(char* dirName) {
    if (g_fst_max_entry_num == 0u) __DVDFSInit();
    s32 entry = DVDConvertPathToEntrynum(dirName);
    if (entry < 0) return 0;
    if (!fst_entry_is_dir((u32)entry)) return 0;
    g_current_directory = (u32)entry;
    return 1;
}
