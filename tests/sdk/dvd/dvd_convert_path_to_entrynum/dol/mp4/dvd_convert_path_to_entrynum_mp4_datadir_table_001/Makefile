#---------------------------------------------------------------------------------
# GameCube test DOL Makefile
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

export DEVKITPPC := $(DEVKITPRO)/devkitPPC

include $(DEVKITPRO)/devkitPPC/gamecube_rules

TOPDIR ?= $(CURDIR)
REPO_ROOT := $(shell git -C "$(TOPDIR)" rev-parse --show-toplevel)

TARGET := dvd_convert_path_to_entrynum_mp4_datadir_table_001
BUILD		:=	build
SOURCES		:=	.
INCLUDES	:=

INCLUDE := -I$(LIBOGC_INC)
CFLAGS	= -g -O2 -Wall $(MACHDEP) $(INCLUDE)
LDFLAGS	= -g $(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS	:=	-logc
LIBDIRS	:=	$(LIBOGC_LIB)
LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir))

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))

export LD	:=	$(CC)
export OFILES	:=	$(CFILES:.c=.o)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).dol $(OUTPUT).elf $(OUTPUT).dol.map

else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
