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

---

## 🔴 当前卡住的问题（未解决）

### 症状

Hook 函数**已被正确调用**（LCALL 到我们的函数），且**返回 1** (MOV DPL, #0x01; RET)，
成功跳过了 SDCC 默认的 buggy genXINIT/genXRAMCLEAR。

**但是**，hook 内部的 XSEG 清零循环和 XINIT 复制循环使用的立即数可能是错误的。

### 具体表现

上一次编译验证时发现：
- `XSEG_SIZE` 定义为 `0x010E` (270)，但编译器生成的汇编是 `MOV R6, #0x01`（只清 1 字节）
- `XINIT_SIZE` 定义为 `0x0026` (38)，但编译器生成的汇编是 `MOV R4, #0x0E; MOV R5, #0x01`（复制 270 字节）
- 两个值**看起来互换了**，而且都不对

### 尝试过的方案及失败原因

| 方案 | 结果 | 原因 |
|------|------|------|
| 在 asm 里定义 `__sdcc_gsinit_startup` 替换默认启动 | ❌ 失败 | 链接器输出了两份 HOME 区代码（我们的 + 库的），库的排在后面覆盖了我们的 |
| `--no-std-crt0` 禁止默认 crt0 | ❌ 失败 | 链接器报 "No definition of area HOME/XSEG/PSEG" |
| 后处理 IHX 删除重复记录 | ⚠️ 部分成功 | 能去掉库的重复记录，但 HOME 区有多段重叠（GSINIT3/GSINIT4），处理复杂且脆弱 |
| 在 asm 里定义 `__sdcc_external_startup` | ❌ 失败 | 链接器丢弃了我们的版本，用了库的 weak 定义 |
| 在 C 里引用链接器符号 (`extern unsigned char l_XSEG`) | ❌ 失败 | SDCC 链接器符号无下划线前缀 (`l_XSEG`)，C 编译后带下划线 (`_l_XSEG`)，无法匹配 |
| 在 C 里用 `#define` 硬编码地址值 | ⚠️ **待验证** | 上次编译发现编译器把 define 的值解析错了（疑似混淆了地址和值），尚未完成修正后的重新验证 |
| 用 linker_syms.asm 桥接符号命名 | ❌ 失败 | `.equ` 不能用于链接时才确定的符号，报 relocation error |

### 可能的根因

1. **SDCC 编译器对常量折叠有 bug**：当 `#define` 的值与某些内部符号地址接近时，编译器可能用符号地址替换了常量值
2. **上次验证时 .lst 文件是旧的**：.lst 和 .rel/BIN 不匹配，可能导致误判
3. **`p[i] = 0` vs `*p++ = 0` 的编译差异**：不同的 C 写法可能导致编译器生成不同的循环结构

### 下一步应该做什么

1. **重新编译 main_app.c**（确保 .lst 和 .rel 都是最新的），检查 hook 函数的 .lst 输出中循环计数是否正确：
   ```
   预期: MOV R6, #0x0E; MOV R7, #0x01  (XSEG_SIZE = 0x010E)
   预期: MOV R4, #0x26; MOV R5, #0x00  (XINIT_SIZE = 0x0026)
   ```

2. 如果立即数仍然错误，尝试以下替代方案：
   - **方案 A**：用 `volatile unsigned int` 变量存储大小，阻止编译器常量折叠
   - **方案 B**：在 hook 函数中用 `__asm ... __endasm` 内联汇编直接写正确的 MOVX/MOVC 循环
   - **方案 C**：在 .asm 文件中定义 `___sdcc_external_startup`，用 `.dw` 数据段存放常量，从 C 读取

3. 确认 XINIT_START 地址正确（当前是 0x143E，随代码变动会变化）

4. 重新链接 → 生成 BIN → 验证 BIN 中 hook 函数的字节是否匹配预期

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

# 3. 验证循环计数是否正确（关键！）
# 从 map 获取 ___sdcc_external_startup 的地址 hook_addr
# 检查 hook_addr+1 和 hook_addr+3 的值:
#   d[hook_addr+1] 应为 XSEG_SIZE 低字节 (0x0E)
#   d[hook_addr+3] 应为 XSEG_SIZE 高字节 (0x01)

# 4. face_table 数据应完整存在于 CODE 区
# 搜索字节序列: 21 03 00 00 00 00 0C 18 (笑脸表情)
```
