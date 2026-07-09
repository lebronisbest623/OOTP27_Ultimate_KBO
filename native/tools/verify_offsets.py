"""Quick player offset verification for a running OOTP process."""
import ctypes
from ctypes import wintypes
import struct

PID = 37764
GLOBAL_DB_PTR = 0x6801B2C0  # from KBOFix log
VECTOR_OFFSET = 0x30         # from KBOFix log
COUNT_DELTA = 0x0c           # OOTP27_GLOBAL_VECTOR_COUNT_DELTA
SAMPLE_COUNT = 80

# Windows API
kernel32 = ctypes.windll.kernel32
PROCESS_VM_READ = 0x0010
h = kernel32.OpenProcess(PROCESS_VM_READ, False, PID)
if not h:
    print("OpenProcess failed")
    exit(1)

def read_mem(addr, size):
    buf = (ctypes.c_byte * size)()
    read = ctypes.c_size_t()
    if kernel32.ReadProcessMemory(h, addr, buf, size, ctypes.byref(read)):
        n = read.value
        return bytearray(buf)[:n]
    return None

# Read player vector pointer and count from global DB
vec_slot = read_mem(GLOBAL_DB_PTR + VECTOR_OFFSET, 16)
if not vec_slot:
    print("Cannot read vector slot from global DB")
    exit(1)

player_vector = struct.unpack_from('<Q', vec_slot, 0)[0]
player_count = struct.unpack_from('<i', vec_slot, COUNT_DELTA)[0]
print(f"Player vector: 0x{player_vector:X}  count: {player_count}")

# Read player pointers
SAMPLE = min(SAMPLE_COUNT, player_count)
ptr_data = read_mem(player_vector, SAMPLE * 8)
if not ptr_data:
    print("Cannot read player pointers")
    exit(1)

player_ptrs = struct.unpack_from(f'<{SAMPLE}Q', ptr_data, 0)

# Collect valid player data
PLAYER_BYTES = 0x1800
samples = []
for pp in player_ptrs:
    if pp == 0:
        continue
    data = read_mem(pp, PLAYER_BYTES)
    if data:
        # Validate: check id and age
        pid_val = struct.unpack_from('<I', data, 0xb4)[0]
        age = struct.unpack_from('<H', data, 0x7c)[0]
        if pid_val > 0 and pid_val < 200000000 and 15 <= age <= 65:
            samples.append((pp, data))
    if len(samples) >= 50:
        break

print(f"Valid samples: {len(samples)}")

# Offset catalog to verify
CATALOG = [
    ("OOTP27_PLAYER_ID_OFFSET",                 0x0b4, 'u32', 1, 199999999, True),
    ("OOTP27_PLAYER_AGE_OFFSET",                0x07c, 'u16', 15, 65, True),
    ("OOTP27_PLAYER_NATION_ID_OFFSET",          0x06c, 'u32', 1, 1000, True),
    ("OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET",    0x058, 'u32', 0, 100000, True),
    ("OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET",     0x060, 'u32', 0, 100000, True),
    ("OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET",   0x05c, 'u32', 0, 100000, True),
    ("OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET",  0x048, 'u32', 0, 100000, True),
    ("OOTP27_PLAYER_ORIGINAL_LEAGUE_ID_OFFSET", 0x04c, 'u32', 0, 100000, True),
    ("OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET",    0xd70, 'u32', 0, 100000, True),
    ("OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET",0x8a8, 'u8',  0, 10, True),
    ("OOTP27_PLAYER_CONTRACT_STATUS_OFFSET",    0x8d0, 'u32', 0, 20, True),
    ("OOTP27_PLAYER_RETIRED_FLAG_OFFSET",       0x040, 'u8',  0, 1, False),
    ("OOTP27_PLAYER_DFA_FLAG_OFFSET",           0x85b, 'u8',  0, 1, False),
    ("OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET",    0x83b, 'u8',  0, 1, False),
    ("OOTP27_PLAYER_INJURY_ACTIVE_OFFSET",      0x874, 'u8',  0, 1, False),
    ("OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET",  0x87c, 'u32', 0, 100000000, True),
    ("OOTP27_PLAYER_ARBITRATION_REQUEST_OFFSET",0x880, 'u32', 0, 100000000, True),
    ("OOTP27_PLAYER_OVERALL_VALUE_OFFSET",      0xcd2, 'i16', -200, 5000, True),
    ("OOTP27_PLAYER_TALENT_VALUE_OFFSET",       0xcd4, 'i16', -200, 5000, True),
    ("OOTP27_PLAYER_POSITION_GROUP_OFFSET",     0xc78, 'u8',  1, 10, True),
]

def read_field(data, offset, ftype):
    if ftype == 'u8':
        return data[offset]
    elif ftype == 'u16':
        return struct.unpack_from('<H', data, offset)[0]
    elif ftype == 'i16':
        return struct.unpack_from('<h', data, offset)[0]
    elif ftype == 'u32':
        return struct.unpack_from('<I', data, offset)[0]
    elif ftype == 'i32':
        return struct.unpack_from('<i', data, offset)[0]
    return None

# Search for correct offset
def find_offset(samples, expected_offset, ftype, min_val, max_val):
    if ftype == 'u8': width = 1
    elif ftype in ('u16','i16'): width = 2
    else: width = 4

    best = []
    lo = max(0, expected_offset - 0x80)
    hi = min(0x1800 - width, expected_offset + 0x80)

    for off in range(lo, hi + 1, width):
        if off == expected_offset:
            continue
        hits = 0
        eligible = 0
        for _, data in samples:
            if off + width <= len(data):
                eligible += 1
                try:
                    v = read_field(data, off, ftype)
                    if v is not None and min_val <= v <= max_val:
                        hits += 1
                except:
                    pass
        if hits > 0:
            best.append((off, hits, eligible))

    best.sort(key=lambda x: -x[1])
    return best[:3]

print(f"{'Offset name':<52} {'hex':<8} {'type':<5} {'result':<12} {'status'}")
print("-" * 95)

failed = []
for name, offset, ftype, minv, maxv, scannable in CATALOG:
    hits = 0
    eligible = 0
    for _, data in samples:
        if offset < len(data):
            eligible += 1
            try:
                v = read_field(data, offset, ftype)
                if v is not None and minv <= v <= maxv:
                    hits += 1
            except:
                pass

    rate = hits / max(1, eligible)
    hex_off = f"0x{offset:03x}"
    result = f"{hits}/{eligible}"

    if rate >= 0.80:
        status = "PASS"
    elif rate >= 0.50:
        status = "WARN"
    else:
        status = "FAIL"

    print(f"  {status:<4} {name:<45} {hex_off:<8} {ftype:<5} {result:<12}", end="")

    if status == "FAIL" and scannable:
        candidates = find_offset(samples, offset, ftype, minv, maxv)
        if candidates:
            best = candidates[0]
            print(f" -> candidate: 0x{best[0]:03x} ({best[1]}/{best[2]})", end="")
            failed.append((name, offset, best[0], best[1], best[2]))
    print()

print(f"\nSummary: {len(CATALOG)} offsets checked")
if failed:
    print(f"\nFailed offsets with candidates:")
    for name, old_off, new_off, hits, eligible in failed:
        print(f"  #define {name:<50} 0x{old_off:03x}u -> 0x{new_off:03x}u  ({hits}/{eligible})")

kernel32.CloseHandle(h)
