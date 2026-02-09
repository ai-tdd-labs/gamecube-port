# GXInitTexObjLOD DOL test build (Dolphin expected)
TOPDIR ?= $(CURDIR)

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

export DEVKITPPC := $(DEVKITPRO)/devkitPPC
include $(DEVKITPRO)/devkitPPC/gamecube_rules

TARGET := gx_init_tex_obj_lod_realistic_pfdrawfonts_001
BUILD := build
SOURCES := .

INCLUDE := -I$(LIBOGC_INC)
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

export LD := $(CC)
export OFILES := $(CFILES:.c=.o)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(TOPDIR)/Makefile TOPDIR=$(TOPDIR)

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
