## Copyright Cypress Semiconductor Corporation, 2010-2018,
## All Rights Reserved
## UNPUBLISHED, LICENSED SOFTWARE.
##
## CONFIDENTIAL AND PROPRIETARY INFORMATION
## WHICH IS THE PROPERTY OF CYPRESS.
##
## Use of this file is governed
## by the license agreement included in the file
##
##      <install>/license/license.txt
##
## where <install> is the Cypress software
## installation root directory path.
##

FX3FWROOT?=../SDK
CYCONFOPT = fx3_release

#all:compile

include $(FX3FWROOT)/fw_build/fx3_fw/fx3_build_config.mak

MODULE = SDDC_FX3

all:$(MODULE).img

SOURCE= cyfxtx.c		\
	DebugConsole.c		\
	i2cmodule.c		\
	tuner_r82xx.c		\
	RunApplication.c	\
	Si5351.c		\
	adf4351.c       \
	StartStopApplication.c	\
	StartUP.c		\
	Support.c		\
	USBdescriptor.c	\
	USBhandler.c		\
	hf103.c		\
	bbrf103.c		\
	rx888.c		\
	rx999.c                \
	rx888r2.c              \
	rxlucy.c		\
	pcal6408a.c

ifeq ($(CYFXBUILD),arm)
SOURCE_ASM=cyfx_startup.S
else
SOURCE_ASM=cyfx_gcc_startup.S
endif

C_OBJECT=$(SOURCE:%.c=./%.o)
A_OBJECT=$(SOURCE_ASM:%.S=./%.o)

EXES = $(MODULE).$(EXEEXT)

$(MODULE).$(EXEEXT): $(A_OBJECT) $(C_OBJECT)
	$(LINK)

$(C_OBJECT) : %.o : %.c
	$(COMPILE)

$(A_OBJECT) : %.o : %.S
	$(ASSEMBLE)

ELF2IMG = ./elf2img

clean:
	rm -f ./$(MODULE).$(EXEEXT)
	rm -f $(ELF2IMG)
	rm -f ./$(MODULE).img
	rm -f ./$(MODULE).map
	rm -f ./*.o


compile: $(C_OBJECT) $(A_OBJECT) $(EXES)

$(ELF2IMG) : $(FX3FWROOT)/util/elf2img/elf2img.c
	gcc -O -Wall -o $@ $<

$(MODULE).img: $(EXES) $(ELF2IMG)
	$(ELF2IMG) -i $< -o $@ -v

#[]#
