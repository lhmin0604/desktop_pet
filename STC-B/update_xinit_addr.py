# update_xinit_addr.py - Extract addresses from .map and update main_app.c
import re
import sys

map_file = r'D:\projects\desktop_pet\STC-B\output\DesktopPet_STC.map'
src_file = r'D:\projects\desktop_pet\STC-B\source\main_app.c'

with open(map_file, 'r') as f:
    map_content = f.read()

# Map format: "C:   XXXXXXXX  symbol_name"
# Address comes BEFORE symbol name
patterns = {
    'XSEG_START':  r'[0-9A-Fa-f]{8}\s+s_XSEG\b',
    'XSEG_SIZE':   r'[0-9A-Fa-f]{8}\s+l_XSEG\b',
    'XISEG_START': r'[0-9A-Fa-f]{8}\s+s_XISEG\b',
    'XINIT_START': r'[0-9A-Fa-f]{8}\s+s_XINIT\b',
    'XINIT_SIZE':  r'[0-9A-Fa-f]{8}\s+l_XINIT\b',
}

addrs = {}
for name, pat in patterns.items():
    m = re.search(pat, map_content)
    if m:
        addr = m.group(0).split()[0].upper()
        addrs[name] = addr
    else:
        print(f"ERROR: cannot find {name} in map file")
        sys.exit(1)

print("Extracted from map:")
for k, v in addrs.items():
    print(f"  {k} = 0x{v}")

with open(src_file, 'r', encoding='utf-8') as f:
    src = f.read()

for name, val in addrs.items():
    pattern = rf'#define\s+{name}\s+0x[0-9A-Fa-f]+U?'
    replacement = f'#define {name}  0x{val}U'
    src = re.sub(pattern, replacement, src)

with open(src_file, 'w', encoding='utf-8') as f:
    f.write(src)

print(f"Updated {src_file}")
