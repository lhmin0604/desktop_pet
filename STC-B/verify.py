with open(r'D:\projects\desktop_pet\STC-B\output\DesktopPet_STC_8k.bin', 'rb') as f:
    d = f.read()

hook = 0x00EA
print('=== Hook function at 0x%04X ===' % hook)
print()
print('Bytes:')
for i in range(0, 64, 16):
    hex_str = ' '.join('%02X' % d[hook+i+j] for j in range(min(16, 64-i)))
    print('  0x%04X: %s' % (hook+i, hex_str))
print()

# Find key patterns
print('Key bytes:')
# Look for subb a,#imm pattern (0x94 followed by immediate)
for i in range(60):
    if d[hook+i] == 0x94:
        print('  subb a,#0x%02X at offset +0x%02X' % (d[hook+i+1], i))
    if d[hook+i] == 0x75 and d[hook+i+1] == 0x82:
        print('  mov dpl,#0x%02X at offset +0x%02X' % (d[hook+i+2], i))
    if d[hook+i] == 0x22:
        print('  ret at offset +0x%02X' % i)
