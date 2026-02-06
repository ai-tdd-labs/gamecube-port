#ifndef _DOLPHIN_HW_REGS
#define _DOLPHIN_HW_REGS

#include <dolphin/types.h>

extern volatile u32 __DIRegs[16];

#define DI_STATUS       (0)
#define DI_COVER_STATUS (1)
#define DI_CMD_BUF_0    (2)
#define DI_CMD_BUF_1    (3)
#define DI_CMD_BUF_2    (4)
#define DI_DMA_MEM_ADDR (5)
#define DI_DMA_LENGTH   (6)
#define DI_CONTROL      (7)
#define DI_MM_BUF       (8)
#define DI_CONFIG       (9)

#endif // _DOLPHIN_HW_REGS
