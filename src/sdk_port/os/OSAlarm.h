/*
 * sdk_port/os/OSAlarm.h --- Host-side port of GC SDK OSAlarm.
 *
 * OSAlarm structs live in gc_mem (big-endian). Queue pointers are
 * GC addresses (u32, 0 = NULL). System time is host-side.
 *
 * Source of truth: external/mp4-decomp/src/dolphin/os/OSAlarm.c
 */
#pragma once

#include <stdint.h>

/* OSAlarm field offsets in gc_mem (matches decomp struct layout, 40 bytes) */
#define PORT_ALARM_HANDLER   0   /* u32: function pointer (non-zero = active) */
#define PORT_ALARM_TAG       4   /* u32: tag value */
#define PORT_ALARM_FIRE      8   /* s64: absolute fire time (hi at +8, lo at +12) */
#define PORT_ALARM_PREV     16   /* u32: GC addr of prev alarm (0 = NULL) */
#define PORT_ALARM_NEXT     20   /* u32: GC addr of next alarm (0 = NULL) */
#define PORT_ALARM_PERIOD   24   /* s64: period (hi at +24, lo at +28; 0 = one-shot) */
#define PORT_ALARM_START    32   /* s64: start time (hi at +32, lo at +36) */
#define PORT_ALARM_SIZE     40

/* Host-side alarm queue state (mirrors decomp's static AlarmQueue) */
typedef struct {
    uint32_t queueHead;   /* GC addr of head alarm (0 = NULL) */
    uint32_t queueTail;   /* GC addr of tail alarm (0 = NULL) */
    int64_t  systemTime;  /* __OSGetSystemTime() equivalent */
} port_OSAlarmState;

void port_OSAlarmInit(port_OSAlarmState *st);
void port_OSCreateAlarm(uint32_t alarmAddr);
void port_OSSetAlarm(port_OSAlarmState *st, uint32_t alarmAddr,
                     int64_t tick);
void port_OSSetPeriodicAlarm(port_OSAlarmState *st, uint32_t alarmAddr,
                             int64_t start, int64_t period);
void port_OSCancelAlarm(port_OSAlarmState *st, uint32_t alarmAddr);

/* Fire the head alarm if systemTime >= fire.
 * Returns GC addr of fired alarm (0 if nothing to fire).
 * Re-inserts periodic alarms automatically. */
uint32_t port_OSAlarmFireHead(port_OSAlarmState *st);
