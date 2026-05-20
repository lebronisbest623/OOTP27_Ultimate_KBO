"""Probe OOTP memory for high-school record fields.

Future split targets: Win32 process discovery, process-memory reads, league/team
memory probes, save-file record loading, and offset scoring/reporting.
"""

import ctypes
import html
import pathlib
import re
import struct
import sys
from ctypes import wintypes

TOOLS_DIR = pathlib.Path(__file__).resolve().parents[2] / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from kbo_product import (  # noqa: E402
    CURRENT_SAVE_PATH_FILE_TEMPLATE,
    DEFAULT_HIGH_SCHOOL_LEAGUE_ID,
    OOTP_EXECUTABLE_NAME,
    league_role_id,
    local_data_path,
)


PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010

TEAM_VECTOR_OFFSET = 0x90
TEAM_COUNT_OFFSET = 0x9C
TEAM_LEAGUE_ID_OFFSET = 0x120
TEAM_ID_OFFSET = 0x4450
TEAM_READABLE_BYTES = 0x4460
LEAGUE_ID_OFFSET = 0x4CC0
LEAGUE_YEAR_OFFSET = 0x44EC
LEAGUE_PHASE_OFFSET = 0x44F0
LEAGUE_PHASE_YEAR_OFFSET = 0x44F4
HIGH_SCHOOL_LEAGUE_ID = league_role_id("high_school_league_id", DEFAULT_HIGH_SCHOOL_LEAGUE_ID)


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)


class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", wintypes.DWORD),
        ("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD),
        ("pcPriClassBase", wintypes.LONG),
        ("dwFlags", wintypes.DWORD),
        ("szExeFile", ctypes.c_char * 260),
    ]


class MODULEENTRY32(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("th32ModuleID", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("GlblcntUsage", wintypes.DWORD),
        ("ProccntUsage", wintypes.DWORD),
        ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
        ("modBaseSize", wintypes.DWORD),
        ("hModule", ctypes.c_void_p),
        ("szModule", ctypes.c_char * 256),
        ("szExePath", ctypes.c_char * 260),
    ]


kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.ReadProcessMemory.argtypes = [
    wintypes.HANDLE,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
]
kernel32.ReadProcessMemory.restype = wintypes.BOOL
kernel32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel32.Process32First.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32)]
kernel32.Process32First.restype = wintypes.BOOL
kernel32.Process32Next.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32)]
kernel32.Process32Next.restype = wintypes.BOOL
kernel32.Module32First.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32)]
kernel32.Module32First.restype = wintypes.BOOL
kernel32.Module32Next.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32)]
kernel32.Module32Next.restype = wintypes.BOOL


def find_ootp_processes():
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == wintypes.HANDLE(-1).value:
        return []
    try:
        entry = PROCESSENTRY32()
        entry.dwSize = ctypes.sizeof(entry)
        rows = []
        if kernel32.Process32First(snapshot, ctypes.byref(entry)):
            while True:
                name = entry.szExeFile.decode(errors="ignore")
                if name.lower() == OOTP_EXECUTABLE_NAME:
                    rows.append(entry.th32ProcessID)
                if not kernel32.Process32Next(snapshot, ctypes.byref(entry)):
                    break
        return rows
    finally:
        kernel32.CloseHandle(snapshot)


def get_module(pid, module_name):
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snapshot == wintypes.HANDLE(-1).value:
        return None
    try:
        entry = MODULEENTRY32()
        entry.dwSize = ctypes.sizeof(entry)
        if kernel32.Module32First(snapshot, ctypes.byref(entry)):
            while True:
                name = entry.szModule.decode(errors="ignore")
                if name.lower() == module_name.lower():
                    return (
                        ctypes.addressof(entry.modBaseAddr.contents),
                        int(entry.modBaseSize),
                        entry.szExePath.decode(errors="ignore"),
                    )
                if not kernel32.Module32Next(snapshot, ctypes.byref(entry)):
                    break
        return None
    finally:
        kernel32.CloseHandle(snapshot)


class ProcessMemory:
    def __init__(self, pid):
        self.pid = pid
        self.handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
        if not self.handle:
            raise OSError(ctypes.get_last_error(), f"OpenProcess failed for pid={pid}")

    def close(self):
        if self.handle:
            kernel32.CloseHandle(self.handle)
            self.handle = None

    def read(self, address, size):
        if not address or address < 0x10000 or size <= 0:
            return None
        buf = ctypes.create_string_buffer(size)
        read = ctypes.c_size_t()
        if not kernel32.ReadProcessMemory(
            self.handle,
            ctypes.c_void_p(address),
            buf,
            size,
            ctypes.byref(read),
        ):
            return None
        if read.value != size:
            return None
        return buf.raw

    def u16(self, address):
        data = self.read(address, 2)
        return struct.unpack("<H", data)[0] if data else None

    def u32(self, address):
        data = self.read(address, 4)
        return struct.unpack("<I", data)[0] if data else None

    def u64(self, address):
        data = self.read(address, 8)
        return struct.unpack("<Q", data)[0] if data else None


def pe_sections(mem, base):
    data = mem.read(base, 0x1000)
    if not data:
        return []
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    nt = mem.read(base + e_lfanew, 0x108)
    if not nt:
        return []
    section_count = struct.unpack_from("<H", nt, 6)[0]
    optional_size = struct.unpack_from("<H", nt, 20)[0]
    section_start = base + e_lfanew + 24 + optional_size
    rows = []
    for index in range(section_count):
        raw = mem.read(section_start + index * 40, 40)
        if not raw:
            continue
        name = raw[:8].rstrip(b"\0").decode(errors="ignore")
        virtual_size, virtual_address = struct.unpack_from("<II", raw, 8)
        characteristics = struct.unpack_from("<I", raw, 36)[0]
        rows.append((name, base + virtual_address, virtual_size, characteristics, virtual_address))
    return rows


def looks_like_global_db(mem, candidate):
    team_count = mem.u32(candidate + TEAM_COUNT_OFFSET)
    if not team_count or team_count < 2 or team_count > 5000:
        return False
    team_vector = mem.u64(candidate + TEAM_VECTOR_OFFSET)
    if not team_vector:
        return False
    for index in range(min(team_count, 5)):
        team_ptr = mem.u64(team_vector + index * 8)
        if not team_ptr:
            return False
        team_id = mem.u32(team_ptr + TEAM_ID_OFFSET)
        league_id = mem.u32(team_ptr + TEAM_LEAGUE_ID_OFFSET)
        if not team_id or team_id > 100000 or not league_id or league_id > 100000:
            return False
    return True


def find_global_db(mem, exe_base):
    for name, address, size, characteristics, virtual_address in pe_sections(mem, exe_base):
        if not (characteristics & 0x80000000):
            continue
        if not (characteristics & 0x40000000):
            continue
        if characteristics & 0x20000000:
            continue
        data = mem.read(address, size)
        if not data:
            continue
        for offset in range(0, len(data) - 8, 8):
            value = struct.unpack_from("<Q", data, offset)[0]
            if 0x10000 <= value <= 0x7FFFFFFFFFFF and looks_like_global_db(mem, value):
                return value, name, virtual_address + offset
    return None, "", 0


def collect_teams(mem, global_db, league_id):
    team_vector = mem.u64(global_db + TEAM_VECTOR_OFFSET)
    team_count = mem.u32(global_db + TEAM_COUNT_OFFSET) or 0
    teams = {}
    for index in range(team_count):
        team_ptr = mem.u64(team_vector + index * 8)
        if not team_ptr:
            continue
        if mem.u32(team_ptr + TEAM_LEAGUE_ID_OFFSET) != league_id:
            continue
        team_id = mem.u32(team_ptr + TEAM_ID_OFFSET)
        if team_id:
            teams[team_id] = team_ptr
    return teams


def load_save_path(pid):
    path = local_data_path(CURRENT_SAVE_PATH_FILE_TEMPLATE.format(pid=pid))
    if path is None:
        return None
    if path.exists():
        text = path.read_text(errors="ignore").strip()
        if text:
            return pathlib.Path(text)
    return None


def load_power_ranking_records(save_path):
    path = save_path / "news" / "html" / "leagues" / "league_203_team_power_rankings_page.html"
    if not path.exists():
        return {}
    text = path.read_text(errors="ignore")
    pattern = re.compile(
        r'href="\.\./teams/team_(\d+)\.html">([^<]+)</a></td>\s*'
        r'<td class="dr">(\d+)</td>\s*'
        r'<td class="dc">[^<]*</td>\s*'
        r'<td class="dr">(\d+)-(\d+)-(\d+)</td>'
    )
    rows = {}
    for match in pattern.finditer(text):
        team_id = int(match.group(1))
        rows[team_id] = {
            "name": html.unescape(match.group(2)),
            "wins": int(match.group(4)),
            "losses": int(match.group(5)),
            "ties": int(match.group(6)),
        }
    return rows


def score_scalar_offsets(mem, teams, records, base_getter, max_offset, label):
    common = [team_id for team_id in records if team_id in teams]
    hits = []
    for width, step, fmt in ((2, 2, "<H"), (4, 4, "<I")):
        for offset in range(0, max_offset, step):
            values = []
            team_ids = []
            for team_id in common:
                base = base_getter(team_id)
                if not base:
                    continue
                data = mem.read(base + offset, width)
                if not data:
                    continue
                value = struct.unpack(fmt, data)[0]
                if value > 250:
                    continue
                values.append(value)
                team_ids.append(team_id)
            if len(values) < 80:
                continue
            for field in ("wins", "losses", "ties"):
                matches = sum(
                    1 for team_id, value in zip(team_ids, values)
                    if records[team_id][field] == value
                )
                if matches >= 70:
                    hits.append({
                        "label": label,
                        "width": width,
                        "offset": offset,
                        "field": field,
                        "matches": matches,
                        "count": len(values),
                        "min": min(values),
                        "max": max(values),
                        "unique": len(set(values)),
                    })
    return hits


def find_team_child_pointer_offsets(mem, teams, common_ids):
    rows = []
    for pointer_offset in range(0, TEAM_READABLE_BYTES, 8):
        pointers = []
        for team_id in common_ids:
            pointer = mem.u64(teams[team_id] + pointer_offset)
            if pointer and 0x10000 <= pointer <= 0x7FFFFFFFFFFF and mem.read(pointer, 0x200):
                pointers.append(pointer)
        if len(pointers) >= 80 and len(set(pointers)) >= 20:
            rows.append((pointer_offset, len(pointers), len(set(pointers))))
    return rows


def print_hits(title, hits, limit=80):
    print(f"\n== {title} ==")
    if not hits:
        print("no hits")
        return
    hits = sorted(hits, key=lambda row: (row["matches"], row["count"], row["unique"]), reverse=True)
    for row in hits[:limit]:
        print(
            f'{row["label"]} {row["field"]} width={row["width"]} '
            f'offset=0x{row["offset"]:x} matches={row["matches"]}/{row["count"]} '
            f'min={row["min"]} max={row["max"]} unique={row["unique"]}'
        )


def main():
    pid = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    if pid == 0:
        processes = find_ootp_processes()
        if len(processes) != 1:
            print(f"Expected one ootp27.exe process, found {processes}")
            return 2
        pid = processes[0]

    save_path = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else load_save_path(pid)
    if not save_path or not save_path.exists():
        print(f"Could not resolve save path for pid={pid}")
        return 3

    records = load_power_ranking_records(save_path)
    if len(records) < 80:
        print(f"Power-ranking oracle missing or too small for save={save_path} rows={len(records)}")
        return 4

    module = get_module(pid, OOTP_EXECUTABLE_NAME)
    if module is None:
        print(f"Could not find {OOTP_EXECUTABLE_NAME} module for pid={pid}")
        return 5
    exe_base, exe_size, exe_path = module

    mem = ProcessMemory(pid)
    try:
        global_db, section_name, section_rva = find_global_db(mem, exe_base)
        if not global_db:
            print("Could not find global DB")
            return 6

        teams = collect_teams(mem, global_db, HIGH_SCHOOL_LEAGUE_ID)
        common = [team_id for team_id in records if team_id in teams]
        print(f"pid={pid}")
        print(f"exe={exe_path} base=0x{exe_base:x} size=0x{exe_size:x}")
        print(f"save={save_path}")
        print(f"global_db=0x{global_db:x} section={section_name} rva=0x{section_rva:x}")
        print(f"high_school_teams_memory={len(teams)} oracle={len(records)} common={len(common)}")

        direct_hits = score_scalar_offsets(
            mem,
            teams,
            records,
            lambda team_id: teams[team_id],
            TEAM_READABLE_BYTES,
            "team",
        )
        print_hits("direct team object fields", direct_hits)

        child_offsets = find_team_child_pointer_offsets(mem, teams, common)
        print(f"\nchild pointer offsets checked={len(child_offsets)}")
        all_child_hits = []
        for pointer_offset, readable_count, unique_count in child_offsets:
            child_hits = score_scalar_offsets(
                mem,
                teams,
                records,
                lambda team_id, po=pointer_offset: mem.u64(teams[team_id] + po),
                0x1200,
                f"team+0x{pointer_offset:x}->",
            )
            if child_hits:
                print(f"child pointer 0x{pointer_offset:x} readable={readable_count} unique={unique_count}")
                print_hits(f"child 0x{pointer_offset:x}", child_hits, limit=20)
                all_child_hits.extend(child_hits)

        print_hits("all child pointer hits", all_child_hits, limit=120)
        return 0
    finally:
        mem.close()


if __name__ == "__main__":
    raise SystemExit(main())
