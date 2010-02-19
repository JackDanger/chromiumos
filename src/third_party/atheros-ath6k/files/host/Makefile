#------------------------------------------------------------------------------
# <copyright file="makefile" company="Atheros">
#    Copyright (c) 2005-2007 Atheros Corporation.  All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation;
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
#
#------------------------------------------------------------------------------
#==============================================================================
# Author(s): ="Atheros"
#==============================================================================
#  Required Variables:  
#
#    ATH_BUILD_OUTPUT - output path override for compiled executables
#

# Include local variables

export  ATH_CROSS_COMPILE_TYPE

MAKE :=make
CC :=$(ATH_CROSS_COMPILE_TYPE)gcc
LD :=$(ATH_CROSS_COMPILE_TYPE)ld
STRIP :=$(ATH_CROSS_COMPILE_TYPE)strip

export  STRIP

#export compiler variables for 3rd party applications (like the WPA supplicant)
export  CC
export  LD
export  STRIP

# Set up object output areas
COMPILED_BIN_OBJECTS_PATH :=$(ATH_BUILD_OUTPUT)

# Export the required variables to other Makefiles in the system
export COMPILED_BIN_OBJECTS_PATH

all:
	$(MAKE) -C tools/recEvent/
	$(MAKE) -C tools/drvdebugctrl/

install:
ifndef ATH_BUILD_OUTPUT
	@echo "Please specify the ATH_BUILD_OUTPUT variable"
	exit 1
endif
	cp -f tools/recEvent/recEvent $(COMPILED_BIN_OBJECTS_PATH)
	cp -f tools/drvdebugctrl/drvdebugctrl $(COMPILED_BIN_OBJECTS_PATH)
	
clobber: clean

clean:	
	rm -f tools/recEvent/recEvent tools/drvdebugctrl/drvdebugctrl
