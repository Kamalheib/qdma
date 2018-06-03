SHELL = /bin/bash

#
# makefile parameters:
# - KDIR/KSRC/KOBJ=, optional
# - install_path=, 			override all install directories
# - kernel_install_path=,	override install directory for kernel module
# - user_install_path=,		override install directory for applications
# - conf_install_path=,		override install directory for config files
# - udev_install_path=,		override install directory for udev rules
# - docs_install_path=,		override install directory for man pages
#
# - enable_wrb_immediate_data=<0|1>	enable immediate data in writeback desc.
#

# Define grep error output to NULL, since -s is not portable.
grep = grep 2>/dev/null

# ALL subdirectories
ALLSUBDIRS := drv
DRIVER_SRC_DIR := drv

# subdirectories to be build
SUBDIRS := $(ALLSUBDIRS)

# Honor the -s (silent) make option.
verbose := $(if $(filter s,$(MAKEFLAGS)),,-v)

# Define paths.
srcdir := $(shell pwd)
topdir := $(shell cd $(srcdir)/.. && pwd)
build_dir := $(srcdir)/build

kernel_check = 1
distro_check = 1

ifeq ($(filter clean,$(MAKECMDGOALS)),clean)
  kernel_check = 0
  distro_check = 0
endif

ifeq ($(filter uninstall,$(MAKECMDGOALS)),uninstall)
  distro_check = 0
endif

ifeq ($(kernel_check),1)
  include make/kernel_check.mk

  ifeq ($(distro_check),1)
    include make/distro_check.mk
  endif
endif

ifneq ($(wildcard $(KINC)/linux/kconfig.h),)
  FLAGS += -DKERNEL_HAS_KCONFIG_H
endif
ifneq ($(wildcard $(KINC)/linux/export.h),)
  FLAGS += -DKERNEL_HAS_EXPORT_H
endif

# Debug flags.
ifeq ($(DEBUG),1)
  FLAGS += -g -DDEBUG
endif
 
ifeq ($(enable_wrb_immediate_data),1)
	FLAGS += -DXNL_IMM_DATA_EN
endif

# Don't allow ARCH to overwrite the modified variable when passed to
# the sub-makes.
MAKEOVERRIDES := $(filter-out ARCH=%,$(MAKEOVERRIDES))
# Don't allow CFLAGS/EXTRA_CFLAGS to clobber definitions in sub-make.
MAKEOVERRIDES := $(filter-out CFLAGS=%,$(MAKEOVERRIDES))
MAKEOVERRIDES := $(filter-out EXTRA_CFLAGS=%,$(MAKEOVERRIDES))

# Exports.
export grep
export srcdir
export topdir
export build_dir
export KERNELRELEASE
export KSRC
export KOBJ
export KINC
# arm64 specific fix to include <ksrc>/arch/<karch> folder properly.
# This hack is motivated by the RHEL7.X/CentOS7.X release where the
# uname Architecture is indicated as "aarch64" but the real Architecture
# source directory is "arm64"
ifeq ($(ARCH),aarch64)
  ifeq ($(wildcard $(KOBJ)/arch/$(ARCH)/Makefile),)
    override MAKECMDGOALS = $(MAKECMDGOALS) "ARCH=arm64"
  else
    export ARCH
  endif
else
  export ARCH
endif
export FLAGS
#export FLAGS += $(CFLAGS) $(EXTRA_CFLAGS) $(CPPFLAGS)
export verbose
export utsrelease
export kversions
export kseries
export modulesymfile

export enable_xvc

# evaluate install paths
ifeq ($(install_path),)
	# defaults
    kernel_install_path ?= $(PREFIX)/lib/modules/$(utsrelease)/updates/kernel/drivers/qdma
    user_install_path ?= /usr/local/sbin
    conf_install_path ?= /etc/xilinx-dma
    udev_install_path ?= /etc/udev/rules.d
    docs_install_path ?= /usr/share/man/man8
else # bundled install
	kernel_install_path ?= $(install_path)/modules
    user_install_path ?= $(install_path)/bin
    conf_install_path ?= $(install_path)/etc
    udev_install_path ?= $(install_path)/etc
    docs_install_path ?= $(install_path)/doc
endif

.PHONY: eval.mak

.PHONY: default
default: user tools mod_pf mod_vf post

.PHONY: pf 
pf: user mod_pf

.PHONY: vf 
vf: user mod_vf

.PHONY: mods
mod: mod_pf mod_vf

.PHONY: install
install: install-mods install-user install-etc

.PHONY: uninstall
uninstall: uninstall-mod uninstall-user

.PHONY: user
user:
	@echo "#######################";
	@echo "####  user        ####";
	@echo "#######################";
	$(MAKE) -C user

.PHONY: tools
tools:
	@echo "#######################";
	@echo "####  tools        ####";
	@echo "#######################";
	$(MAKE) -C tools

.PHONY: mod_pf
mod_pf:
	@if [ -n "$(verbose)" ]; then \
	   echo "#######################";\
	   printf "#### PF %-8s%5s####\n" $(DRIVER_SRC_DIR);\
	   echo "#######################";\
	 fi;
	@drvdir=$(shell pwd)/$(DRIVER_SRC_DIR) $(MAKE) VF=0 -C $(DRIVER_SRC_DIR);

.PHONY: mod_vf
mod_vf:
	@if [ -n "$(verbose)" ]; then \
	   echo "#######################";\
	   printf "#### VF %-8s%5s####\n" $(DRIVER_SRC_DIR);\
	   echo "#######################";\
	 fi;
	@drvdir=$(shell pwd)/$(DRIVER_SRC_DIR) $(MAKE) VF=1 -C $(DRIVER_SRC_DIR);

.PHONY: post
post:
	@if [ -n "$(post_msg)" ]; then \
	   echo -e "\nWARNING:\n $(post_msg)";\
	 fi;

.PHONY: clean
clean:
	@echo "#######################";
	@echo "####  user         ####";
	@echo "#######################";
	$(MAKE) -C user clean;
	@echo "#######################";
	@echo "####  tools        ####";
	@echo "#######################";
	$(MAKE) -C tools clean;
	@for dir in $(ALLSUBDIRS); do \
	   echo "#######################";\
	   printf "####  %-8s%5s####\n" $$dir;\
	   echo "#######################";\
	  drvdir=$(shell pwd)/$$dir $(MAKE) -C $$dir clean;\
	done;
	@-/bin/rm -f *.symvers eval.mak 2>/dev/null;

.PHONY: install-mods
install-mods:
	@echo "installing kernel modules to $(kernel_install_path) ..."
	@mkdir -p -m 755 $(kernel_install_path)
	@install -v -m 644 $(build_dir)/*.ko $(kernel_install_path)
	@depmod -a || true


.PHONY: install-user
install-user:
	@echo "installing user tools to $(user_install_path) ..."
	@mkdir -p -m 755 $(user_install_path)
	@install -v -m 755 build/dmactl* $(user_install_path)
	@install -v -m 755 tools/dma_from_device $(user_install_path)
	@install -v -m 755 tools/dma_to_device $(user_install_path)

.PHONY: install-etc
install-etc:
	@echo "install Xilinx DMA udev rules:"
	@mkdir -p -m 755 $(udev_install_path)
	@if [ ! -d "$(udev_install_path)/30-qdma.rules" ]; then \
		install -v -m 644 etc/30-qdma.rules $(udev_install_path); \
	fi;

.PHONY: uninstall-mod
uninstall-mod:
	@echo "Un-installing $(kernel_install_path) ..."
	@/bin/rm -rf $(kernel_install_path)/*
	@depmod -a

.PHONY: uninstall-user
uninstall-user:
	@echo "Un-installing user tools under $(user_install_path) ..."
	@/bin/rm -f $(user_install_path)/dmactl
	@/bin/rm -f $(user_install_path)/dma_from_device
	@/bin/rm -f $(user_install_path)/dma_to_device

.PHONY: help
help:
	@echo "Build Targets:";\
	 echo " install             - Installs all compiled drivers.";\
	 echo " uninstall           - Uninstalls drivers.";\
	 echo " clean               - Removes all generated files.";\
	 echo;\
	 echo "Build Options:";\
	 echo " KOBJ=<path>         - Kernel build (object) path.";\
	 echo " KSRC=<path>         - Kernel source path.";\
	 echo "                     - Note: When using KSRC or KOBJ, both";\
	 echo "                             variables must be specified.";\
	 echo " KDIR=<path>         - Kernel build and source path. Shortcut";\
	 echo "                       for KOBJ=KSRC=<path>.";\
	 echo " kernel_install_path=<path>";\
	 echo "                     - kernel module install path.";\
	 echo " user_install_path=<path>";\
	 echo "                     - user cli tool install path.";\
	 ech_;
