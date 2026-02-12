/*
 * sdk_port/os/OSAlarm.c --- Host-side port of GC SDK OSAlarm.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/os/OSAlarm.c
 *
 * Same logic as decomp InsertAlarm / OSSetAlarm / OSSetPeriodicAlarm /
 * OSCancelAlarm / DecrementerExceptionCallback (modeled as FireHead).
 * Alarm structs live in gc_mem (big-endian). Hardware interaction
 * (SetTimer, interrupts, PPCMtdec) is stripped.
 */
#include <stdint.h>
#include "OSAlarm.h"
#include "../gc_mem.h"

/* ── Big-endian u32 helpers ── */

static inline uint32_t load_u32be(uint32_t addr)
{
    uint8_t *p = gc_mem_ptr(addr, 4);
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static inline void store_u32be(uint32_t addr, uint32_t val)
{
    uint8_t *p = gc_mem_ptr(addr, 4);
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >> 8);
    p[3] = (uint8_t)(val);
}

/* ── Big-endian s64 helpers ── */

static inline int64_t load_s64be(uint32_t addr)
{
    int32_t hi = (int32_t)load_u32be(addr);
    uint32_t lo = load_u32be(addr + 4);
    return ((int64_t)hi << 32) | (uint64_t)lo;
}

static inline void store_s64be(uint32_t addr, int64_t val)
{
    store_u32be(addr, (uint32_t)(val >> 32));
    store_u32be(addr + 4, (uint32_t)(val & 0xFFFFFFFFu));
}

/* ── Init ── */

void port_OSAlarmInit(port_OSAlarmState *st)
{
    st->queueHead = 0;
    st->queueTail = 0;
    st->systemTime = 0;
}

/* ── OSCreateAlarm (OSAlarm.c:26) ── */

void port_OSCreateAlarm(uint32_t alarmAddr)
{
    store_u32be(alarmAddr + PORT_ALARM_HANDLER, 0);
}

/* ── InsertAlarm (OSAlarm.c:40-82) ── */

static void InsertAlarm(port_OSAlarmState *st, uint32_t alarm,
                        int64_t fire, uint32_t handler)
{
    uint32_t next_a;
    uint32_t prev_a;

    if (0 < load_s64be(alarm + PORT_ALARM_PERIOD)) {
        int64_t time = st->systemTime;
        int64_t start = load_s64be(alarm + PORT_ALARM_START);
        int64_t period = load_s64be(alarm + PORT_ALARM_PERIOD);
        fire = start;
        if (start < time) {
            fire += period * ((time - start) / period + 1);
        }
    }

    store_u32be(alarm + PORT_ALARM_HANDLER, handler);
    store_s64be(alarm + PORT_ALARM_FIRE, fire);

    for (next_a = st->queueHead; next_a != 0;
         next_a = load_u32be(next_a + PORT_ALARM_NEXT)) {
        int64_t next_fire = load_s64be(next_a + PORT_ALARM_FIRE);
        if (next_fire <= fire) {
            continue;
        }
        /* Insert before next_a */
        prev_a = load_u32be(next_a + PORT_ALARM_PREV);
        store_u32be(alarm + PORT_ALARM_PREV, prev_a);
        store_u32be(next_a + PORT_ALARM_PREV, alarm);
        store_u32be(alarm + PORT_ALARM_NEXT, next_a);
        if (prev_a) {
            store_u32be(prev_a + PORT_ALARM_NEXT, alarm);
        } else {
            st->queueHead = alarm;
        }
        return;
    }

    /* Insert at tail */
    store_u32be(alarm + PORT_ALARM_NEXT, 0);
    prev_a = st->queueTail;
    st->queueTail = alarm;
    store_u32be(alarm + PORT_ALARM_PREV, prev_a);
    if (prev_a) {
        store_u32be(prev_a + PORT_ALARM_NEXT, alarm);
    } else {
        st->queueHead = st->queueTail = alarm;
    }
}

/* ── OSSetAlarm (OSAlarm.c:85-91) ── */

void port_OSSetAlarm(port_OSAlarmState *st, uint32_t alarmAddr, int64_t tick)
{
    store_s64be(alarmAddr + PORT_ALARM_PERIOD, 0);
    InsertAlarm(st, alarmAddr, st->systemTime + tick, 1);
}

/* ── OSSetPeriodicAlarm (OSAlarm.c:93-100) ── */

void port_OSSetPeriodicAlarm(port_OSAlarmState *st, uint32_t alarmAddr,
                             int64_t start, int64_t period)
{
    store_s64be(alarmAddr + PORT_ALARM_PERIOD, period);
    store_s64be(alarmAddr + PORT_ALARM_START, start);
    InsertAlarm(st, alarmAddr, 0, 1);
}

/* ── OSCancelAlarm (OSAlarm.c:102-124) ── */

void port_OSCancelAlarm(port_OSAlarmState *st, uint32_t alarmAddr)
{
    uint32_t next_a;

    if (load_u32be(alarmAddr + PORT_ALARM_HANDLER) == 0) return;

    next_a = load_u32be(alarmAddr + PORT_ALARM_NEXT);
    if (next_a == 0) {
        st->queueTail = load_u32be(alarmAddr + PORT_ALARM_PREV);
    } else {
        store_u32be(next_a + PORT_ALARM_PREV,
                    load_u32be(alarmAddr + PORT_ALARM_PREV));
    }
    if (load_u32be(alarmAddr + PORT_ALARM_PREV)) {
        uint32_t prev_a = load_u32be(alarmAddr + PORT_ALARM_PREV);
        store_u32be(prev_a + PORT_ALARM_NEXT, next_a);
    } else {
        st->queueHead = next_a;
    }
    store_u32be(alarmAddr + PORT_ALARM_HANDLER, 0);
}

/* ── DecrementerExceptionCallback / FireHead (OSAlarm.c:127-167) ── */

uint32_t port_OSAlarmFireHead(port_OSAlarmState *st)
{
    uint32_t alarm = st->queueHead;
    uint32_t next_a;
    uint32_t handler;

    if (alarm == 0) return 0;
    if (st->systemTime < load_s64be(alarm + PORT_ALARM_FIRE)) return 0;

    next_a = load_u32be(alarm + PORT_ALARM_NEXT);
    st->queueHead = next_a;
    if (next_a == 0) {
        st->queueTail = 0;
    } else {
        store_u32be(next_a + PORT_ALARM_PREV, 0);
    }

    handler = load_u32be(alarm + PORT_ALARM_HANDLER);
    store_u32be(alarm + PORT_ALARM_HANDLER, 0);
    if (0 < load_s64be(alarm + PORT_ALARM_PERIOD)) {
        InsertAlarm(st, alarm, 0, handler);
    }

    return alarm;
}
