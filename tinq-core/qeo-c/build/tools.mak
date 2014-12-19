# Read in the configuration of the target architecture (E defines
# the target architecture).
##<

# We reset the variables typically defined in the .env file. For some
# variables, to prevent variables from other environments to persist after the
# include of the .env file
##<
TARG_ARCH              :=
TARG_OS                :=
E_SHARED_LIB_CFLAGS    :=
E_SHARED_DIR_INFIX     :=
E_STD_LIBS             :=
E_ENDIAN               :=
E_SIZEOF_LONG          :=
TARG_ARCH_RESOLVED     :=

E_CC_REQUIRED_OPTIONS  :=
E_CCC_REQUIRED_OPTIONS :=

COMPILER             :=
CONFIGURATION_NAME   :=
TOOLCHAIN_PREFIX :=
TOOLCHAIN_DIR :=
SUPPORTED_HOSTTYPES  :=

E_CFLAGS           :=
E_CCFLAGS          :=
E_LFLAGS           :=

GDBSERVER            :=
GDBSERVER_LIBS       :=

E_KERNEL_DIR                := 
E_KERNEL_BUILD_EXTRA_PARAMS :=

prefix                  :=
bindir                  :=
libdir                  :=
datadir                 :=
pkgconfigdir            :=
incdir                  :=
sysconfdir              :=
rgcdir                  :=
viewmapdir              :=
clifwmapdir             :=
luamoddir               :=
lualibdir               :=

##>
ifdef EDIR
ifneq ($(wildcard $(EDIR)/$(E).env),)
include $(EDIR)/$(E).env
else
include $(projectdir/)build/supported_targets/$(E).env
endif
else
include $(projectdir/)build/supported_targets/$(E).env
endif

# In the case where we are working on host (i.e. not cross-compiling), we can
# fill in some of the variables by quering the properties of the system.  We do
# this by building a small program that outputs a Makefile the sets E_ENDIAN,
# ... to $(OUTPUT_PREFIX)/Makefile_machdef
#
# Query host:
##<

ifeq ($(TARG_ARCH), HOST)
ifeq ($(TARG_ARCH_RESOLVED),)
TARG_ARCH_RESOLVED:=$(HOSTMACHINE_CAPS)
endif
MACHDEF_PREFIX:=$(OUTPUT_PREFIX)

sinclude $(MACHDEF_PREFIX)/Makefile_machdef

ifeq ($(E_ENDIAN),)
ifeq ($(origin $(HOSTTYPE)_TOOLCHAIN),undefined)
ifneq ($(origin TOOLCHAIN),undefined)
$(HOSTTYPE)_TOOLCHAIN:=$(TOOLCHAIN)
endif
endif
$(shell $(mkdir) -p $(MACHDEF_PREFIX))
$(shell $($(HOSTTYPE)_TOOLCHAIN)gcc $(projectdir/)build/machdef.c -o $(MACHDEF_PREFIX)/machdef)
$(shell $(MACHDEF_PREFIX)/machdef > $(MACHDEF_PREFIX)/Makefile_machdef)
$(shell $(rm) -f $(MACHDEF_PREFIX)/machdef)
include $(MACHDEF_PREFIX)/Makefile_machdef
endif
else
ifeq ($(TARG_ARCH_RESOLVED),)
TARG_ARCH_RESOLVED:=$(TARG_ARCH)
endif
endif
##>

# Check if the required .env variables have been set
##<

# E_SHARED_LIB_CFLAGS can be empty, no need to check
# E_SHARED_LIB_INFIX can be empty, no need to check

ifeq ($(TARG_ARCH),)
$(error 'TARG_ARCH not set in $(E).env!')
$(shell echo 'TARG_ARCH not set in $(E).env!' >&2)
die
endif

ifeq ($(TARG_OS),)
$(error 'TARG_OS not set in $(E).env!')
$(shell echo 'TARG_OS not set in $(E).env!' >&2)
die
endif

ifeq ($(E_ENDIAN),)
$(error 'E_ENDIAN not set in $(E).env!')
$(shell echo 'E_ENDIAN not set in $(E).env!' >&2)
die
endif

ifeq ($(E_SIZEOF_LONG),)
$(error 'E_SIZEOF_LONG not set in $(E).env!')
$(shell echo 'E_SIZEOF_LONG not set in $(E).env!' >&2)
die
endif
##> 

$(E)_SHARED_LIB_CFLAGS         := $(E_SHARED_LIB_CFLAGS)
$(E)_SHARED_DIR_INFIX          := $(E_SHARED_DIR_INFIX)
$(E)_TARG_ARCH                 := $(TARG_ARCH)
$(E)_TARG_OS                   := $(TARG_OS)
$(E)_ENDIAN                    := $(E_ENDIAN)
$(E)_SIZEOF_LONG               := $(E_SIZEOF_LONG)
$(E)_KERNEL_DIR                := $(E_KERNEL_DIR)
$(E)_KERNEL_BUILD_EXTRA_PARAMS := $(E_KERNEL_BUILD_EXTRA_PARAMS)
$(E)_GCC_VERSION               := $(GCC_VERSION)
##>

# Utility defines for make programs:

# Multiple target rules do not specify whether all targets are generated at
# once by calling the build command or the build command has to be called
# seperately for each target. This does not pose problems in a normal
# sequential build, where make will check before a build command whether the
# target file already exists. However, in a distributed build, the build
# command might eagerly schedule multiple of these target on different queues.
# This can result in the same files being generated multiple times on different
# machines. To prevent this, clearmake borrows the concept of target groups
# from sun makefile syntax: add a + between targets to mark them as generated
# as a group. 
#
# See the IBM technote "About rules with multiple targets and parallel builds"
# for details.
# 
# We define the variable clearmakegroup here to be used for that reason. It is
# set to + in the case of clearmake, empty otherwise
ifeq ($(patsubst %clearmake,clearmake,$(MAKE)),clearmake)
clearmakegroup=+
MULTI_TARGET=$(subst $(space), $(clearmakegroup) ,$(sort $(MULTI_TARGET_FILES) $(MULTI_TARGET_DEPUTY)))
MULTI_TARGET_SETUP=
else
MULTI_TARGET=$(MULTI_TARGET_DEPUTY)
MULTI_TARGET_SETUP=$(MULTI_TARGET_FILES): $(MULTI_TARGET_DEPUTY)
endif

# GNU Shell commmands
CAT=$(GNU_SHELL_CMD)cat
CP=$(GNU_SHELL_CMD)cp
DATE=$(GNU_SHELL_CMD)date
GMAKE=$(GNU_SHELL_CMD)make
ID=$(GNU_SHELL_CMD)id
INSTALL=$(GNU_SHELL_CMD)install
INSTALL-D=$(GNU_SHELL_CMD)install -D
LN=$(GNU_SHELL_CMD)ln
LS=$(GNU_SHELL_CMD)ls
MV=$(GNU_SHELL_CMD)mv
PERL=$(GNU_SHELL_CMD)perl
PWD=$(GNU_SHELL_CMD)pwd
SED=$(GNU_SHELL_CMD)sed
SORT=$(GNU_SHELL_CMD)sort
TAIL=$(GNU_SHELL_CMD)tail
TAR=$(GNU_SHELL_CMD)tar
TEST=$(GNU_SHELL_CMD)test
TOUCH=$(GNU_SHELL_CMD)touch
TR=$(GNU_SHELL_CMD)tr
UNAME=$(GNU_SHELL_CMD)uname
WC=$(GNU_SHELL_CMD)wc


ifeq ($(PYTHON),)
export PYTHON:=$(PYTHON_PREFIX/)python
endif

ifeq ($(GNUMAKE),YES)
GNU_MAKE:=$(MAKE)
else
ifneq ($(GNUMAKE),)
GNU_MAKE:=$(GNUMAKE)
endif
endif

ifeq ($(GNU_MAKE),)
PRELOAD:=
GNU_MAKE:=$(GMAKE)
endif

ifeq ($(origin $(HOSTTYPE)_TOOLCHAIN),undefined)
ifneq ($(origin TOOLCHAIN),undefined)
$(HOSTTYPE)_TOOLCHAIN:=$(TOOLCHAIN)
endif
endif

ifeq ($(origin $(HOSTTYPE)_TOOLCHAIN),undefined)
$(error Did not find a toolchain for $(HOSTTYPE). Please add it in the $(E).env file)
# Clearmake has no $(error) so we work around that...
$(shell echo Did not find a toolchain for $(HOSTTYPE). Please add it in the $(E).env file >&2)
die
endif

$(E)_CC                           := $($(HOSTTYPE)_TOOLCHAIN)gcc

$(COMPONENT)_$(E)_CC              := $($(HOSTTYPE)_TOOLCHAIN)gcc $(E_CC_REQUIRED_OPTIONS)
$(COMPONENT)_$(E)_LD              := $($(HOSTTYPE)_TOOLCHAIN)ld $(E_LD_REQUIRED_OPTIONS)
$(COMPONENT)_$(E)_CCC             := $($(HOSTTYPE)_TOOLCHAIN)g++ $(E_CCC_REQUIRED_OPTIONS)
ifneq ($(LINKER),)
$(COMPONENT)_$(E)_LINKER          := $(LINKER)
LINKER:=
else
$(COMPONENT)_$(E)_LINKER          := $($(HOSTTYPE)_TOOLCHAIN)gcc $(E_CC_REQUIRED_OPTIONS)
endif
$(COMPONENT)_$(E)_PART_LINKER     := $($(HOSTTYPE)_TOOLCHAIN)ld -r $(E_LD_REQUIRED_OPTIONS)
$(COMPONENT)_$(E)_LIBRARIAN       := $($(HOSTTYPE)_TOOLCHAIN)ar $(E_AR_REQUIRED_OPTIONS)
$(COMPONENT)_$(E)_RANLIB          := $($(HOSTTYPE)_TOOLCHAIN)ranlib $(E_RANLIB_REQUIRED_OPTIONS)
$(COMPONENT)_$(E)_STRIP           := $($(HOSTTYPE)_TOOLCHAIN)strip $(E_STRIP_REQUIRED_OPTIONS)
$(COMPONENT)_$(E)_OBJCOPY         := $($(HOSTTYPE)_TOOLCHAIN)objcopy $(E_OBJCOPY_REQUIRED_OPTIONS)
$(COMPONENT)_$(E)_NM              := $($(HOSTTYPE)_TOOLCHAIN)nm $(E_NM_REQUIRED_OPTIONS)

ifneq ($(E_STD_LIBS),)
$(COMPONENT)_$(E)_STD_LIBS_PARAM  := $(E_STD_LIBS)
else
$(COMPONENT)_$(E)_STD_LIBS_PARAM  := -lpthread -lc
endif

$(COMPONENT)_$(E)_STD_BEGIN_LIB     := $(E_STD_BEGIN_LIB)
$(COMPONENT)_$(E)_STD_BEGIN_PROGRAM := $(E_STD_BEGIN_PROGRAM)

# GDB stuff
ifneq ($(GDB),)
$(COMPONENT)_$(E)_GDB          := $(GDB)
GDB:=
else
$(COMPONENT)_$(E)_GDB          := $($(HOSTTYPE)_TOOLCHAIN)gdb
endif


$(COMPONENT)_$(E)_GDBSERVER      := $(GDBSERVER)
$(COMPONENT)_$(E)_GDBSERVER_LIBS := $(GDBSERVER_LIBS)

XFLAGS := -DCOMPILER_$(COMPILER)= \
          -DTARG_ARCH_$(TARG_ARCH)= \
          -DTARG_OS_$(TARG_OS)= \
	  -DCOMPONENT_$(subst -,_,$(COMPONENT))

ifeq ($($(COMPONENT)_DEBUG),1)
XFLAGS += -DDEBUG
endif

$(COMPONENT)_$(E)_TMP_CFLAGS  := $(strip $(E_CFLAGS)  $(XFLAGS))
$(COMPONENT)_$(E)_TMP_CCFLAGS := $(strip $(E_CCFLAGS) $(XFLAGS))
$(COMPONENT)_$(E)_TMP_LFLAGS  := $(strip $(E_LFLAGS))

ifeq ($($(COMPONENT)_COVERAGE),1)
$(COMPONENT)_$(E)_TMP_CFLAGS_NO_COVERAGE  := $(strip $(E_CFLAGS_NO_COVERAGE)  $(XFLAGS))
$(COMPONENT)_$(E)_TMP_CCFLAGS_NO_COVERAGE := $(strip $(E_CCFLAGS_NO_COVERAGE) $(XFLAGS))
endif

$(COMPONENT)_$(E)_CONFIGURATION_NAME      := $(CONFIGURATION_NAME)
# vim: foldmethod=marker foldmarker=##<,##> :
