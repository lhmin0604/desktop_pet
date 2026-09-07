# fix_uart2_vector.py - Fix UART2 interrupt vector in both BIN and IHX
# SDCC places interrupt(8) at 0x0043, but STC15 expects it at 0x0033
import sys, os

out_dir = r'D:\projects\desktop_pet\STC-B\output'
bin_file = os.path.join(out_dir, 'DesktopPet_STC_8k.bin')
ihx_file = os.path.join(out_dir, 'DesktopPet_STC.ihx')

def fix_bin(path):
    with open(path, 'rb') as f:
        data = bytearray(f.read())
    uart2_vec = data[0x43:0x46]
    if uart2_vec[0] != 0x02:
        print('BIN: No UART2 vector at 0x0043 (0x%02X), skip' % uart2_vec[0])
        return
    target = (uart2_vec[1] << 8) | uart2_vec[2]
    print('BIN: UART2 at 0x0043 -> LJMP 0x%04X, copy to 0x0033' % target)
    data[0x33:0x36] = uart2_vec
    data[0x43:0x46] = b'\xFF\xFF\xFF'
    with open(path, 'wb') as f:
        f.write(data)
    print('BIN: Fixed!')

def fix_ihx(path):
    with open(path, 'r') as f:
        lines = f.readlines()
    
    # Parse all records, find data at 0x0033 and 0x0043
    # We need to inject a record at 0x0033 with the UART2 vector bytes
    # and remove the bytes at 0x0043 if they exist
    
    # First pass: read all data into a buffer
    buf = bytearray(65536)
    mask = bytearray(65536)  # which bytes are written
    
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
            mask[address + i] = 1
    
    # Check UART2 vector at 0x0043
    if buf[0x43] != 0x02:
        print('IHX: No UART2 vector at 0x0043 (0x%02X), skip' % buf[0x43])
        return
    
    target = (buf[0x44] << 8) | buf[0x45]
    print('IHX: UART2 at 0x0043 -> LJMP 0x%04X, copy to 0x0033' % target)
    
    # Copy vector to 0x0033
    buf[0x33] = buf[0x43]
    buf[0x34] = buf[0x44]
    buf[0x35] = buf[0x45]
    mask[0x33] = 1
    mask[0x34] = 1
    mask[0x35] = 1
    
    # Clear 0x0043
    buf[0x43] = 0xFF
    buf[0x44] = 0xFF
    buf[0x45] = 0xFF
    
    # Rebuild IHX from buffer
    new_lines = []
    addr = 0
    while addr < 65536:
        if not mask[addr]:
            addr += 1
            continue
        # Find contiguous block
        start = addr
        while addr < 65536 and mask[addr] and (addr - start) < 16:
            addr += 1
        length = addr - start
        rec = ':%02X%04X00' % (length, start)
        checksum = length + (start >> 8) + (start & 0xFF)
        for i in range(length):
            rec += '%02X' % buf[start + i]
            checksum += buf[start + i]
        rec += '%02X\n' % ((-checksum) & 0xFF)
        new_lines.append(rec)
    
    new_lines.append(':00000001FF\n')
    
    with open(path, 'w') as f:
        f.writelines(new_lines)
    print('IHX: Fixed!')

if os.path.exists(bin_file):
    fix_bin(bin_file)
if os.path.exists(ihx_file):
    fix_ihx(ihx_file)
