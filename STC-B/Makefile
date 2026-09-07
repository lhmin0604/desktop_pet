# Makefile - STC-B SDCC build (Linux)
# 协议: paw_box + RS485 (板上 MAX485, P3.6/P3.7, T2 波特率)
#
# 用法:
#   make           # 编译 output/DesktopPet_STC.ihx
#   make flash     # 编译并通过 stcgal 烧录 (需先: pip install stcgal)
#   make clean     # 清理 output/

SDCC    ?= sdcc
SDAS    ?= sdas8051
SDCC_LIB_DIR ?= /tmp/sdcc_local/usr/share/sdcc/lib

SRC     = source
INC     = inc
OUT     = output

CFLAGS  = -mmcs51 --model-large --std-sdcc99 --opt-code-size -I$(INC) -I$(SRC)
LDFLAGS = -mmcs51 --model-large --opt-code-size -L $(SDCC_LIB_DIR)/large

# ============ 源文件 ============
# 应用入口 + 通信栈 (paw_box + 485) + 8 个 SDCC native 外设驱动
SRCS_C = $(SRC)/main_app.c \
         $(SRC)/comm_pawbox_sdcc.c \
         $(SRC)/bsp485_sdcc.c \
         $(SRC)/sys.c \
         $(SRC)/display.c \
         $(SRC)/beep.c \
         $(SRC)/keys.c \
         $(SRC)/adc_drv.c \
         $(SRC)/vib.c \
         $(SRC)/hall.c \
         $(SRC)/expression.c

# 弃用 (XOR 协议 + TTL 直连 P1.0/P1.1), 保留作存档, 不参与 link
OBSOLETE_SRCS = $(SRC)/comm_esp32.c \
                $(SRC)/uart2_ext.c

RELS_C    = $(patsubst $(SRC)/%.c,$(OUT)/%.rel,$(SRCS_C))
TARGET    = $(OUT)/DesktopPet_STC.ihx
HEX       = $(OUT)/DesktopPet_STC.hex

.PHONY: all clean flash

all: $(TARGET)

$(OUT):
	mkdir -p $(OUT)

# crt0 (sdas8051 汇编) 必须在 source/ 下跑, 产物挪到 output/
$(OUT)/crt0_sdcc.rel: $(SRC)/crt0_sdcc.asm | $(OUT)
	cd $(SRC) && $(SDAS) -plosgff crt0_sdcc.asm
	mv $(SRC)/crt0_sdcc.rel $(OUT)/
	touch $(OUT)/crt0_sdcc.lst

$(OUT)/%.rel: $(SRC)/%.c | $(OUT)
	$(SDCC) -c $(CFLAGS) -o $@ $<

# 链接: crt0 + 11 个 C 模块
$(TARGET): $(OUT)/crt0_sdcc.rel $(RELS_C) | $(OUT)
	cd $(OUT) && $(SDCC) $(LDFLAGS) -o DesktopPet_STC.ihx \
		crt0_sdcc.rel \
		main_app.rel \
		comm_pawbox_sdcc.rel \
		bsp485_sdcc.rel \
		sys.rel \
		display.rel \
		beep.rel \
		keys.rel \
		adc_drv.rel \
		vib.rel \
		hall.rel \
		expression.rel

# 转 Intel HEX (供 stcgal / 烧录软件用)
$(HEX): $(TARGET)
	makebin -s 32768 $< $@

flash: $(HEX)
	stcgal -p /dev/ttyUSB0 -b 115200 -t 0 $(HEX)

clean:
	rm -rf $(OUT)
