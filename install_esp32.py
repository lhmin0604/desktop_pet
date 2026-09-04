"""
ESP32 Arduino 板包离线安装脚本
从乐鑫中国镜像下载，绕过 GitHub 超时问题

用法: python install_esp32.py
"""

import os, sys, json, zipfile, shutil, hashlib
from pathlib import Path

try:
    import urllib.request
    from urllib.error import URLError, HTTPError
except ImportError:
    print("[错误] 需要 Python 3.6+")
    sys.exit(1)

# ==================== 路径配置 ====================
LOCAL_APP_DATA = os.environ.get('LOCALAPPDATA', '')
if not LOCAL_APP_DATA:
    # 尝试常见路径
    home = str(Path.home())
    LOCAL_APP_DATA = os.path.join(home, 'AppData', 'Local')

ARDUINO15  = os.path.join(LOCAL_APP_DATA, 'Arduino15')
HW_DIR     = os.path.join(ARDUINO15, 'packages', 'esp32', 'hardware', 'esp32')
TOOLS_DIR  = os.path.join(ARDUINO15, 'packages', 'esp32', 'tools')
DOWNLOADS  = os.path.join(os.path.dirname(os.path.abspath(__file__)), '_esp32_downloads')

MIRROR = 'https://dl.espressif.cn/github_assets'

# ==================== 工具函数 ====================

def size_str(n):
    if n > 1024*1024: return f"{n/1024/1024:.1f}MB"
    if n > 1024: return f"{n/1024:.1f}KB"
    return f"{n}B"

def download(url, dest):
    """下载文件，自动尝试中国镜像"""
    if os.path.exists(dest):
        size = os.path.getsize(dest)
        if size > 100000:
            print(f"  [已有] {os.path.basename(dest)} ({size_str(size)})")
            return True
        else:
            os.remove(dest)

    os.makedirs(os.path.dirname(dest), exist_ok=True)

    urls = [url]
    if 'github.com' in url:
        mirror_url = url.replace('https://github.com', MIRROR)
        urls.insert(0, mirror_url)  # 优先用镜像

    for u in urls:
        label = "镜像" if MIRROR in u else "GitHub"
        print(f"  [{label}] {os.path.basename(dest)}")
        try:
            req = urllib.request.Request(u, headers={'User-Agent': 'ESP32-Installer/1.0'})
            with urllib.request.urlopen(req, timeout=120) as resp:
                total = int(resp.headers.get('Content-Length', 0))
                downloaded = 0
                with open(dest, 'wb') as f:
                    while True:
                        chunk = resp.read(65536)
                        if not chunk:
                            break
                        f.write(chunk)
                        downloaded += len(chunk)
                        if total > 0:
                            pct = downloaded * 100 / total
                            bar = '█' * int(pct // 2.5) + '░' * (40 - int(pct // 2.5))
                            print(f"\r    [{bar}] {pct:.0f}% {size_str(downloaded)}/{size_str(total)}", end='', flush=True)
                print()
            return True
        except Exception as e:
            print(f"\n    ⚠ {label}失败: {e}")
            if os.path.exists(dest):
                os.remove(dest)

    return False

def extract_zip(zip_path, dest_dir):
    """解压 zip 到指定目录"""
    os.makedirs(dest_dir, exist_ok=True)
    print(f"  解压 → {dest_dir}")
    with zipfile.ZipFile(zip_path, 'r') as z:
        z.extractall(dest_dir)
    return True

# ==================== 主流程 ====================

def main():
    print("=" * 55)
    print("  ESP32 Arduino 板包离线安装器")
    print("  数据源: 乐鑫中国镜像 (dl.espressif.cn)")
    print("=" * 55)
    print()

    # 检查 Python 版本
    if sys.version_info < (3, 6):
        print("[错误] 需要 Python 3.6 或更高版本")
        sys.exit(1)

    # 检查 Arduino IDE 是否安装
    if not os.path.isdir(ARDUINO15):
        print(f"[错误] 未找到 Arduino IDE 配置目录:")
        print(f"  {ARDUINO15}")
        print(f"  请先安装并打开一次 Arduino IDE")
        sys.exit(1)

    os.makedirs(DOWNLOADS, exist_ok=True)
    print(f"下载缓存: {DOWNLOADS}")
    print(f"安装目标: {ARDUINO15}\\packages\\esp32\\")
    print()

    # ====== 第 1 步: 获取包索引，确定版本号 ======
    print("[1/3] 获取 ESP32 包索引...")
    index_url = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
    index_path = os.path.join(DOWNLOADS, "package_esp32_index.json")

    # 尝试多个源
    index_urls = [
        index_url.replace('https://espressif.github.io', MIRROR + '/espressif.github.io'),
        index_url,
    ]

    index_data = None
    for iu in index_urls:
        try:
            req = urllib.request.Request(iu, headers={'User-Agent': 'ESP32-Installer/1.0'})
            with urllib.request.urlopen(req, timeout=30) as resp:
                index_data = json.loads(resp.read().decode('utf-8'))
            break
        except:
            pass

    if index_data is None:
        print("  ⚠ 无法获取包索引，使用预设版本号")
        CORE_VER = "3.3.11"
        CORE_URL = f"https://github.com/espressif/arduino-esp32/releases/download/{CORE_VER}/esp32-core-{CORE_VER}.zip"
        XTENSA_VER = "14.2.0_20260121"
        TOOLS_TO_DOWNLOAD = []
    else:
        # 解析最新版本
        platforms = index_data['packages'][0]['platforms']
        # 找最新的 3.x 版本
        latest = None
        for p in platforms:
            ver = p.get('version', '')
            if ver.startswith('3.') and (latest is None or ver > latest['version']):
                latest = p

        if latest is None:
            latest = platforms[-1]

        CORE_VER = latest['version']
        CORE_URL = latest['url']  # ← 从索引获取真正的下载链接！
        print(f"  找到版本: {CORE_VER}")
        print(f"  核心包URL: {CORE_URL}")

        # 提取该版本需要的工具
        tools_list = latest.get('toolsDependencies', [])
        all_tools = index_data['packages'][0]['tools']

        TOOLS_TO_DOWNLOAD = []
        for dep in tools_list:
            tool_name = dep['name']
            tool_ver = dep['version']
            for tool in all_tools:
                if tool['name'] == tool_name and tool['version'] == tool_ver:
                    for sys_info in tool.get('systems', []):
                        host = sys_info.get('host', '')
                        if 'mingw32' in host and 'x86_64' in host:
                            TOOLS_TO_DOWNLOAD.append({
                                'name': tool_name,
                                'version': tool_ver,
                                'url': sys_info['url'],
                            })
                            break
                    break

    # ====== 第 2 步: 下载 ======
    print()
    print(f"[2/3] 下载文件 (版本 {CORE_VER})...")
    print()

    # ESP32 Arduino Core — 使用索引中的真实 URL
    core_filename = CORE_URL.split('/')[-1]
    core_zip = os.path.join(DOWNLOADS, core_filename)

    print(f"[核心] ESP32 Arduino Core {CORE_VER}")
    if not download(CORE_URL, core_zip):
        print("  [错误] 核心包下载失败！")
        print(f"  请手动下载: {CORE_URL}")
        sys.exit(1)

    # 如果从索引获取了工具列表，下载所有工具；否则只下载 xtensa
    if TOOLS_TO_DOWNLOAD:
        print(f"\n发现 {len(TOOLS_TO_DOWNLOAD)} 个工具需要下载:")
        tool_downloads = []
        for tool in TOOLS_TO_DOWNLOAD:
            name = tool['name']
            ver = tool['version']
            url = tool['url']
            filename = url.split('/')[-1]
            dest = os.path.join(DOWNLOADS, filename)

            print(f"\n[工具] {name} {ver}")
            if download(url, dest):
                tool_downloads.append({
                    'name': name,
                    'version': ver,
                    'zip': dest,
                })
            else:
                print(f"  [警告] {name} 下载失败，跳过")
    else:
        # 回退：只下载 xtensa-esp-elf
        print("\n[工具] xtensa-esp-elf (ESP32/S2/S3 编译器)")
        xtensa_filename = f"xtensa-esp-elf-{XTENSA_VER}-x86_64-w64-mingw32.zip"
        xtensa_url = f"https://github.com/espressif/crosstool-NG/releases/download/esp-{XTENSA_VER}/{xtensa_filename}"
        xtensa_zip = os.path.join(DOWNLOADS, xtensa_filename)

        tool_downloads = []
        if download(xtensa_url, xtensa_zip):
            tool_downloads.append({
                'name': 'xtensa-esp-elf',
                'version': f'esp-{XTENSA_VER}',
                'zip': xtensa_zip,
            })

    # ====== 第 3 步: 安装 ======
    print()
    print("[3/3] 安装到 Arduino IDE...")
    print()

    # 安装核心包
    # 核心包解压后通常有一个顶层目录 (如 esp32-3.3.11/)
    # 需要把内容放到 HW_DIR/<version>/ 下
    core_dest = os.path.join(HW_DIR, CORE_VER)
    if os.path.isdir(core_dest):
        print(f"  核心包已存在: {core_dest}")
    else:
        print("  安装核心包...")
        # 先解压到临时目录，检查结构
        temp_dir = os.path.join(DOWNLOADS, '_temp_core')
        if os.path.isdir(temp_dir):
            shutil.rmtree(temp_dir)
        extract_zip(core_zip, temp_dir)

        # 检查是否有顶层目录
        items = os.listdir(temp_dir)
        if len(items) == 1 and os.path.isdir(os.path.join(temp_dir, items[0])):
            # 有顶层目录，移动内容
            src = os.path.join(temp_dir, items[0])
            os.makedirs(os.path.dirname(core_dest), exist_ok=True)
            shutil.move(src, core_dest)
        else:
            # 没有顶层目录，直接移动
            os.makedirs(os.path.dirname(core_dest), exist_ok=True)
            shutil.move(temp_dir, core_dest)

        # 清理临时目录
        if os.path.isdir(temp_dir):
            shutil.rmtree(temp_dir, ignore_errors=True)

        print(f"  ✅ 核心包安装完成: {core_dest}")

    # 安装工具
    print()
    for td in tool_downloads:
        tool_name = td['name']
        tool_ver = td['version']
        tool_zip = td['zip']

        tool_dest = os.path.join(TOOLS_DIR, tool_name, tool_ver)
        if os.path.isdir(tool_dest) and os.listdir(tool_dest):
            print(f"  [已有] {tool_name} {tool_ver}")
        else:
            print(f"  [安装] {tool_name} {tool_ver}")
            # 清理旧版本
            tool_base = os.path.join(TOOLS_DIR, tool_name)
            if os.path.isdir(tool_base):
                for d in os.listdir(tool_base):
                    old_path = os.path.join(tool_base, d)
                    if os.path.isdir(old_path):
                        shutil.rmtree(old_path, ignore_errors=True)

            # 解压
            temp_tool = os.path.join(DOWNLOADS, f'_temp_{tool_name}')
            if os.path.isdir(temp_tool):
                shutil.rmtree(temp_tool)
            extract_zip(tool_zip, temp_tool)

            # 检查结构并移动
            items = os.listdir(temp_tool)
            os.makedirs(tool_dest, exist_ok=True)
            if len(items) == 1 and os.path.isdir(os.path.join(temp_tool, items[0])):
                src = os.path.join(temp_tool, items[0])
                # 把内容移动到 tool_dest
                for item in os.listdir(src):
                    s = os.path.join(src, item)
                    d = os.path.join(tool_dest, item)
                    if os.path.isdir(s):
                        if os.path.exists(d):
                            shutil.rmtree(d)
                        shutil.move(s, d)
                    else:
                        shutil.move(s, d)
            else:
                for item in items:
                    s = os.path.join(temp_tool, item)
                    d = os.path.join(tool_dest, item)
                    if os.path.isdir(s):
                        if os.path.exists(d):
                            shutil.rmtree(d)
                        shutil.move(s, d)
                    else:
                        shutil.move(s, d)

            # 清理
            if os.path.isdir(temp_tool):
                shutil.rmtree(temp_tool, ignore_errors=True)

            print(f"    ✅ {tool_dest}")

    # 验证安装
    print()
    print("=" * 55)
    print("  安装完成！验证结果:")
    print("=" * 55)

    ok = True
    core_check = os.path.join(HW_DIR, CORE_VER, 'boards.txt')
    if os.path.isfile(core_check):
        print(f"  ✅ 核心包: {core_check}")
    else:
        print(f"  ❌ 核心包未找到: {core_check}")
        # 列出实际安装的目录
        if os.path.isdir(HW_DIR):
            print(f"     已安装版本: {os.listdir(HW_DIR)}")
        ok = False

    # 编译器工具自 3.3.x 起改名为 esp-x32（xtensa）与 esp-rv32（riscv）
    for compiler in ('esp-x32', 'esp-rv32'):
        comp_check = os.path.join(TOOLS_DIR, compiler)
        if os.path.isdir(comp_check):
            versions = os.listdir(comp_check)
            print(f"  ✅ {compiler}: {versions}")
        else:
            print(f"  ❌ {compiler} 未安装")
            ok = False

    gdb_check = os.path.join(TOOLS_DIR, 'xtensa-esp-elf-gdb')
    if os.path.isdir(gdb_check):
        print(f"  ✅ xtensa-esp-elf-gdb: {os.listdir(gdb_check)}")
    else:
        print(f"  ⚠ xtensa-esp-elf-gdb 未安装（仅调试需要，编译不影响）")

    print()
    if ok:
        print("🎉 安装成功！")
        print()
        print("现在请:")
        print("  1. 重启 Arduino IDE")
        print("  2. Tools → Board → esp32 → ESP32S3 Dev Module")
        print("  3. File → Open → desktop_pet.ino")
        print("  4. 点 ✅ Verify 编译")
    else:
        print("⚠ 安装可能不完整，请检查上面的错误信息")
        print()
        print("如果核心包下载失败，可以手动下载:")
        print(f"  URL: {CORE_URL}")
        print(f"  解压到: {HW_DIR}\\{CORE_VER}\\")

    print()
    input("按回车键退出...")

if __name__ == '__main__':
    main()
