# SPDX-License-Identifier: GPL-2.0
# ===========================================================================
# Kernel configuration targets
# These targets are used from top-level makefile

PHONY += xconfig gconfig menuconfig config localmodconfig localyesconfig \
	build_menuconfig build_nconfig build_gconfig build_xconfig satconfig satdvconfig

ifdef KBUILD_KCONFIG
Kconfig := $(KBUILD_KCONFIG)
else
Kconfig := Kconfig
endif

ifndef KBUILD_DEFCONFIG
KBUILD_DEFCONFIG := defconfig
endif

ifeq ($(quiet),silent_)
silent := -s
endif

# We need this, in case the user has it in its environment
unexport CONFIG_

xconfig: $(obj)/qconf
	$< $(silent) $(Kconfig)

gconfig: $(obj)/gconf
	$< $(silent) $(Kconfig)

menuconfig: $(obj)/mconf
	$< $(silent) $(Kconfig)

config: $(obj)/conf
	$< $(silent) --oldaskconfig $(Kconfig)

nconfig: $(obj)/nconf
	$< $(silent) $(Kconfig)

satconfig: $(obj)/satconfig
	$< $(silent) $(Kconfig)

satdvconfig: $(obj)/satdvconfig
	$< $(silent) $(Kconfig)

build_menuconfig: $(obj)/mconf

build_nconfig: $(obj)/nconf

build_gconfig: $(obj)/gconf

build_xconfig: $(obj)/qconf

localyesconfig localmodconfig: $(obj)/conf
	$(Q)perl $(srctree)/$(src)/streamline_config.pl --$@ $(srctree) $(Kconfig) > .tmp.config
	$(Q)if [ -f .config ]; then 					\
			cmp -s .tmp.config .config ||			\
			(mv -f .config .config.old.1;			\
			 mv -f .tmp.config .config;			\
			 $< $(silent) --oldconfig $(Kconfig);		\
			 mv -f .config.old.1 .config.old)		\
	else								\
			mv -f .tmp.config .config;			\
			$< $(silent) --oldconfig $(Kconfig);		\
	fi
	$(Q)rm -f .tmp.config

# These targets map 1:1 to the commandline options of 'conf'
#
# Note:
#  syncconfig has become an internal implementation detail and is now
#  deprecated for external use
simple-targets := oldconfig allnoconfig allyesconfig allmodconfig \
	alldefconfig randconfig listnewconfig olddefconfig syncconfig
PHONY += $(simple-targets)

$(simple-targets): $(obj)/conf
	$< $(silent) --$@ $(Kconfig)

PHONY += savedefconfig defconfig

savedefconfig: $(obj)/conf
	$< $(silent) --$@=defconfig $(Kconfig)

defconfig: $(obj)/conf
ifneq ($(wildcard $(srctree)/arch/$(SRCARCH)/configs/$(KBUILD_DEFCONFIG)),)
	@$(kecho) "*** Default configuration is based on '$(KBUILD_DEFCONFIG)'"
	$(Q)$< $(silent) --defconfig=arch/$(SRCARCH)/configs/$(KBUILD_DEFCONFIG) $(Kconfig)
else
	@$(kecho) "*** Default configuration is based on target '$(KBUILD_DEFCONFIG)'"
	$(Q)$(MAKE) -f $(srctree)/Makefile $(KBUILD_DEFCONFIG)
endif

%_defconfig: $(obj)/conf
	$(Q)$< $(silent) --defconfig=arch/$(SRCARCH)/configs/$@ $(Kconfig)

configfiles=$(wildcard $(srctree)/kernel/configs/$@ $(srctree)/arch/$(SRCARCH)/configs/$@)

%.config: $(obj)/conf
	$(if $(call configfiles),, $(error No configuration exists for this target on this architecture))
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/kconfig/merge_config.sh -m .config $(configfiles)
	$(Q)$(MAKE) -f $(srctree)/Makefile olddefconfig

PHONY += kvmconfig
kvmconfig: kvm_guest.config
	@:

PHONY += xenconfig
xenconfig: xen.config
	@:

PHONY += tinyconfig
tinyconfig:
	$(Q)$(MAKE) -f $(srctree)/Makefile allnoconfig tiny.config

# CHECK: -o cache_dir=<path> working?
PHONY += testconfig
testconfig: $(obj)/conf
	$(PYTHON3) -B -m pytest $(srctree)/$(src)/tests \
	-o cache_dir=$(abspath $(obj)/tests/.cache) \
	$(if $(findstring 1,$(KBUILD_VERBOSE)),--capture=no)
clean-dirs += tests/.cache

# Help text used by make help
help:
	@echo  '  config	  - Update current config utilising a line-oriented program'
	@echo  '  nconfig         - Update current config utilising a ncurses menu based program'
	@echo  '  menuconfig	  - Update current config utilising a menu based program'
	@echo  '  xconfig	  - Update current config utilising a Qt based front-end'
	@echo  '  gconfig	  - Update current config utilising a GTK+ based front-end'
	@echo  '  oldconfig	  - Update current config utilising a provided .config as base'
	@echo  '  satconfig	  - Update current config to a valid config'
	@echo  '  satdvconfig	  - CLI to change a symbol to a desired value'
	@echo  '  localmodconfig  - Update current config disabling modules not loaded'
	@echo  '  localyesconfig  - Update current config converting local mods to core'
	@echo  '  defconfig	  - New config with default from ARCH supplied defconfig'
	@echo  '  savedefconfig   - Save current config as ./defconfig (minimal config)'
	@echo  '  allnoconfig	  - New config where all options are answered with no'
	@echo  '  allyesconfig	  - New config where all options are accepted with yes'
	@echo  '  allmodconfig	  - New config selecting modules when possible'
	@echo  '  alldefconfig    - New config with all symbols set to default'
	@echo  '  randconfig	  - New config with random answer to all options'
	@echo  '  listnewconfig   - List new options'
	@echo  '  olddefconfig	  - Same as oldconfig but sets new symbols to their'
	@echo  '                    default value without prompting'
	@echo  '  kvmconfig	  - Enable additional options for kvm guest kernel support'
	@echo  '  xenconfig       - Enable additional options for xen dom0 and guest kernel support'
	@echo  '  tinyconfig	  - Configure the tiniest possible kernel'
	@echo  '  testconfig	  - Run Kconfig unit tests (requires python3 and pytest)'

# ===========================================================================
# object files used by all kconfig flavours
common-objs	:= confdata.o expr.o lexer.lex.o parser.tab.o preprocess.o \
		   symbol.o

$(obj)/lexer.lex.o: $(obj)/parser.tab.h
HOSTCFLAGS_lexer.lex.o	:= -I $(srctree)/$(src)
HOSTCFLAGS_parser.tab.o	:= -I $(srctree)/$(src)

# conf: Used for defconfig, oldconfig and related targets
hostprogs-y	+= conf
conf-objs	:= conf.o $(common-objs)

# nconf: Used for the nconfig target based on ncurses
hostprogs-y	+= nconf
nconf-objs	:= nconf.o nconf.gui.o $(common-objs)

HOSTLDLIBS_nconf	= $(shell . $(obj)/nconf-cfg && echo $$libs)
HOSTCFLAGS_nconf.o	= $(shell . $(obj)/nconf-cfg && echo $$cflags)
HOSTCFLAGS_nconf.gui.o	= $(shell . $(obj)/nconf-cfg && echo $$cflags)

$(obj)/nconf.o $(obj)/nconf.gui.o: $(obj)/nconf-cfg

# mconf: Used for the menuconfig target based on lxdialog
hostprogs-y	+= mconf
lxdialog	:= checklist.o inputbox.o menubox.o textbox.o util.o yesno.o
mconf-objs	:= mconf.o $(addprefix lxdialog/, $(lxdialog)) $(common-objs)

HOSTLDLIBS_mconf = $(shell . $(obj)/mconf-cfg && echo $$libs)
$(foreach f, mconf.o $(lxdialog), \
  $(eval HOSTCFLAGS_$f = $$(shell . $(obj)/mconf-cfg && echo $$$$cflags)))

$(obj)/mconf.o: $(obj)/mconf-cfg
$(addprefix $(obj)/lxdialog/, $(lxdialog)): $(obj)/mconf-cfg

# qconf: Used for the xconfig target based on Qt

### satconfig ###
hostprogs-y	+= satconfig satdvconfig
satconf-objs	:= kconfig-sat/cnf.o kconfig-sat/constraints.o kconfig-sat/fexpr.o kconfig-sat/picosat.o kconfig-sat/print.o kconfig-sat/rangefix.o kconfig-sat/satconf.o kconfig-sat/satutils.o kconfig-sat/symbol_mod.o kconfig-sat/utils.o

satconfig-objs	:= satconfig.o $(satconf-objs) $(common-objs)
satdvconfig-objs	:= satdvconfig.o $(satconf-objs) $(common-objs)

HOSTLDLIBS_satconfig	= $(shell pkg-config --libs glib-2.0)
HOSTCFLAGS_satconfig.o	= $(shell pkg-config --cflags glib-2.0)
HOSTLDLIBS_satdvconfig	= $(shell pkg-config --libs glib-2.0)
HOSTCFLAGS_satdvconfig.o	= $(shell pkg-config --cflags glib-2.0)

HOSTCFLAGS_cnf.o	= $(shell pkg-config --cflags glib-2.0)
HOSTCFLAGS_constraints.o= $(shell pkg-config --cflags glib-2.0)
HOSTCFLAGS_fexpr.o	= $(shell pkg-config --cflags glib-2.0)
HOSTCFLAGS_picosat.o	= -DTRACE -Wno-missing-prototypes -Wno-pointer-compare
HOSTCFLAGS_print.o	= $(shell pkg-config --cflags glib-2.0)
HOSTCFLAGS_rangefix.o	= $(shell pkg-config --cflags glib-2.0)
HOSTCFLAGS_satconf.o	= $(shell pkg-config --cflags glib-2.0)
HOSTCFLAGS_satutils.o	= $(shell pkg-config --cflags glib-2.0)
HOSTCFLAGS_symbol_mod.o	= $(shell pkg-config --cflags glib-2.0)
HOSTCFLAGS_utils.o	= $(shell pkg-config --cflags glib-2.0)
### satconfig end ###

hostprogs-y	+= qconf
qconf-cxxobjs	:= qconf.o conflict_resolver.o
qconf-objs	:= images.o $(common-objs) kconfig-sat/cnf.o kconfig-sat/constraints.o kconfig-sat/fexpr.o kconfig-sat/picosat.o kconfig-sat/print.o kconfig-sat/rangefix.o kconfig-sat/satconf.o kconfig-sat/satutils.o kconfig-sat/symbol_mod.o kconfig-sat/utils.o

HOSTLDLIBS_qconf	= $(shell . $(obj)/qconf-cfg && echo $$libs && pkg-config --libs glib-2.0)
HOSTLDLIBS_conflict_resolver	= $(shell . $(obj)/qconf-cfg && echo $$libs)
HOSTCXXFLAGS_qconf.o	= $(shell . $(obj)/qconf-cfg && echo $$cflags && pkg-config --cflags glib-2.0)
HOSTCXXFLAGS_conflict_resolver.o	= $(shell . $(obj)/qconf-cfg && echo $$cflags)

$(obj)/qconf.o: $(obj)/qconf-cfg $(obj)/qconf.moc

quiet_cmd_moc = MOC     $@
      cmd_moc = $(shell . $(obj)/qconf-cfg && echo $$moc) -i $< -o $@

$(obj)/%.moc: $(src)/%.h $(obj)/qconf-cfg
	$(call cmd,moc)

# gconf: Used for the gconfig target based on GTK+
hostprogs-y	+= gconf
gconf-objs	:= gconf.o images.o $(common-objs)

HOSTLDLIBS_gconf    = $(shell . $(obj)/gconf-cfg && echo $$libs)
HOSTCFLAGS_gconf.o  = $(shell . $(obj)/gconf-cfg && echo $$cflags)

$(obj)/gconf.o: $(obj)/gconf-cfg

# check if necessary packages are available, and configure build flags
filechk_conf_cfg = $(CONFIG_SHELL) $<

$(obj)/%conf-cfg: $(src)/%conf-cfg.sh FORCE
	$(call filechk,conf_cfg)

clean-files += *conf-cfg
