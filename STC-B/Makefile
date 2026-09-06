# Makefile - STC-B SDCC build (Linux)
SDCC ?= sdcc
SDAS ?= sdas8051
SDCC_LIB_DIR ?= /tmp/sdcc_local/usr/share/sdcc/lib

SRC = source
INC = inc
OUT = output

CFLAGS = -mmcs51 --model-large --std-sdcc99 --opt-code-size -I$(INC) -I$(SRC)
LDFLAGS = -mmcs51 --model-large --opt-code-size -L $(SDCC_LIB_DIR)/large

SRCS_C = $(SRC)/main_app.c \
         $(SRC)/comm_esp32.c \
         $(SRC)/uart2_ext.c \
         $(SRC)/sys.c \
         $(SRC)/display.c \
         $(SRC)/beep.c \
         $(SRC)/keys.c \
         $(SRC)/adc_drv.c \
         $(SRC)/vib.c \
         $(SRC)/hall.c \
         $(SRC)/expression.c

RELS_C = $(patsubst $(SRC)/%.c,$(OUT)/%.rel,$(SRCS_C))
TARGET = $(OUT)/DesktopPet_STC.ihx

.PHONY: all clean

all: $(TARGET)

$(OUT):
	mkdir -p $(OUT)

$(OUT)/crt0_sdcc.rel: $(SRC)/crt0_sdcc.asm | $(OUT)
	cd $(SRC) && $(SDAS) -posff crt0_sdcc.asm
	mv $(SRC)/crt0_sdcc.rel $(OUT)/
	touch $(OUT)/crt0_sdcc.lst

$(OUT)/%.rel: $(SRC)/%.c | $(OUT)
	$(SDCC) -c $(CFLAGS) -o $@ $<

$(TARGET): $(OUT)/crt0_sdcc.rel $(RELS_C) | $(OUT)
	cd $(OUT) && $(SDCC) $(LDFLAGS) -o DesktopPet_STC.ihx \
		crt0_sdcc.rel \
		main_app.rel \
		comm_esp32.rel \
		uart2_ext.rel \
		sys.rel \
		display.rel \
		beep.rel \
		keys.rel \
		adc_drv.rel \
		vib.rel \
		hall.rel \
		expression.rel

clean:
	rm -rf $(OUT)
