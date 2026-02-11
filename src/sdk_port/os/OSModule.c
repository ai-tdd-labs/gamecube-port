#include "dolphin/os.h"

// Minimal module linker state used by MP4 objdll path.
// This is intentionally small: enough queue/link bookkeeping for deterministic
// host/runtime progression; relocation semantics are handled separately.

OSModuleQueue __OSModuleInfoList = {0};
const void *__OSStringTable = 0;

__attribute__((weak)) void OSNotifyLink(void) {}
__attribute__((weak)) void OSNotifyUnlink(void) {}

void OSSetStringTable(const void *stringTable) {
    __OSStringTable = stringTable;
}

BOOL OSLink(OSModuleInfo *newModule, void *bss) {
    (void)bss;
    if (!newModule) {
        return FALSE;
    }

    // If already linked as tail item, keep behavior idempotent for host flow.
    if (__OSModuleInfoList.head == newModule || newModule->link.prev || newModule->link.next) {
        return TRUE;
    }

    newModule->link.prev = __OSModuleInfoList.tail;
    newModule->link.next = 0;
    if (__OSModuleInfoList.tail) {
        __OSModuleInfoList.tail->link.next = newModule;
    } else {
        __OSModuleInfoList.head = newModule;
    }
    __OSModuleInfoList.tail = newModule;
    OSNotifyLink();
    return TRUE;
}

BOOL OSUnlink(OSModuleInfo *oldModule) {
    if (!oldModule) {
        return FALSE;
    }

    // Treat unlink of unknown node as failure.
    if (__OSModuleInfoList.head != oldModule && !oldModule->link.prev && !oldModule->link.next) {
        return FALSE;
    }

    if (oldModule->link.prev) {
        oldModule->link.prev->link.next = oldModule->link.next;
    } else {
        __OSModuleInfoList.head = oldModule->link.next;
    }

    if (oldModule->link.next) {
        oldModule->link.next->link.prev = oldModule->link.prev;
    } else {
        __OSModuleInfoList.tail = oldModule->link.prev;
    }

    oldModule->link.prev = 0;
    oldModule->link.next = 0;
    OSNotifyUnlink();
    return TRUE;
}
