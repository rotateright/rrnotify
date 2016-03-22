###############################################################################
# Configure the Makefile 
###############################################################################
KERNEL_REL          = $(shell uname -r)
KERNEL_MAJOR_VER    = $(shell echo $(KERNEL_REL) | cut -f 1 -d ".")
KERNEL_MINOR_VER    = $(shell echo $(KERNEL_REL) | cut -f 2 -d ".")
KERNEL_PATCH_VER    = $(shell echo $(KERNEL_REL) | cut -f 3 -d "." | cut -f 1 -d "-" | cut -f 1 -d "_")

KERNEL_REL_GE_2_6_9	= $(shell if [ $(KERNEL_MAJOR_VER) -ge 3 ] || ( [ $(KERNEL_MAJOR_VER) -eq 2 ] && [ $(KERNEL_MINOR_VER) -eq 6 ] && [ $(KERNEL_PATCH_VER) -ge 9 ] ); then echo "1"; fi)
KERNEL_REL_GE_2_6_18  = $(shell if [ $(KERNEL_MAJOR_VER) -ge 3 ] || ( [ $(KERNEL_MAJOR_VER) -eq 2 ] && [ $(KERNEL_MINOR_VER) -eq 6 ] && [ $(KERNEL_PATCH_VER) -ge 18 ] ); then echo "1"; fi)
KERNEL_REL_GE_2_6_19  = $(shell if [ $(KERNEL_MAJOR_VER) -ge 3 ] || ( [ $(KERNEL_MAJOR_VER) -eq 2 ] && [ $(KERNEL_MINOR_VER) -eq 6 ] && [ $(KERNEL_PATCH_VER) -ge 19 ] ); then echo "1"; fi)

VENDOR_REDHAT       = $(shell if [ -f /etc/redhat-release ]; then echo "1" ; fi)
VENDOR_SUSE         = $(shell if [ -f /etc/SuSE-release ]; then echo "1" ; fi)
VENDOR_DEBIAN       = $(shell if [ -f /etc/debian_version ] ; then echo "1"; fi)
VENDOR_GENTOO       = $(shell if [ -f /etc/gentoo-release ]; then echo "1"; fi)
VENDOR_ARCH         = $(shell if [ -f /etc/arch-release ]; then echo "1"; fi)

ifeq ($(VENDOR_DEBIAN),1)
	ARCH = $(shell uname -m | sed -e "s/i.86/i386/g" )
else
	ifeq ($(VENDOR_GENTOO),1)
		ARCH = $(shell uname -m)
	else
		ifeq ($(VENDOR_ARCH),1)
			ARCH = $(shell uname -m)
		else
			ARCH = $(shell uname -i)
		endif
	endif
endif

# fixup 32-bit x86 variants
ifeq ($(ARCH),i486)
	ARCH = i386
else
	ifeq ($(ARCH),i586)
		ARCH = i386
	else
		ifeq ($(ARCH),i686)
			ARCH = i386
		endif
	endif
endif


EXTRA_CFLAGS        += -DRRNOTIFY
MKDIR               = mkdir -p
INSTALL_FILE        = cp -fp
CHK_DIR_EXISTS      = test -d
INST_DIR            = /lib/modules/$(KERNEL_REL)/extra
KERNEL_SOURCE       = /lib/modules/$(KERNEL_REL)/build
ARCH_VALID          = 0

###############################################################################
# Driver (Generic Code) 
###############################################################################

ifeq ($(KERNEL_REL_GE_2_6_19),1)
EXTRA_CFLAGS += -DHAS_IPRIVATE
else
ifeq ($(KERNEL_REL_GE_2_6_18),1)
ifeq ($(VENDOR_REDHAT),1)
EXTRA_CFLAGS += -DHAS_IPRIVATE
endif
endif
endif

###############################################################################
# Makefile Targets
###############################################################################

ifneq ($(KERNEL_REL_GE_2_6_9), 1)
all install default clean:
	$(error "Unsupported kernel version $(KERNEL_MAJOR_VER).$(KERNEL_MINOR_VER).$(KERNEL_PATCH_VER) ($(KERNEL_REL)). Please contact RotateRight for support options.");
else

obj-m += rrnotify.o

RRNOTIFY-y := rrnotify_init.o \
	rrnotifyfs.o rrnotify_stats.o \
	buffer_sync.o event_buffer.o

rrnotify-y := $(RRNOTIFY-y)

all:
	make -C $(KERNEL_SOURCE) SUBDIRS=`pwd` modules

default:
	make -C $(KERNEL_SOURCE) SUBDIRS=`pwd` modules

install:
	$(CHK_DIR_EXISTS) "$(DESTDIR)$(INST_DIR)" || \
		$(MKDIR) "$(DESTDIR)$(INST_DIR)"
	$(INSTALL_FILE) rrnotify.ko $(DESTDIR)$(INST_DIR)

clean:
	rm -f *.o *.ko .*.cmd *.mod.c 
	rm -fr .tmp_versions
	rm -f Module.symvers
	
endif
