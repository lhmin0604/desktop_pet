with open(r'D:\projects\desktop_pet\STC-B\output\DesktopPet_STC.ihx', 'r') as f:
    lines = f.readlines()

buf = bytearray(65536)
for line in lines:
    line = line.strip()
    if not line.startswith(':'):
        continue
    byte_count = int(line[1:3], 16)
    address = int(line[3:7], 16)
    record_type = int(line[7:9], 16)
    if record_type != 0:
        continue
    for i in range(byte_count):
        buf[address + i] = int(line[9+i*2:11+i*2], 16)

print('=== IHX Verification ===')
print('Reset: LJMP 0x%04X' % (buf[1]*256+buf[2]))
print()
print('Interrupt vectors:')
for addr, name in [(0x000B,'Timer0'), (0x0033,'UART2')]:
    if buf[addr] == 0x02:
        target = buf[addr+1]*256 + buf[addr+2]
        print('  0x%04X %s: LJMP 0x%04X' % (addr, name, target))
    else:
        print('  0x%04X %s: NOT LJMP (0x%02X)' % (addr, name, buf[addr]))
