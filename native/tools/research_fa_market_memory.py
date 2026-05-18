import argparse
import ctypes
import csv
import os
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path


# Win32 process/memory flags used only by this live-memory research tool.
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
MEM_COMMIT = 0x1000
PAGE_GUARD = 0x100
PAGE_NOACCESS = 0x01
READABLE_PROTECT = {0x02, 0x04, 0x08, 0x20, 0x40, 0x80}

# OOTP player-vector geometry and anchor fields.
PLAYER_SCAN_BYTES = 0x1800
PLAYER_VECTOR_OFFSET = 0x30
PLAYER_ID_OFFSET = 0xB4
PLAYER_AGE_OFFSET = 0x7C

# Player memory fields probed during FA market reverse-engineering.
KNOWN_FIELDS = [
    ("retired_flag", "u8", 0x40),
    ("current_league", "u32", 0x48),
    ("loan_league", "u32", 0x54),
    ("current_team", "u32", 0x58),
    ("active_team", "u32", 0x60),
    ("loan_team", "u32", 0x68),
    ("nation", "u32", 0x6C),
    ("age", "u16", 0x7C),
    ("player_id", "u32", 0xB4),
    ("restricted", "u8", 0x83B),
    ("restricted2", "u8", 0x83C),
    ("loan_active", "u8", 0x842),
    ("loan_cleared", "u8", 0x843),
    ("dfa", "u8", 0x85B),
    ("injury_active", "u8", 0x874),
    ("injury_days", "u16", 0x876),
    ("military_exempt", "u8", 0x895),
    ("military_active", "u8", 0x896),
    ("contract_level", "u8", 0x8A8),
    ("position_group", "u8", 0xC78),
    ("position_role", "u8", 0xC79),
    ("overall_value", "u16", 0xCD2),
    ("talent_value", "u16", 0xCD4),
    ("ratings_value", "u16", 0xCD6),
    ("career_value", "u16", 0xCD8),
    ("draft_league", "u32", 0xD70),
    ("fa_demand", "i32", 0xD84),
    ("draft_class", "u8", 0xDB1),
    ("draft_subtype", "u8", 0xDB2),
    ("draft_eligible", "u8", 0xDB5),
    ("draft_extra", "u8", 0xDB6),
    ("loan_dirty", "u8", 0xF69),
    ("generation_flags", "u8", 0xFAA),
    ("generation_context", "u8", 0xFAC),
    ("generation_grade", "u8", 0xFAD),
    ("generation_special", "u8", 0xFB2),
]


class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p),
        ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", ctypes.c_ulong),
        ("PartitionId", ctypes.c_ushort),
        ("RegionSize", ctypes.c_size_t),
        ("State", ctypes.c_ulong),
        ("Protect", ctypes.c_ulong),
        ("Type", ctypes.c_ulong),
    ]


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
kernel32.OpenProcess.restype = ctypes.c_void_p
kernel32.ReadProcessMemory.argtypes = [
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
]
kernel32.ReadProcessMemory.restype = ctypes.c_int
kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
kernel32.CloseHandle.restype = ctypes.c_int
kernel32.VirtualQueryEx.argtypes = [
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.POINTER(MEMORY_BASIC_INFORMATION),
    ctypes.c_size_t,
]
kernel32.VirtualQueryEx.restype = ctypes.c_size_t


def read_exact(handle, address, size):
    if address < 0x10000 or size <= 0:
        return None
    buf = (ctypes.c_ubyte * size)()
    read = ctypes.c_size_t(0)
    ok = kernel32.ReadProcessMemory(
        handle,
        ctypes.c_void_p(address),
        ctypes.byref(buf),
        size,
        ctypes.byref(read),
    )
    if not ok or read.value != size:
        return None
    return bytes(buf)


def u8(data, off):
    return data[off]


def u16(data, off):
    return struct.unpack_from("<H", data, off)[0]


def u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def i32(data, off):
    return struct.unpack_from("<i", data, off)[0]


def u64(data, off):
    return struct.unpack_from("<Q", data, off)[0]


READERS = {
    "u8": u8,
    "u16": u16,
    "u32": u32,
    "i32": i32,
}


def parse_ootp_export(path):
    text = Path(path).read_text(encoding="utf-8-sig", errors="replace").splitlines()
    header = None
    rows = []
    in_free_agents = False
    for line in text:
        if line.startswith("//id,"):
            header = line[2:]
            continue
        if line.strip() == "// FREE AGENTS":
            in_free_agents = True
            continue
        if not in_free_agents or not line or line.startswith("//"):
            continue
        rows.append(line)
    if header is None:
        raise RuntimeError(f"export header not found: {path}")

    headers = next(csv.reader([header]))
    first_index = {}
    for idx, name in enumerate(headers):
        first_index.setdefault(name.strip(), idx)

    wanted = ["id", "LastName", "FirstName", "NationalityID", "Position", "ML Service", "pro years"]
    missing = [name for name in wanted if name not in first_index]
    if missing:
        raise RuntimeError(f"export missing columns: {missing}")

    result = {}
    for line in rows:
        parts = next(csv.reader([line]))
        if not parts or not parts[0].strip().isdigit():
            continue
        pid = int(parts[first_index["id"]])
        result[pid] = {
            "id": pid,
            "name": f"{parts[first_index['FirstName']]} {parts[first_index['LastName']]}".strip(),
            "nation": int(parts[first_index["NationalityID"]] or 0),
            "position": parts[first_index["Position"]],
            "ml_service": int(parts[first_index["ML Service"]] or 0),
            "pro_years": int(parts[first_index["pro years"]] or 0),
        }
    return result


def parse_our_csv(path):
    result = {}
    with Path(path).open(newline="", encoding="utf-8-sig", errors="replace") as f:
        for row in csv.DictReader(f):
            raw = row.get("player_id", "")
            if raw.isdigit():
                pid = int(raw)
                result[pid] = row
    return result


def value_from(data, typ, off):
    if off < 0 or off >= len(data):
        return None
    if typ == "u8" and off <= len(data) - 1:
        return u8(data, off)
    if typ == "u16" and off <= len(data) - 2:
        return u16(data, off)
    if typ == "u32" and off <= len(data) - 4:
        return u32(data, off)
    if typ == "i32" and off <= len(data) - 4:
        return i32(data, off)
    return None


def field_map(data):
    return {name: value_from(data, typ, off) for name, typ, off in KNOWN_FIELDS}


def classify_id(pid, export_ids, our_ids):
    in_export = pid in export_ids
    in_ours = pid in our_ids
    if in_export and in_ours:
        return "overlap"
    if in_export:
        return "ootp_only"
    if in_ours:
        return "ours_only"
    return "other"


def find_players(handle, global_ptr, selected_ids):
    slot = read_exact(handle, global_ptr + PLAYER_VECTOR_OFFSET, 0x10)
    if slot is None:
        raise RuntimeError("cannot read player vector slot")
    vector = u64(slot, 0)
    count = i32(slot, 0x0C)
    if vector < 0x10000 or count <= 0 or count > 300000:
        raise RuntimeError(f"bad player vector vector=0x{vector:X} count={count}")
    raw_ptrs = read_exact(handle, vector, count * 8)
    if raw_ptrs is None:
        raise RuntimeError(f"cannot read player pointer vector: 0x{vector:X} count={count}")

    ptr_to_id = {}
    id_to_ptr = {}
    selected = {}
    plausible = 0
    teamless = 0
    for index in range(count):
        ptr = u64(raw_ptrs, index * 8)
        if ptr < 0x10000:
            continue
        header = read_exact(handle, ptr + PLAYER_AGE_OFFSET, PLAYER_ID_OFFSET - PLAYER_AGE_OFFSET + 4)
        if header is None:
            continue
        age = u16(header, 0)
        pid = u32(header, PLAYER_ID_OFFSET - PLAYER_AGE_OFFSET)
        if pid == 0 or pid > 200000000 or age > 90:
            continue
        plausible += 1
        ptr_to_id[ptr] = pid
        id_to_ptr[pid] = ptr
        if pid in selected_ids:
            data = read_exact(handle, ptr, PLAYER_SCAN_BYTES)
            if data is not None:
                selected[pid] = {
                    "index": index,
                    "ptr": ptr,
                    "bytes": data,
                    "fields": field_map(data),
                }
        if pid not in selected_ids:
            small = read_exact(handle, ptr + 0x58, 4)
            if small is not None and u32(small, 0) == 0:
                teamless += 1
    return {
        "vector": vector,
        "count": count,
        "plausible": plausible,
        "teamless": teamless,
        "ptr_to_id": ptr_to_id,
        "id_to_ptr": id_to_ptr,
        "selected": selected,
    }


def format_counter(values, limit=8):
    items = Counter(values).most_common(limit)
    return " ".join(f"{value}:{count}" for value, count in items)


def summarize_known_fields(samples, export_ids, our_ids):
    groups = defaultdict(list)
    for pid, sample in samples.items():
        groups[classify_id(pid, export_ids, our_ids)].append(sample)

    lines = []
    for name, _, _ in KNOWN_FIELDS:
        parts = []
        for label in ["overlap", "ootp_only", "ours_only"]:
            vals = [s["fields"].get(name) for s in groups[label] if s["fields"].get(name) is not None]
            parts.append(f"{label}=[{format_counter(vals)}]")
        lines.append((name, " | ".join(parts)))
    return lines


def scan_separating_offsets(samples, positive_ids, negative_ids, typ, max_distinct=16):
    pos = [samples[pid]["bytes"] for pid in positive_ids if pid in samples]
    neg = [samples[pid]["bytes"] for pid in negative_ids if pid in samples]
    if not pos or not neg:
        return []

    size = {"u8": 1, "u16": 2, "u32": 4, "i32": 4}[typ]
    reader = READERS[typ]
    results = []
    for off in range(0, PLAYER_SCAN_BYTES - size + 1):
        pos_vals = [reader(data, off) for data in pos]
        neg_vals = [reader(data, off) for data in neg]
        pos_set = set(pos_vals)
        neg_set = set(neg_vals)
        if pos_set.isdisjoint(neg_set) and len(pos_set) <= max_distinct and len(neg_set) <= max_distinct:
            results.append({
                "typ": typ,
                "off": off,
                "pos_distinct": len(pos_set),
                "neg_distinct": len(neg_set),
                "pos_values": format_counter(pos_vals, 8),
                "neg_values": format_counter(neg_vals, 8),
            })
            continue

        best_value = None
        best_score = None
        for value in pos_set | neg_set:
            tp = sum(1 for v in pos_vals if v == value)
            fp = sum(1 for v in neg_vals if v == value)
            if tp == 0:
                continue
            precision = tp / max(1, tp + fp)
            recall = tp / len(pos_vals)
            score = precision * recall
            if best_score is None or score > best_score:
                best_score = score
                best_value = (value, tp, fp, precision, recall)
        if best_value and best_score and best_score >= 0.80:
            value, tp, fp, precision, recall = best_value
            results.append({
                "typ": typ,
                "off": off,
                "pos_distinct": len(pos_set),
                "neg_distinct": len(neg_set),
                "pos_values": f"best={value} tp={tp} recall={recall:.3f}",
                "neg_values": f"fp={fp} precision={precision:.3f}",
            })
    results.sort(key=lambda r: (r["pos_distinct"] + r["neg_distinct"], r["off"]))
    return results


def iter_regions(handle, min_addr=0x10000, max_addr=0x7FFFFFFFFFFF):
    addr = min_addr
    mbi = MEMORY_BASIC_INFORMATION()
    mbi_size = ctypes.sizeof(mbi)
    while addr < max_addr:
        result = kernel32.VirtualQueryEx(handle, ctypes.c_void_p(addr), ctypes.byref(mbi), mbi_size)
        if not result:
            addr += 0x10000
            continue
        base = int(mbi.BaseAddress or 0)
        size = int(mbi.RegionSize or 0)
        protect = int(mbi.Protect)
        if size <= 0:
            addr += 0x10000
            continue
        if mbi.State == MEM_COMMIT and not (protect & PAGE_GUARD) and not (protect & PAGE_NOACCESS):
            base_protect = protect & 0xFF
            if base_protect in READABLE_PROTECT:
                yield base, size, protect, int(mbi.Type)
        addr = base + size


def read_region_chunks(handle, base, size, chunk_size=16 * 1024 * 1024):
    done = 0
    while done < size:
        take = min(chunk_size, size - done)
        data = read_exact(handle, base + done, take)
        if data is not None:
            yield base + done, data
        done += take


def scan_pointer_segments(handle, ptr_to_id, export_ids, our_ids, max_region_mb):
    pointer_set = set(ptr_to_id)
    if not pointer_set:
        return []
    segments = []
    region_limit = max_region_mb * 1024 * 1024 if max_region_mb else None
    for base, size, protect, mem_type in iter_regions(handle):
        if region_limit and size > region_limit:
            continue
        for chunk_base, data in read_region_chunks(handle, base, size):
            words = len(data) // 8
            i = 0
            while i < words:
                ptr = u64(data, i * 8)
                if ptr not in pointer_set:
                    i += 1
                    continue
                start = i
                ids = []
                while i < words:
                    ptr = u64(data, i * 8)
                    if ptr not in pointer_set:
                        break
                    ids.append(ptr_to_id[ptr])
                    i += 1
                    if len(ids) >= 5000:
                        break
                if len(ids) >= 5:
                    export_hits = sum(1 for pid in ids if pid in export_ids)
                    ours_only_hits = sum(1 for pid in ids if pid in our_ids and pid not in export_ids)
                    if export_hits >= 3 or ours_only_hits >= 3:
                        segments.append({
                            "addr": chunk_base + start * 8,
                            "count": len(ids),
                            "export_hits": export_hits,
                            "ours_only_hits": ours_only_hits,
                            "first_ids": ids[:16],
                            "protect": protect,
                            "type": mem_type,
                        })
                i += 1
    segments.sort(key=lambda s: (s["export_hits"], -s["ours_only_hits"], s["count"]), reverse=True)
    return segments


def search_exact_sequences(handle, samples, export_order, max_region_mb):
    ptr_seq = []
    for pid in export_order:
        if pid in samples:
            ptr_seq.append(samples[pid]["ptr"])
        if len(ptr_seq) >= 8:
            break
    id_seq = export_order[:8]
    needles = []
    if len(ptr_seq) >= 4:
        needles.append(("ptr_export_first", b"".join(struct.pack("<Q", p) for p in ptr_seq)))
    if len(id_seq) >= 4:
        needles.append(("id_export_first", b"".join(struct.pack("<I", pid) for pid in id_seq)))
    if not needles:
        return []

    hits = []
    region_limit = max_region_mb * 1024 * 1024 if max_region_mb else None
    for base, size, protect, mem_type in iter_regions(handle):
        if region_limit and size > region_limit:
            continue
        overlap = max(len(needle) for _, needle in needles) - 1
        previous = b""
        previous_base = 0
        for chunk_base, data in read_region_chunks(handle, base, size):
            haystack = previous + data
            hay_base = previous_base if previous else chunk_base
            for name, needle in needles:
                pos = haystack.find(needle)
                while pos >= 0:
                    hits.append({
                        "name": name,
                        "addr": hay_base + pos,
                        "protect": protect,
                        "type": mem_type,
                    })
                    pos = haystack.find(needle, pos + 1)
            if overlap > 0:
                previous = haystack[-overlap:]
                previous_base = hay_base + len(haystack) - len(previous)
    return hits


def write_samples_csv(path, samples, export_rows, our_rows):
    fields = ["id", "label", "ptr", "index", "ootp_name", "our_name"] + [name for name, _, _ in KNOWN_FIELDS]
    with Path(path).open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        export_ids = set(export_rows)
        our_ids = set(our_rows)
        for pid in sorted(samples):
            sample = samples[pid]
            row = {
                "id": pid,
                "label": classify_id(pid, export_ids, our_ids),
                "ptr": f"0x{sample['ptr']:X}",
                "index": sample["index"],
                "ootp_name": export_rows.get(pid, {}).get("name", ""),
                "our_name": our_rows.get(pid, {}).get("name", ""),
            }
            row.update(sample["fields"])
            writer.writerow(row)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--global-ptr", type=lambda s: int(s, 0), required=True)
    parser.add_argument("--export-csv", required=True)
    parser.add_argument("--ours-csv", required=True)
    parser.add_argument("--out-dir", default=".")
    parser.add_argument("--scan-regions", action="store_true")
    parser.add_argument("--max-region-mb", type=int, default=256)
    args = parser.parse_args()

    export_rows = parse_ootp_export(args.export_csv)
    our_rows = parse_our_csv(args.ours_csv)
    export_ids = set(export_rows)
    our_ids = set(our_rows)
    selected_ids = export_ids | our_ids

    handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, args.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        found = find_players(handle, args.global_ptr, selected_ids)
        samples = found["selected"]
        out_dir = Path(args.out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        write_samples_csv(out_dir / "fa_memory_research_samples.csv", samples, export_rows, our_rows)

        found_export = export_ids & set(samples)
        found_ours = our_ids & set(samples)
        print(f"export_count={len(export_ids)} ours_count={len(our_ids)} selected={len(selected_ids)}")
        print(
            "player_vector "
            f"offset=0x{PLAYER_VECTOR_OFFSET:X} vector=0x{found['vector']:X} "
            f"count={found['count']} plausible={found['plausible']} "
            f"selected_found={len(samples)} export_found={len(found_export)} ours_found={len(found_ours)}"
        )
        print(
            "sets "
            f"overlap={len(export_ids & our_ids)} "
            f"ootp_only={len(export_ids - our_ids)} "
            f"ours_only={len(our_ids - export_ids)}"
        )

        print("\nknown_fields")
        for name, summary in summarize_known_fields(samples, export_ids, our_ids):
            print(f"{name}: {summary}")

        comparisons = [
            ("export_vs_ours_only", export_ids, our_ids - export_ids),
            ("ootp_only_vs_ours_only", export_ids - our_ids, our_ids - export_ids),
            ("overlap_vs_ours_only", export_ids & our_ids, our_ids - export_ids),
        ]
        offset_rows = []
        for label, pos_ids, neg_ids in comparisons:
            print(f"\nseparating_offsets {label} pos={len(pos_ids & set(samples))} neg={len(neg_ids & set(samples))}")
            for typ in ["u8", "u16", "u32", "i32"]:
                rows = scan_separating_offsets(samples, pos_ids, neg_ids, typ)
                for row in rows[:12]:
                    row["comparison"] = label
                    offset_rows.append(row)
                    print(
                        f"{typ} off=0x{row['off']:03X} "
                        f"pos_d={row['pos_distinct']} neg_d={row['neg_distinct']} "
                        f"pos={row['pos_values']} neg={row['neg_values']}"
                    )

        with (out_dir / "fa_memory_research_offsets.csv").open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(
                f,
                fieldnames=["comparison", "typ", "off", "pos_distinct", "neg_distinct", "pos_values", "neg_values"],
            )
            writer.writeheader()
            writer.writerows(offset_rows)

        print("\nsamples ootp_only")
        for pid in sorted(export_ids - our_ids)[:20]:
            if pid not in samples:
                print(f"{pid}: not_found")
                continue
            f = samples[pid]["fields"]
            print(
                f"{pid} {export_rows[pid]['name']} ptr=0x{samples[pid]['ptr']:X} "
                f"team={f['current_team']} league={f['current_league']} draft={f['draft_league']} "
                f"nation={f['nation']} age={f['age']} demand={f['fa_demand']} "
                f"retired={f['retired_flag']} level={f['contract_level']} "
                f"draft_flags={f['draft_class']}/{f['draft_subtype']}/{f['draft_eligible']}/{f['draft_extra']} "
                f"gen={f['generation_flags']}/{f['generation_context']}/{f['generation_grade']}/{f['generation_special']}"
            )

        print("\nsamples ours_only")
        for pid in sorted(our_ids - export_ids)[:20]:
            if pid not in samples:
                print(f"{pid}: not_found")
                continue
            f = samples[pid]["fields"]
            print(
                f"{pid} {our_rows[pid].get('name','')} ptr=0x{samples[pid]['ptr']:X} "
                f"team={f['current_team']} league={f['current_league']} draft={f['draft_league']} "
                f"nation={f['nation']} age={f['age']} demand={f['fa_demand']} "
                f"retired={f['retired_flag']} level={f['contract_level']} "
                f"draft_flags={f['draft_class']}/{f['draft_subtype']}/{f['draft_eligible']}/{f['draft_extra']} "
                f"gen={f['generation_flags']}/{f['generation_context']}/{f['generation_grade']}/{f['generation_special']}"
            )

        if args.scan_regions:
            print("\nexact_sequence_hits")
            sequence_hits = search_exact_sequences(handle, samples, list(export_rows.keys()), args.max_region_mb)
            for hit in sequence_hits[:20]:
                print(f"{hit['name']} addr=0x{hit['addr']:X} protect=0x{hit['protect']:X} type=0x{hit['type']:X}")
            if not sequence_hits:
                print("none")

            print("\npointer_segments")
            segments = scan_pointer_segments(handle, found["ptr_to_id"], export_ids, our_ids, args.max_region_mb)
            segment_path = out_dir / "fa_memory_research_segments.csv"
            with segment_path.open("w", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(
                    f,
                    fieldnames=["addr", "count", "export_hits", "ours_only_hits", "first_ids", "protect", "type"],
                )
                writer.writeheader()
                for segment in segments[:200]:
                    row = dict(segment)
                    row["addr"] = f"0x{row['addr']:X}"
                    row["first_ids"] = " ".join(str(pid) for pid in row["first_ids"])
                    row["protect"] = f"0x{row['protect']:X}"
                    row["type"] = f"0x{row['type']:X}"
                    writer.writerow(row)
            for segment in segments[:30]:
                print(
                    f"addr=0x{segment['addr']:X} count={segment['count']} "
                    f"export={segment['export_hits']} ours_only={segment['ours_only_hits']} "
                    f"first={' '.join(str(pid) for pid in segment['first_ids'])}"
                )
            if not segments:
                print("none")

        print(f"\nwrote={out_dir}")
    finally:
        kernel32.CloseHandle(handle)


if __name__ == "__main__":
    main()
