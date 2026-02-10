#
# Tiny C Compiler Makefile for libtcc1.a - Separated Build
#

TOP = ..
include $(TOP)/config.mak
VPATH = $(TOPSRC)/lib $(TOPSRC)/win32/lib

# Build configuration  
T = $(or $(CROSS_TARGET),$(NATIVE_TARGET),unknown)
X = $(if $(CROSS_TARGET),$(CROSS_TARGET)-)
XCFG = $(or $(findstring -win,$T),-unx)
S = $(if $(findstring yes,$(SILENT)),@$(info * $@))

# Build directories
TEMP_TARGET ?= $(TOP)/build/temp
RUNTIME_TARGET ?= $(TOP)/build/runtime

# Compiler and tools configuration  
TCC = $(TOP)/build/compiler/tcc$(EXESUF)
XTCC ?= $(TCC)
XCC = $(XTCC)
XAR = $(XTCC) -ar
XFLAGS-unx = -B$(TOPSRC) -I$(TOPSRC)
XFLAGS-win = -B$(TOPSRC)/win32 -I$(TOPSRC)/include  
XFLAGS = $(XFLAGS$(XCFG))
BFLAGS = -bt

# Object file definitions (adapted from original)
I386_O = libtcc1.o $(COMMON_O)
X86_64_O = libtcc1.o $(COMMON_O) 
ARM_O = libtcc1.o armeabi.o armflush.o $(COMMON_O)
ARM64_O = lib-arm64.o $(COMMON_O)
RISCV64_O = lib-arm64.o $(COMMON_O)
COMMON_O = stdatomic.o atomic.o builtin.o alloca.o alloca-bt.o
WIN_O = crt1.o crt1w.o wincrt1.o wincrt1w.o dllcrt1.o dllmain.o  
LIN_O = dsohandle.o
OSX_O =

# Runtime object selection by target
OBJ-i386 = $(I386_O) pic86.o $(LIN_O)
OBJ-x86_64 = $(X86_64_O) $(LIN_O)
OBJ-x86_64-osx = $(X86_64_O) $(OSX_O)  
OBJ-arm64 = $(ARM64_O) $(LIN_O)
OBJ-arm64-osx = $(ARM64_O) $(OSX_O)
OBJ-arm = $(ARM_O) $(LIN_O)
OBJ-riscv64 = $(RISCV64_O) $(LIN_O)

# Additional runtime objects (not in libtcc1.a)
EXTRA_O = runmain.o bt-exe.o bt-dll.o bt-log.o bcheck.o tcov.o

# backtrace/bcheck/run only for native compiler 
Nat = $(if $(CROSS_TARGET),no,yes)
Cbt = $(if $(CONFIG_backtrace),$(Nat),no)
Cbc = $(if $(CONFIG_bcheck),$(Nat),no)

# Conditional inclusion of extra objects
RUNTIME_EXTRA_O = runmain.o
ifeq ($(Cbt),yes)
 RUNTIME_EXTRA_O += bt-exe.o bt-log.o
endif
ifeq ($(Cbc),yes)
 RUNTIME_EXTRA_O += bcheck.o
endif

# Filter extra objects for current target
OBJ-extra = $(filter $(EXTRA_O),$(OBJ-$(T)))
OBJ-libtcc1 = $(filter-out $(OBJ-extra),$(OBJ-$(T)))
ALL_RUNTIME = $(RUNTIME_TARGET)/libtcc1.a $(addprefix $(RUNTIME_TARGET)/,$(RUNTIME_EXTRA_O))

all: $(ALL_RUNTIME)

# Build libtcc1.a
$(RUNTIME_TARGET)/libtcc1.a: $(TCC) 
	@mkdir -p $(TEMP_TARGET) $(RUNTIME_TARGET)
	@echo "Building libtcc1.a for target $(T)..."
	@cd $(TEMP_TARGET) && $(XCC) -c ../../lib/lib-arm64.c $(XFLAGS)
	@cd $(TEMP_TARGET) && $(XCC) -c ../../lib/stdatomic.c $(XFLAGS)
	@cd $(TEMP_TARGET) && $(XCC) -c ../../lib/atomic.S $(XFLAGS)
	@cd $(TEMP_TARGET) && $(XCC) -c ../../lib/builtin.c $(XFLAGS)
	@cd $(TEMP_TARGET) && $(XCC) -c ../../lib/alloca.S $(XFLAGS)
	@cd $(TEMP_TARGET) && $(XCC) -c ../../lib/alloca-bt.S $(XFLAGS)
	@cd $(TEMP_TARGET) && $(XCC) -c ../../lib/dsohandle.c $(XFLAGS)
	@cd $(TEMP_TARGET) && $(XAR) rcs $(notdir $@) lib-arm64.o stdatomic.o atomic.o builtin.o alloca.o alloca-bt.o dsohandle.o
	@cp $(TEMP_TARGET)/$(notdir $@) $@

# Build runmain.o and other runtime extras separately
$(RUNTIME_TARGET)/runmain.o: runmain.c $(TCC)
	@mkdir -p $(TEMP_TARGET) $(RUNTIME_TARGET)
	$S$(XCC) -c $< -o $(TEMP_TARGET)/$(notdir $@) $(XFLAGS)
	@cp $(TEMP_TARGET)/$(notdir $@) $@

$(RUNTIME_TARGET)/bt-exe.o: bt-exe.c $(TCC)
	@mkdir -p $(TEMP_TARGET) $(RUNTIME_TARGET)
	$S$(XCC) -c $< -o $(TEMP_TARGET)/$(notdir $@) $(XFLAGS) $(BFLAGS)
	@cp $(TEMP_TARGET)/$(notdir $@) $@

$(RUNTIME_TARGET)/bt-log.o: bt-log.c $(TCC)
	@mkdir -p $(TEMP_TARGET) $(RUNTIME_TARGET)
	$S$(XCC) -c $< -o $(TEMP_TARGET)/$(notdir $@) $(XFLAGS) $(BFLAGS)
	@cp $(TEMP_TARGET)/$(notdir $@) $@

$(RUNTIME_TARGET)/bcheck.o: bcheck.c $(TCC)
	@mkdir -p $(TEMP_TARGET) $(RUNTIME_TARGET)  
	$S$(XCC) -c $< -o $(TEMP_TARGET)/$(notdir $@) $(XFLAGS) $(BFLAGS)
	@cp $(TEMP_TARGET)/$(notdir $@) $@

clean:
	rm -f $(RUNTIME_TARGET)/libtcc1.a $(addprefix $(RUNTIME_TARGET)/,$(RUNTIME_EXTRA_O))
	rm -f $(TEMP_TARGET)/*.o

.PHONY: all clean debug-vars