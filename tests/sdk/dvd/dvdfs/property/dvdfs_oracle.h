#ifndef DVDFS_ORACLE_H
#define DVDFS_ORACLE_H

/*
 * Oracle derived from MP4 decomp: external/mp4-decomp/src/dolphin/dvd/dvdfs.c
 * This is host-only, intended for property/parity testing.
 */

#include <stdint.h>
#include <ctype.h>
#include <string.h>

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

/* Minimal OSBootInfo mirror (only fields we need). */
typedef struct OSBootInfo_s {
    u8  _pad0[0x24];
    void *arenaLo;
    void *arenaHi;
    void *FSTLocation;
    u32  FSTMaxLength;
} OSBootInfo;

/* Minimal FSTEntry mirror. */
typedef struct FSTEntry_s {
    u32 isDirAndStringOff;
    u32 parentOrPosition;
    u32 nextEntryOrLength;
} FSTEntry;

static OSBootInfo* Oracle_BootInfo;
static FSTEntry* Oracle_FstStart;
static char* Oracle_FstStringStart;
static u32 Oracle_MaxEntryNum;
static u32 Oracle_currentDirectory;
static u32 Oracle___DVDLongFileNameFlag;
static u8* Oracle_mem_base;

static void Oracle_ResetState(void) {
    Oracle_BootInfo = NULL;
    Oracle_FstStart = NULL;
    Oracle_FstStringStart = NULL;
    Oracle_MaxEntryNum = 0;
    Oracle_currentDirectory = 0;
    Oracle___DVDLongFileNameFlag = 0;
    Oracle_mem_base = NULL;
}

static void *Oracle_OSPhysicalToCached(u32 paddr) {
    /* In this host harness, \"physical\" addresses are offsets into a buffer whose base
       represents 0x80000000. */
    if (!Oracle_mem_base) return NULL;
    return (void *)(Oracle_mem_base + paddr);
}

static BOOL Oracle_isSame(const char* path, const char* string) {
    while (*string != '\0') {
        if (tolower((unsigned char)*path++) != tolower((unsigned char)*string++)) {
            return FALSE;
        }
    }

    if ((*path == '/') || (*path == '\0')) {
        return TRUE;
    }

    return FALSE;
}

#define Oracle_entryIsDir(i) ((((Oracle_FstStart[(i)].isDirAndStringOff) & 0xff000000u) == 0u) ? FALSE : TRUE)
#define Oracle_stringOff(i)  ((Oracle_FstStart[(i)].isDirAndStringOff) & ~0xff000000u)
#define Oracle_parentDir(i)  (Oracle_FstStart[(i)].parentOrPosition)
#define Oracle_nextDir(i)    (Oracle_FstStart[(i)].nextEntryOrLength)

static void Oracle___DVDFSInit(void) {
    Oracle_BootInfo = (OSBootInfo*)Oracle_OSPhysicalToCached(0);
    Oracle_FstStart = (FSTEntry*)Oracle_BootInfo->FSTLocation;

    if (Oracle_FstStart) {
        Oracle_MaxEntryNum = Oracle_FstStart[0].nextEntryOrLength;
        Oracle_FstStringStart = (char*)&(Oracle_FstStart[Oracle_MaxEntryNum]);
    }
}

static s32 Oracle_DVDConvertPathToEntrynum(char* pathPtr) {
    const char* ptr;
    char* stringPtr;
    BOOL isDir;
    u32 length;
    u32 dirLookAt;
    u32 i;
    const char* origPathPtr = pathPtr;
    const char* extentionStart;
    BOOL illegal;
    BOOL extention;

    (void)origPathPtr;
    (void)extentionStart;

    dirLookAt = Oracle_currentDirectory;

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
                    dirLookAt = Oracle_parentDir(dirLookAt);
                    pathPtr += 3;
                    continue;
                } else if (*(pathPtr + 2) == '\0') {
                    return (s32)Oracle_parentDir(dirLookAt);
                }
            } else if (*(pathPtr + 1) == '/') {
                pathPtr += 2;
                continue;
            } else if (*(pathPtr + 1) == '\0') {
                return (s32)dirLookAt;
            }
        }

        if (Oracle___DVDLongFileNameFlag == 0) {
            extention = FALSE;
            illegal = FALSE;

            for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++) {
                if (*ptr == '.') {
                    if (((u32)(ptr - pathPtr) > 8u) || (extention == TRUE)) {
                        illegal = TRUE;
                        break;
                    }
                    extention = TRUE;
                    extentionStart = ptr + 1;
                } else if (*ptr == ' ') {
                    illegal = TRUE;
                }
            }

            if ((extention == TRUE) && ((u32)(ptr - extentionStart) > 3u)) {
                illegal = TRUE;
            }

            /* In oracle harness we don't panic; treat as "not found". */
            if (illegal) {
                return -1;
            }
        } else {
            for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++) {
                ;
            }
        }

        isDir = (*ptr == '\0') ? FALSE : TRUE;
        length = (u32)(ptr - pathPtr);

        ptr = pathPtr;

        for (i = dirLookAt + 1; i < Oracle_nextDir(dirLookAt);
             i = Oracle_entryIsDir(i) ? Oracle_nextDir(i) : (i + 1)) {
            if ((Oracle_entryIsDir(i) == FALSE) && (isDir == TRUE)) {
                continue;
            }

            stringPtr = Oracle_FstStringStart + Oracle_stringOff(i);

            if (Oracle_isSame(ptr, stringPtr) == TRUE) {
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

static u32 Oracle_myStrncpy(char* dest, char* src, u32 maxlen) {
    u32 i = maxlen;

    while ((i > 0) && (*src != 0)) {
        *dest++ = *src++;
        i--;
    }

    return (maxlen - i);
}

static u32 Oracle_entryToPath(u32 entry, char* path, u32 maxlen) {
    char* name;
    u32 loc;

    if (entry == 0) {
        return 0;
    }

    name = Oracle_FstStringStart + Oracle_stringOff(entry);

    loc = Oracle_entryToPath(Oracle_parentDir(entry), path, maxlen);

    if (loc == maxlen) {
        return loc;
    }

    *(path + loc++) = '/';

    loc += Oracle_myStrncpy(path + loc, name, maxlen - loc);

    return loc;
}

static BOOL Oracle_DVDConvertEntrynumToPath(s32 entrynum, char* path, u32 maxlen) {
    u32 loc;

    loc = Oracle_entryToPath((u32)entrynum, path, maxlen);

    if (loc == maxlen) {
        path[maxlen - 1] = '\0';
        return FALSE;
    }

    if (Oracle_entryIsDir((u32)entrynum)) {
        if (loc == maxlen - 1) {
            path[loc] = '\0';
            return FALSE;
        }

        path[loc++] = '/';
    }

    path[loc] = '\0';
    return TRUE;
}

static BOOL Oracle_DVDChangeDir(char* dirName) {
    s32 entry;
    entry = Oracle_DVDConvertPathToEntrynum(dirName);
    if ((entry < 0) || (Oracle_entryIsDir((u32)entry) == FALSE)) {
        return FALSE;
    }

    Oracle_currentDirectory = (u32)entry;
    return TRUE;
}

/* Exposed API for the property test harness */
static void dvdfs_oracle_init(void *mem_base_0x80000000) {
    Oracle_ResetState();
    Oracle_mem_base = (u8*)mem_base_0x80000000;

    Oracle___DVDFSInit();
}

static s32 dvdfs_oracle_path_to_entry(char *path) {
    return Oracle_DVDConvertPathToEntrynum(path);
}

static BOOL dvdfs_oracle_entry_to_path(s32 entry, char *out, u32 maxlen) {
    return Oracle_DVDConvertEntrynumToPath(entry, out, maxlen);
}

static BOOL dvdfs_oracle_chdir(char *path) {
    return Oracle_DVDChangeDir(path);
}

#endif /* DVDFS_ORACLE_H */
