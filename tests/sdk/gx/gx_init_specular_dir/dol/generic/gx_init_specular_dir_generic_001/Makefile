# gx_init_specular_dir_generic_001 SDK DOL test build
TOPDIR ?= $(CURDIR)
REPO_ROOT := $(shell git -C "$(TOPDIR)" rev-parse --show-toplevel)

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

export DEVKITPPC := $(DEVKITPRO)/devkitPPC
include $(DEVKITPRO)/devkitPPC/gamecube_rules

TARGET := gx_init_specular_dir_generic_001
BUILD := build
SOURCES := .

INCLUDE := -I$(LIBOGC_INC) -I$(REPO_ROOT)
CFLAGS = -g -O2 -Wall -std=gnu99 $(MACHDEP) $(INCLUDE)
LDFLAGS = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS := -logc -lm
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
	@rm -fr $(BUILD) $(OUTPUT).dol $(OUTPUT).elf $(OUTPUT).dol.map

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
