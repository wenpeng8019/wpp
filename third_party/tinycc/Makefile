# --------------------------------------------------------------------------
#
# Tiny C Compiler Makefile - Separated Build System
# Build compiler and runtime libraries separately
#

ifndef TOP
 TOP = .
 INCLUDED = no
endif

ifeq ($(findstring $(MAKECMDGOALS),clean distclean),)
 include $(TOP)/config.mak
endif

ifeq (-$(GCC_MAJOR)-$(findstring $(GCC_MINOR),56789)-,-4--)
 CFLAGS += -D_FORTIFY_SOURCE=0
endif

# Build directory structure
BUILD_DIR = $(TOP)/build
COMPILER_DIR = $(BUILD_DIR)/compiler
RUNTIME_DIR = $(BUILD_DIR)/runtime
TEMP_DIR = $(BUILD_DIR)/temp

# Library configuration
LIBTCC = libtcc.a
LIBTCC1 = libtcc1.a
LINK_LIBTCC =
LIBS =
CFLAGS += $(CPPFLAGS)
VPATH = $(TOPSRC)
-LTCC = $(COMPILER_DIR)/$(LIBTCC)

ifdef CONFIG_WIN32
 CFG = -win
 ifneq ($(CONFIG_static),yes)
  LIBTCC = libtcc$(DLLSUF)
  LIBTCCDEF = libtcc.def
 endif
 ifneq ($(CONFIG_debug),yes)
  LDFLAGS += -s
 endif
 NATIVE_TARGET = $(ARCH)-win$(if $(findstring arm,$(ARCH)),ce,32)
else
 CFG = -unx
 LIBS+=-lm
 ifneq ($(CONFIG_ldl),no)
  LIBS+=-ldl
 endif
 ifneq ($(CONFIG_pthread),no)
  LIBS+=-lpthread
 endif
 # make libtcc as static or dynamic library?
 ifeq ($(CONFIG_static),no)
  LIBTCC=libtcc$(DLLSUF)
  export LD_LIBRARY_PATH := $(CURDIR)/$(COMPILER_DIR)
  ifneq ($(CONFIG_rpath),no)
   LINK_LIBTCC += -Wl,-rpath,"$(libdir)"
  endif
 endif
 NATIVE_TARGET = $(ARCH)$(if $(wildcard /lib/ld-linux-x32.so.2),-x32)
endif

# target specific object rule
TARGETOS_arm-wince = -DTARGETOS_WIN32
TARGETOS_i386-win32 = $(TARGETOS_arm-wince)
TARGETOS_x86_64-win32 = $(TARGETOS_arm-wince)

# cross compiler prefixes
TCC_X = i386 x86_64 i386-win32 x86_64-win32 x86_64-osx arm arm64 arm-wince c67
TCC_X += riscv64 arm64-osx

LIBTCC1_X = $(filter-out c67,$(TCC_X))

# Object files with proper build directory paths
LIBTCC_OBJ = $(TEMP_DIR)/libtcc.o
TCC_OBJ = $(TEMP_DIR)/tcc.o $(TEMP_DIR)/tcctools.o

# Core object files for all platforms
CORE_OBJS = $(TEMP_DIR)/tccpp.o $(TEMP_DIR)/tccgen.o $(TEMP_DIR)/tccdbg.o $(TEMP_DIR)/tccelf.o \
           $(TEMP_DIR)/tccasm.o $(TEMP_DIR)/tccrun.o

# Platform-specific object files
ifeq ($(CONFIG_OSX),yes)
 CORE_OBJS += $(TEMP_DIR)/tccmacho.o
endif

ifdef CONFIG_WIN32
 CORE_OBJS += $(TEMP_DIR)/tccpe.o
endif

# Target architecture specific files  
ifeq ($(ARCH),arm64)
 CORE_OBJS += $(TEMP_DIR)/arm64-gen.o $(TEMP_DIR)/arm64-link.o $(TEMP_DIR)/arm64-asm.o
else ifeq ($(ARCH),x86_64)
 CORE_OBJS += $(TEMP_DIR)/x86_64-gen.o $(TEMP_DIR)/x86_64-link.o $(TEMP_DIR)/i386-asm.o
else ifeq ($(ARCH),i386)  
 CORE_OBJS += $(TEMP_DIR)/i386-gen.o $(TEMP_DIR)/i386-link.o $(TEMP_DIR)/i386-asm.o
endif

PROGS = $(COMPILER_DIR)/tcc$(EXESUF)
TCCLIBS = $(if $(LIBTCCDEF),$(COMPILER_DIR)/$(LIBTCCDEF)) $(COMPILER_DIR)/$(LIBTCC) $(COMPILER_DIR)/libtcc.a $(RUNTIME_DIR)/$(LIBTCC1)
TCCDOCS = tcc.1 tcc-doc.html tcc-doc.info

ifeq ($(INCLUDED),no)
# --------------------------------------------------------------------------
# Main build targets

all: setup-dirs $(PROGS) $(TCCLIBS) $(TCCDOCS)

# Create build directories
setup-dirs:
	@mkdir -p $(TEMP_DIR) $(COMPILER_DIR) $(RUNTIME_DIR)

# Compiler executable
$(COMPILER_DIR)/tcc$(EXESUF): $(TCC_OBJ) $(COMPILER_DIR)/$(LIBTCC)
	$(CC) -o $@ $(TCC_OBJ) -L$(COMPILER_DIR) -ltcc $(LIBS) $(LDFLAGS) -Wl,-rpath,$(abspath $(COMPILER_DIR))

# Compiler library 
$(COMPILER_DIR)/$(LIBTCC): $(LIBTCC_OBJ) $(CORE_OBJS)
	@mkdir -p $(COMPILER_DIR)
ifdef CONFIG_WIN32
ifeq ($(CONFIG_static),no)
	$(CC) -shared -o $(COMPILER_DIR)/libtcc$(DLLSUF) $(LIBTCC_OBJ) $(CORE_OBJS) $(LDFLAGS)
	$(if $(LIBTCCDEF),$(DLLTOOL) -l $(COMPILER_DIR)/$@ -d $(COMPILER_DIR)/$(LIBTCCDEF) -D $(notdir $(COMPILER_DIR)/libtcc$(DLLSUF)))
endif
else
ifeq ($(CONFIG_static),no)
	$(CC) -shared -install_name $(abspath $(COMPILER_DIR))/libtcc$(DLLSUF) -o $(COMPILER_DIR)/libtcc$(DLLSUF) $(LIBTCC_OBJ) $(CORE_OBJS) $(LIBS) $(LDFLAGS)
endif
endif
	$(AR) rcs $(COMPILER_DIR)/libtcc.a $(LIBTCC_OBJ) $(CORE_OBJS)

# Object file compilation rules
$(TEMP_DIR)/%.o: %.c | setup-dirs
	$(CC) -o $@ -c $< $(DEFINES) $(CFLAGS)

$(TEMP_DIR)/libtcc.o: libtcc.c $(CORE_OBJS)
	$(CC) -o $@ -c $< -DONE_SOURCE=0 $(DEFINES) $(CFLAGS)

$(TEMP_DIR)/tcc.o: tcc.c | setup-dirs
	$(CC) -o $@ -c $< $(DEFINES) $(CFLAGS)

# Runtime library (built in runtime directory)
$(RUNTIME_DIR)/$(LIBTCC1): $(COMPILER_DIR)/tcc$(EXESUF) setup-dirs FORCE
	@$(MAKE) -C lib TEMP_TARGET=$(abspath $(TEMP_DIR)) RUNTIME_TARGET=$(abspath $(RUNTIME_DIR)) XTCC=$(abspath $(COMPILER_DIR))/tcc$(EXESUF) NATIVE_TARGET=$(NATIVE_TARGET)

# Clean targets
clean:
	rm -rf $(BUILD_DIR)
	rm -f *.o *.a *.dylib tcc$(EXESUF)
	@$(MAKE) -C lib clean

distclean: clean
	rm -f config.h config.mak config.texi

# Other patterns remain the same
FORCE:

.PHONY: all clean distclean setup-dirs

endif