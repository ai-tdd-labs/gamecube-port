# GameCube SDK DOL harness (builds test DOLs against decomp SDK sources)

TOPDIR := $(CURDIR)

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

export DEVKITPPC := $(DEVKITPRO)/devkitPPC
include $(DEVKITPRO)/devkitPPC/gamecube_rules

# Required vars:
#   TEST_C   = path to test C file
#   TARGET   = output name (no extension)

SDK_ROOT := $(TOPDIR)/../../decomp_mario_party_4
SDK_SRC  := $(SDK_ROOT)/src/dolphin
SDK_INC  := $(SDK_ROOT)/include

SOURCES := \
	$(SDK_SRC)/dvd \
	$(SDK_SRC)/os

INCLUDE := -I$(LIBOGC_INC) -I$(SDK_INC)
CFLAGS  = -g -O2 -Wall $(MACHDEP) $(INCLUDE)
LDFLAGS = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS    := -logc
LIBDIRS := $(LIBOGC_LIB)
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir))

ifneq ($(BUILD),$(notdir $(CURDIR)))

BUILD := build

export OUTPUT := $(TOPDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(TOPDIR)/$(dir))
export DEPSDIR := $(TOPDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CFILES += $(notdir $(TEST_C))

export LD := $(CC)
export OFILES := $(CFILES:.c=.o)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(TOPDIR)/Makefile \
		TEST_C=$(TEST_C) TARGET=$(TARGET) TOPDIR=$(TOPDIR)

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).dol $(OUTPUT).elf $(OUTPUT).dol.map

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
