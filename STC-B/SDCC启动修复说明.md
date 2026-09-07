# SDCC 启动代码 Bug 修复说明

## 问题背景

STC-B 板 (STC15F2K60S2) 使用 SDCC 编译后，7段数码管显示异常（最右边一位全亮，其余灭）。
通过反汇编分析固件，发现根因是 **SDCC 默认启动代码 (genXINIT / genXRAMCLEAR) 存在 bug**。

---

## Bug 分析

### SDCC 启动流程

```
CPU Reset → 0x0000
    → __sdcc_gsinit_startup (库代码, GSINIT0)
        → MOV SP, #0xFF
        → LCALL ___sdcc_external_startup   ← hook 钩子
        → 如果返回 0: 执行 genXINIT + genXRAMCLEAR
        → 如果返回非 0: 跳过 genXINIT + genXRAMCLEAR
    → GSFINAL: LJMP _main
```

### genXRAMCLEAR Bug (XDATA BSS 清零)

当 XSEG 大小的高低字节都非零时（我们的项目 XSEG = 0x010E = 270 字节），
默认的双重循环逻辑出错：

```asm
; SDCC 默认 genXRAMCLEAR (有 bug):
MOV R0, #l_XSEG        ; R0 = 0x0E (低字节)
MOV R1, #(l_XSEG >> 8) ; R1 = 0x01 (高字节)
...
genXRAMCLEAR:
    CJNE R0, #0, inner  ; 低字节≠0 时跳入内循环
inner:
    MOVX @DPTR, A       ; 写零
    INC DPTR
    DJNZ R0, inner      ; R0 减到 0 时退出内循环
    DJNZ R1, genXRAMCLEAR ; R1 减 1, 再进外循环
```

**问题**：第一轮内循环执行 14 次 (R0=0x0E→0)，DJNZ R1 使 R1=0x00。
第二轮外循环时 R0 仍为 0，`CJNE R0, #0` 不跳转，内循环执行 256 次（R0=0 等价于 256）。
但此时 R1 已为 0，DJNZ R1 直接退出。

**结果**：只清了 14 + 14 = 28 字节（应为 270 字节），242 字节的 XDATA BSS 未被清零。

### genXINIT Bug (XDATA 初始化数据复制)

类似的双重循环 bug，当 XINIT 长度高低字节都非零时可能越界复制，
将 XINIT 区域之外的数据（可能是 0xFF）写入 XISEG，破坏已初始化的变量。

---

## 修复方案

### 方案：C 语言 Hook 函数

在 `main_app.c` 中定义 `__sdcc_external_startup()` 钩子函数：

```c
unsigned char __sdcc_external_startup(void)
{
    // 1. 正确清零 XSEG (270 字节 XDATA BSS)
    unsigned char __xdata *p = (unsigned char __xdata *)XSEG_START;
    for (i = 0; i < XSEG_SIZE; i++) p[i] = 0;

    // 2. 正确复制 XINIT → XISEG (38 字节, 用 MOVC 读 CODE)
    unsigned char __code *src = (unsigned char __code *)XINIT_START;
    unsigned char __xdata *dst = (unsigned char __xdata *)XISEG_START;
    for (i = 0; i < XINIT_SIZE; i++) dst[i] = src[i];

    return 1;  // 返回非零 → 跳过 SDCC 默认的 buggy genXINIT/genXRAMCLEAR
}
```

库的 `__sdcc_gsinit_startup` 会调用此 hook，收到非零返回值后跳过默认初始化。

### 修改的文件

| 文件 | 改动说明 |
|------|----------|
| `source/main_app.c` | 新增 `__sdcc_external_startup` hook 函数 |
| `build_sdcc.bat` | 移除 crt0_sdcc.asm 汇编步骤，简化构建流程 |
| `source/expression.c` | face_table/sound 改为 `static const` 放入 CODE 区（绕过 XINIT） |
| `source/display.c` | 小幅适配修改 |
| `source/keys.c` | 小幅适配修改 |
| `source/comm_pawbox_sdcc.c` | 小幅适配修改 |
| `source/crt0_sdcc.asm` | 保留作为参考，**不再参与构建** |

---

## 构建方法

```bash
# Windows 下直接运行
STC-B\build_sdcc.bat

# 或手动构建 (bash/git-bash)
export PATH="/f/SDCC/bin:$PATH"
cd STC-B/output

# 编译所有 C 文件
for f in main_app comm_pawbox_sdcc bsp485_sdcc sys display beep keys adc_drv vib hall expression; do
    sdcc -c -mmcs51 --model-large --std-sdcc99 --opt-code-size \
        -I "../source" -I "../inc" -I "F:/SDCC/include" \
        -o "${f}.rel" "../source/${f}.c"
done

# 链接
sdcc -mmcs51 --model-large --opt-code-size \
    -o DesktopPet_STC.ihx \
    main_app.rel comm_pawbox_sdcc.rel bsp485_sdcc.rel sys.rel display.rel \
    beep.rel keys.rel adc_drv.rel vib.rel hall.rel expression.rel \
    -I "F:/SDCC/include" -L "F:/SDCC/lib/mcs51"

# 生成 BIN
makebin -s 65536 DesktopPet_STC.ihx DesktopPet_STC_8k.bin
```

输出：`output/DesktopPet_STC_8k.bin` (64KB)，可直接烧录到 STC15F2K60S2。

---

## ⚠️ 当前状态与注意事项

### 硬编码地址问题

hook 函数中的内存地址是从 `.map` 文件硬编码提取的：

```c
#define XSEG_START   0x0001   // XDATA BSS 起始
#define XSEG_SIZE    0x010E   // XDATA BSS 长度 (270 字节)
#define XISEG_START  0x010F   // XDATA 初始化区起始
#define XINIT_START  0x143E   // CODE 中初始化数据源
#define XINIT_SIZE   0x0026   // 初始化数据长度 (38 字节)
```

**如果源文件增删导致内存布局变化，这些值需要同步更新！** 方法：
1. 先正常链接一次
2. 查看 `output/DesktopPet_STC.map` 中的 `s_XSEG`, `l_XSEG`, `s_XISEG`, `s_XINIT`, `l_XINIT`
3. 更新 `main_app.c` 中的 `#define` 值

### SDCC 链接器符号引用问题

SDCC 对链接器符号（如 `l_XSEG`, `s_XINIT`）的 C 级别引用存在 bug：
编译器会将符号的**地址**和符号地址处的**值**混淆，导致生成错误的立即数。
因此当前采用硬编码方式，而非引用链接器符号。

### 验证方法

构建后可用 Python 验证固件：

```python
with open('output/DesktopPet_STC_8k.bin', 'rb') as f:
    d = f.read()

# 1. Reset vector 应跳转到 __sdcc_gsinit_startup
assert d[0] == 0x02  # LJMP 指令

# 2. Hook 函数应存在且返回 1
# 在 map 文件中查找 ___sdcc_external_startup 地址
# 该函数末尾应为: 75 82 01 22 (MOV DPL, #1; RET)

# 3. face_table 数据应完整存在于 CODE 区
# 搜索字节序列: 21 03 00 00 00 00 0C 18 (笑脸表情)
```
