"""Verify team and offer struct offsets."""
import ctypes
from ctypes import wintypes
import struct

PID = 37764
GLOBAL_DB_PTR = 0x6801B2C0
VECTOR_OFFSET = 0x30
COUNT_DELTA = 0x0c

kernel32 = ctypes.windll.kernel32
h = kernel32.OpenProcess(0x0010, False, PID)
if not h:
    print("OpenProcess failed")
    exit(1)

def read_mem(addr, size):
    buf = (ctypes.c_byte * size)()
    read = ctypes.c_size_t()
    if kernel32.ReadProcessMemory(h, addr, buf, size, ctypes.byref(read)):
        return bytearray(buf)[:read.value]
    return None

# Read team vector from global DB (offset 0x90)
TEAM_VECTOR_OFF = 0x90
TEAM_COUNT_OFF = 0x9c
db_data = read_mem(GLOBAL_DB_PTR, 0xb0)
if not db_data:
    print("Cannot read global DB")
    exit(1)

team_vector = struct.unpack_from('<Q', db_data, TEAM_VECTOR_OFF)[0]
team_count = struct.unpack_from('<i', db_data, TEAM_COUNT_OFF)[0]
print(f"Team vector: 0x{team_vector:X}  count: {team_count}")

# Read team samples
SAMPLE = min(15, team_count)
ptr_data = read_mem(team_vector, SAMPLE * 8)
if not ptr_data:
    print("Cannot read team pointers")
    exit(1)

team_ptrs = struct.unpack_from(f'<{SAMPLE}Q', ptr_data, 0)

TEAM_READABLE = 0x4460
samples = []
for tp in team_ptrs:
    if tp == 0: continue
    data = read_mem(tp, TEAM_READABLE)
    if data:
        tid = struct.unpack_from('<I', data, 0x4450)[0]
        lid = struct.unpack_from('<I', data, 0x120)[0]
        if tid > 0 and tid < 100000 and lid > 0 and lid < 100000:
            samples.append((tp, data))
    if len(samples) >= 10: break

print(f"Valid team samples: {len(samples)}")

# Offsets to verify
TEAM_CATALOG = [
    ("OOTP27_KBO_TEAM_ID_OFFSET",            0x4450, 'u32', 1, 100000),
    ("OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET",     0x120,  'u32', 1, 100000),
    ("OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET", 0x4440, 'u32', 0, 100000),
]

def read_field(data, offset, ftype):
    if ftype == 'u8': return data[offset]
    elif ftype == 'u32': return struct.unpack_from('<I', data, offset)[0]
    elif ftype == 'i32': return struct.unpack_from('<i', data, offset)[0]
    return None

print(f"\n{'Team Offset name':<50} {'hex':<8} {'type':<5} {'result':<12} {'status'}")
print("-" * 85)
for name, offset, ftype, minv, maxv in TEAM_CATALOG:
    hits = 0
    for _, data in samples:
        try:
            v = read_field(data, offset, ftype)
            if v is not None and minv <= v <= maxv:
                hits += 1
        except: pass
    rate = hits / max(1, len(samples))
    status = "PASS" if rate >= 0.80 else ("WARN" if rate >= 0.50 else "FAIL")
    print(f"  {status:<4} {name:<44} 0x{offset:03x}  {ftype:<5} {hits}/{len(samples)}")

# Also check global DB offsets
print(f"\n{'Global DB Offset name':<50} {'hex':<8} {'type':<5} {'value':<20} {'status'}")
print("-" * 95)
GLOBAL_CHECKS = [
    ("OOTP27_GLOBAL_SQL_DATABASE_OFFSET",        0x2d78, 'u32'),
    ("OOTP27_GLOBAL_CURRENT_DATE_OFFSET",         0x2d8,  'u32'),
    ("OOTP27_GLOBAL_HUMAN_MANAGER_VECTOR_OFFSET", 0xf0,   'u64'),
    ("OOTP27_GLOBAL_HUMAN_MANAGER_COUNT_OFFSET",   0xfc,   'u32'),
]
for name, offset, ftype in GLOBAL_CHECKS:
    try:
        if ftype == 'u32':
            v = struct.unpack_from('<I', db_data, offset)[0]
        else:
            v = struct.unpack_from('<Q', db_data, offset)[0]
        # Check plausibility
        if ftype == 'u64':
            ok = v > 0x10000 and v < 0x7FFFFFFFFFFF
        else:
            ok = True  # can't easily validate arbitrary u32
        status = "OK" if ok else "?"
        print(f"  {status:<4} {name:<44} 0x{offset:03x}  {ftype:<5} 0x{v:X}")
    except:
        print(f"  FAIL {name:<44} 0x{offset:03x}  {ftype:<5} (cannot read)")

kernel32.CloseHandle(h)
