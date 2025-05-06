# Copyright (C) 2013-2018 Altera Corporation, San Jose, California, USA. All rights reserved.
# Permission is hereby granted, free of charge, to any person obtaining a copy of this
# software and associated documentation files (the "Software"), to deal in the Software
# without restriction, including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to
# whom the Software is furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all copies or
# substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# This agreement shall be governed in all respects by the laws of the State of California and
# by the laws of the United States of America.
# This is a GNU Makefile.

# You must configure INTELFPGAOCLSDKROOT to point the root directory of the Intel(R) FPGA SDK for OpenCL(TM)
# software installation.
# See http://www.altera.com/literature/hb/opencl-sdk/aocl_getting_started.pdf
# for more information on installing and configuring the Intel(R) FPGA SDK for OpenCL(TM).

ifeq ($(VERBOSE),1)
ECHO :=
else
ECHO := @
endif

# Where is the Intel(R) FPGA SDK for OpenCL(TM) software?
ifeq ($(wildcard $(INTELFPGAOCLSDKROOT)),)
$(error Set INTELFPGAOCLSDKROOT to the root directory of the Intel(R) FPGA SDK for OpenCL(TM) software installation)
endif
ifeq ($(wildcard $(INTELFPGAOCLSDKROOT)/host/include/CL/opencl.h),)
$(error Set INTELFPGAOCLSDKROOT to the root directory of the Intel(R) FPGA SDK for OpenCL(TM) software installation.)
endif

# OpenCL compile and link flags.
AOCL_COMPILE_CONFIG := $(shell aocl compile-config --arm)

# Update this variable if libacl_emulator_kernel_rt.so is in a different directory
EMULATOR_LIB_DIR := $(INTELFPGAOCLSDKROOT)/host/arm32/lib

# New setup with explicit paths and libraries
LIB_PATHS := -L$(AOCL_BOARD_PACKAGE_ROOT)/arm32/lib -L$(INTELFPGAOCLSDKROOT)/host/arm32/lib -L$(INTELFPGAOCLSDKROOT)/host/linux64/lib
LIBS_TO_LINK := -lalteracl -lintel_soc32_mmd -lstdc++ -lelf

# Ensure you also include the runtime linker flags to avoid "as-needed" issues
LINKER_FLAGS := -Wl,--no-as-needed

# Modify AOCL_LINK_CONFIG to include -rpath or -rpath-link
AOCL_LINK_CONFIG := $(LIB_PATHS) $(LIBS_TO_LINK) $(LINKER_FLAGS) -Wl,-rpath,$(EMULATOR_LIB_DIR) -Wl,-rpath-link,$(EMULATOR_LIB_DIR)

# Compilation flags
ifeq ($(DEBUG),1)
CXXFLAGS += -g
else
CXXFLAGS += -O2
endif

# Compiler. ARM cross-compiler.
CXX := arm-linux-gnueabihf-g++

# Target
TARGET := host
TARGET_DIR := bin

# Directories
INC_DIRS := ../common/inc
LIB_DIRS :=

# Files
INCS := $(wildcard )
SRCS := $(wildcard host/src/*.cpp ../common/src/AOCLUtils/*.cpp)
LIBS := rt pthread

# Make it all!
all : $(TARGET_DIR)/$(TARGET)

# Host executable target.
$(TARGET_DIR)/$(TARGET) : Makefile $(SRCS) $(INCS) $(TARGET_DIR)
	@echo "Compiling with command:"
	$(ECHO)echo $(CXX) $(CPPFLAGS) $(CXXFLAGS) -fPIC $(foreach D,$(INC_DIRS),-I$D) \
	    $(AOCL_COMPILE_CONFIG) $(SRCS) $(AOCL_LINK_CONFIG) \
	    $(foreach D,$(LIB_DIRS),-L$D) \
	    $(foreach L,$(LIBS),-l$L) \
	    -o $(TARGET_DIR)/$(TARGET)
	$(ECHO)$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fPIC $(foreach D,$(INC_DIRS),-I$D) \
	    $(AOCL_COMPILE_CONFIG) $(SRCS) $(AOCL_LINK_CONFIG) \
	    $(foreach D,$(LIB_DIRS),-L$D) \
	    $(foreach L,$(LIBS),-l$L) \
	    -o $(TARGET_DIR)/$(TARGET)

# # Host executable target.
# $(TARGET_DIR)/$(TARGET) : Makefile $(SRCS) $(INCS) $(TARGET_DIR)
# 	$(ECHO)$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fPIC $(foreach D,$(INC_DIRS),-I$D) \
# 			$(AOCL_COMPILE_CONFIG) $(SRCS) $(AOCL_LINK_CONFIG) \
# 			$(foreach D,$(LIB_DIRS),-L$D) \
# 			$(foreach L,$(LIBS),-l$L) \
# 			-o $(TARGET_DIR)/$(TARGET)

$(TARGET_DIR) :
	$(ECHO)mkdir $(TARGET_DIR)

# Standard make targets
clean :
	$(ECHO)rm -f $(TARGET_DIR)/$(TARGET)

.PHONY : all clean
