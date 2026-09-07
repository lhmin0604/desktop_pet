import struct

with open(r'D:\projects\desktop_pet\STC-B\output\DesktopPet_STC.ihx', 'r') as f:
    lines = f.readlines()

print("=== IHX 文件分析 ===")
print(f"总行数: {len(lines)}")

# 解析第一条记录（通常是启动地址）
first = lines[0].strip()
print(f"第一条: {first}")

# 找 reset vector 相关的记录
for line in lines[:10]:
    line = line.strip()
    if line.startswith(':'):
        byte_count = int(line[1:3], 16)
        address = int(line[3:7], 16)
        record_type = int(line[7:9], 16)
        print(f"  地址 0x{address:04X}, 类型 {record_type}, 字节数 {byte_count}")

# 检查是否有 0x0000 地址的数据
for line in lines:
    line = line.strip()
    if line.startswith(':'):
        address = int(line[3:7], 16)
        record_type = int(line[7:9], 16)
        if record_type == 0 and address == 0:
            data = line[9:9+int(line[1:3],16)*2]
            print(f"\nReset vector 数据: {data}")
            print(f"  字节 0-2: {data[0:6]}")
            if data[0:2] == '02':
                target = int(data[2:6], 16)
                print(f"  LJMP 0x{target:04X}")
            break
