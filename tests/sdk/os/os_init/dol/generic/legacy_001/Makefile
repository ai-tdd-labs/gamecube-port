# OSInit SDK DOL test build
TOPDIR ?= $(CURDIR)
REPO_ROOT := $(shell git -C "$(TOPDIR)" rev-parse --show-toplevel)

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

export DEVKITPPC := $(DEVKITPRO)/devkitPPC
include $(DEVKITPRO)/devkitPPC/gamecube_rules

TARGET := os_init_001
BUILD := build
SOURCES := . ../../../../../decomp_mario_party_4/src/dolphin/os ../../../../../decomp_mario_party_4/src/dolphin/dvd ../../../../../decomp_mario_party_4/src/dolphin/exi ../../../../../decomp_mario_party_4/src/dolphin/si

INCLUDE := -I$(REPO_ROOT)/tools/sdk_dol/include -I$(REPO_ROOT)/decomp_mario_party_4/include -I$(LIBOGC_INC)
CFLAGS = -g -O2 -Wall -std=gnu89 $(MACHDEP) $(INCLUDE)
LDFLAGS = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS := -logc
LIBDIRS := $(LIBOGC_LIB)
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir))

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(TOPDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(TOPDIR)/$(dir))
export DEPSDIR := $(TOPDIR)/$(BUILD)

CFILES := $(notdir $(wildcard $(TOPDIR)/*.c))
# Core OS init dependencies
CFILES += OS.c OSArena.c OSAlloc.c OSContext.c OSInterrupt.c OSCache.c OSPPC.c OSReset.c OSRtc.c OSThread.c OSException.c OSError.c OSMessage.c OSMutex.c OSAlarm.c
# Minimal EXI/SI/DVD needed for OSInit
CFILES += EXIBios.c EXICommon.c
CFILES += SIBios.c
CFILES += dvd.c dvdqueue.c dvdidutils.c dvdFatal.c dvdFile.c dvdLow.c dvdfs.c dvddevice.c

export LD := $(CC)
export OFILES := $(CFILES:.c=.o)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(TOPDIR)/Makefile TOPDIR=$(TOPDIR)

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).dol $(OUTPUT).elf $(OUTPUT).dol.map

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
