#!/usr/bin/env python3
"""Generate a machine-readable map of the recompiled game.

Usage:
    python3 tools/map_index.py [--extract build/extract] [--config config/usa/arm9]
                               [--out docs/map-index.json]

The index groups the ARM9 code (main, ITCM, DTCM, overlays 0..34) and the
ARM7 by stable ids, lists every function's boundary (name, address, size,
mode) and the call edges between functions. A group whose purpose is not yet
known is recorded as "unknown" rather than guessed.
"""
import argparse
import json
import os
import re
import bisect

SYM_RE = re.compile(r"^(\S+) kind:(function|label|data)\(([^)]*)\) addr:0x([0-9a-fA-F]+)")
SIZE_RE = re.compile(r"size=0x([0-9a-fA-F]+)")
MODE_RE = re.compile(r"^(arm|thumb|any)")

# Stable group metadata: (subsystem, one-line purpose). Unknown groups are
# filled in below as ("unknown", "unknown (not yet classified)").
GROUP_META = {
    "main": ("core/boot", "ARM9 entry point, main loop, memory, BIOS shims, scheduler and OS services"),
    "itcm": ("core/itcm", "time-critical ARM9 code (IRQ delivery, dispatch, hot paths)"),
    "dtcm": ("core/dtcm", "ARM9 tightly-coupled data"),
    "ov000": ("battle", "combat turn processing and combatant stat updates"),
    "ov002": ("items", "inventory item lookup"),
    "ov024": ("battle", "attack damage calculation"),
    "ov033": ("loading", "background asset loader"),
    "arm7": ("services", "ARM7 services (sound, wireless, IPC, timers, RTC)"),
}


def parse_symbols(path):
    funcs = []
    if not os.path.exists(path):
        return funcs
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            m = SYM_RE.match(line)
            if not m:
                continue
            name, kind, meta, addr = m.group(1), m.group(2), m.group(3), int(m.group(4), 16)
            if kind != "function":
                continue
            sm = SIZE_RE.search(meta)
            size = int(sm.group(1), 16) if sm else 0
            mm = MODE_RE.match(meta)
            mode = mm.group(1) if mm else "arm"
            if mode == "any":
                mode = "arm"
            funcs.append({"name": name, "address": addr, "size": size, "mode": mode})
    return funcs


def load_yaml(path):
    try:
        import yaml
    except ImportError:
        yaml = None
    if yaml is not None:
        with open(path, encoding="utf-8") as f:
            return yaml.safe_load(f)
    # Minimal fallback for the two simple files we need.
    result = {}
    cur = None
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if line.startswith("- "):
                if cur is None:
                    cur = []
                    result.setdefault("_list", []).append(cur)
                # not general; only used for overlays list below via yaml if present
                continue
            k, _, v = line.partition(":")
            if v.strip() == "":
                cur = []
                result[k.strip()] = cur
            else:
                result[k.strip()] = v.strip()
                cur = None
    return result


def module_size_and_base(extract, module):
    if module == "main":
        y = load_yaml(os.path.join(extract, "arm9", "arm9.yaml"))
        base = int(y["base_address"])
        bin_path = os.path.join(extract, "arm9", "arm9.bin")
        size = os.path.getsize(bin_path) if os.path.exists(bin_path) else 0
        return base, size, y
    if module == "itcm":
        y = load_yaml(os.path.join(extract, "arm9", "itcm.yaml"))
        base = int(y["base_address"])
        bin_path = os.path.join(extract, "arm9", "itcm.bin")
        size = os.path.getsize(bin_path) if os.path.exists(bin_path) else 0
        return base, size, y
    if module == "dtcm":
        y = load_yaml(os.path.join(extract, "arm9", "dtcm.yaml"))
        base = int(y["base_address"])
        bin_path = os.path.join(extract, "arm9", "dtcm.bin")
        size = os.path.getsize(bin_path) if os.path.exists(bin_path) else 0
        return base, size, y
    if module == "arm7":
        y = load_yaml(os.path.join(extract, "arm7", "arm7.yaml"))
        base = int(y["base_address"])
        bin_path = os.path.join(extract, "arm7", "arm7.bin")
        size = os.path.getsize(bin_path) if os.path.exists(bin_path) else 0
        return base, size, y
    if module.startswith("ov"):
        ovs = load_yaml(os.path.join(extract, "arm9_overlays", "overlays.yaml"))
        oid = int(module[2:])
        ov = next(o for o in ovs["overlays"] if int(o["id"]) == oid)
        base = int(ov["base_address"])
        size = int(ov["code_size"])
        return base, size, ov
    raise ValueError(module)


def parse_relocs(path):
    edges = []  # (from_addr, kind, to_addr, module_token)
    if not os.path.exists(path):
        return edges
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"^from:0x([0-9a-fA-F]+) kind:(\S+) to:0x([0-9a-fA-F]+) module:(\S+)", line.strip())
            if not m:
                continue
            kind = m.group(2)
            if "call" not in kind:
                continue
            edges.append((int(m.group(1), 16), kind, int(m.group(3), 16), m.group(4)))
    return edges


def containing_function(funcs, addr):
    # funcs sorted by address
    best = None
    for fn in funcs:
        if fn["address"] <= addr < fn["address"] + (fn["size"] or 1):
            best = fn
            break
    # fallback: nearest preceding function
    if best is None:
        prev = None
        for fn in funcs:
            if fn["address"] <= addr:
                prev = fn
            else:
                break
        best = prev
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--extract", default="build/extract")
    ap.add_argument("--config", default="config/usa/arm9")
    ap.add_argument("--out", default="docs/map-index.json")
    args = ap.parse_args()

    extract = args.extract
    config = args.config

    # Group list: main, itcm, dtcm, overlays, arm7.
    modules = ["main", "itcm", "dtcm"] + [f"ov{i:03d}" for i in range(35)] + ["arm7"]

    groups = []
    group_functions = {}  # module -> list of func dicts with global id
    all_functions = []   # list of {id, group, name, address, size, mode}
    fid = 0

    for mod in modules:
        if mod == "arm7":
            sym_path = None
        elif mod in ("main", "itcm", "dtcm"):
            sym_path = os.path.join(config, f"{mod if mod != 'main' else ''}".strip("/") , "symbols.txt") if False else None
        # resolve symbol paths
        if mod == "main":
            sym_path = os.path.join(config, "symbols.txt")
        elif mod in ("itcm", "dtcm"):
            sym_path = os.path.join(config, mod, "symbols.txt")
        elif mod.startswith("ov"):
            oid = int(mod[2:])
            sym_path = os.path.join(config, "overlays", mod, "symbols.txt")
        else:
            sym_path = None

        funcs = parse_symbols(sym_path) if sym_path else []

        # sort by address
        funcs.sort(key=lambda x: x["address"])
        base, size, _ = module_size_and_base(extract, mod)

        subsystem, purpose = GROUP_META.get(mod, ("unknown", "unknown (not yet classified)"))

        gid = f"GRP-ARM9-{mod.upper()}" if mod != "arm7" else "GRP-ARM7"
        if mod.startswith("ov"):
            gid = f"GRP-ARM9-{mod.upper()}"
        if mod == "main":
            gid = "GRP-ARM9-MAIN"
        if mod == "itcm":
            gid = "GRP-ARM9-ITCM"
        if mod == "dtcm":
            gid = "GRP-ARM9-DTCM"

        groups.append({
            "id": gid,
            "module": mod,
            "region": "arm7" if mod == "arm7" else "arm9",
            "subsystem": subsystem,
            "purpose": purpose,
            "classified": subsystem != "unknown",
            "base_address": f"0x{base:08x}",
            "size_bytes": size,
            "function_count": len(funcs),
        })

        id_list = []
        for fn in funcs:
            entry = {
                "id": fid,
                "group": gid,
                "name": fn["name"],
                "address": f"0x{fn['address']:08x}",
                "size": fn["size"],
                "mode": fn["mode"],
            }
            all_functions.append(entry)
            id_list.append((fn, fid))
            fid += 1
        group_functions[mod] = (funcs, id_list, gid)

    # Build call edges.
    # Map module -> funcs list for containment.
    mod_funcs = {}
    for mod in modules:
        if mod == "arm7":
            mod_funcs[mod] = []
            continue
        if mod == "main":
            sp = os.path.join(config, "symbols.txt")
        elif mod in ("itcm", "dtcm"):
            sp = os.path.join(config, mod, "symbols.txt")
        elif mod.startswith("ov"):
            sp = os.path.join(config, "overlays", mod, "symbols.txt")
        else:
            sp = None
        mod_funcs[mod] = parse_symbols(sp) if sp else []
        mod_funcs[mod].sort(key=lambda x: x["address"])

    # function address -> global id lookup per module
    addr_to_id = {}
    for mod in modules:
        # rebuild ids by scanning all_functions for group
        pass

    # Map (module, address) -> global function id
    func_id_lookup = {}
    for entry in all_functions:
        # find module from group id
        pass

    # Easier: maintain mapping during creation. Rebuild from groups.
    # We'll just search all_functions by group+address.
    def find_gid(mod):
        if mod == "arm7":
            return "GRP-ARM7"
        if mod == "main":
            return "GRP-ARM9-MAIN"
        if mod == "itcm":
            return "GRP-ARM9-ITCM"
        if mod == "dtcm":
            return "GRP-ARM9-DTCM"
        return f"GRP-ARM9-{mod.upper()}"

    # Build address->id per module
    module_addr_to_id = {m: {} for m in modules}
    for entry in all_functions:
        # determine module from group id
        pass

    # Instead, walk again with counters is complex; do lookup by (group,address)
    group_addr_to_id = {}
    for entry in all_functions:
        group_addr_to_id.setdefault(entry["group"], {})[entry["address"]] = entry["id"]

    def func_id_for(mod, addr):
        gid = find_gid(mod)
        # exact or containing
        d = group_addr_to_id.get(gid, {})
        if addr in d:
            # but addr stored as hex string
            pass
        # convert addr to hex key
        key = f"0x{addr:08x}"
        if key in d:
            return d[key]
        # containing
        # find in all_functions
        for entry in all_functions:
            if entry["group"] != gid:
                continue
            a = int(entry["address"], 16)
            sz = entry["size"] or 1
            if a <= addr < a + sz:
                return entry["id"]
        return None

    # Actually above group_addr_to_id keys are hex strings already; build correctly:
    # Per-module sorted function arrays for O(log n) containment lookup.
    module_func_arrays = {}
    for mod in modules:
        gid = find_gid(mod)
        arr = sorted((int(e["address"],16), (e["size"] or 1), e["id"]) for e in all_functions if e["group"]==gid)
        addrs = [a for a,_,_ in arr]
        module_func_arrays[mod] = (addrs, arr)

    def func_id_for2(mod, addr):
        if mod not in module_func_arrays:
            return None
        addrs, arr = module_func_arrays[mod]
        i = bisect.bisect_right(addrs, addr) - 1
        if i < 0:
            return None
        a, sz, fid = arr[i]
        if a <= addr < a + sz:
            return fid
        return None

    edges = set()
    for mod in modules:
        if mod == "arm7":
            continue
        if mod == "main":
            reloc_path = os.path.join(config, "relocs.txt")
        elif mod in ("itcm", "dtcm"):
            reloc_path = os.path.join(config, mod, "relocs.txt")
        else:
            reloc_path = os.path.join(config, "overlays", mod, "relocs.txt")
        relocs = parse_relocs(reloc_path)
        funcs = mod_funcs[mod]
        for from_addr, kind, to_addr, to_mod in [(r[0], r[1], r[2], r[3]) for r in relocs]:
            # relocs stored as tuples (from_addr, kind, to_addr, to_mod)? parse returns 4-tuple without kind? check
            pass
        # parse_relocs returns 4-tuple (from_addr, kind, to_addr, to_mod)? We built (from_addr, kind, to_addr, module) yes.
        for fr, kind, to, token in relocs:
            from_id = func_id_for2(mod, fr)
            if from_id is None:
                continue
            # resolve target module candidates from token like "overlay(29)" or "overlays(1,2)"
            cands = []
            if token in ("main", "itcm", "dtcm", "arm7"):
                cands = [token]
            elif token.startswith("overlay") or token.startswith("overlays"):
                import re as _re
                ids = _re.findall(r"\d+", token)
                cands = [f"ov{int(x):03d}" for x in ids]
            else:
                cands = [token]
            for tmod in cands:
                if tmod not in modules:
                    continue
                to_id = func_id_for2(tmod, to)
                if to_id is not None:
                    edges.add((from_id, to_id))

    # Group-level edge summary
    group_edges = {}
    for (a, b) in edges:
        ga = all_functions[a]["group"]
        gb = all_functions[b]["group"]
        if ga == gb:
            continue
        group_edges.setdefault(ga, {}).setdefault(gb, 0)
        group_edges[ga][gb] += 1

    # Attach group edge summary
    for g in groups:
        g["calls_to"] = [{"group": k, "edges": v} for k, v in sorted(group_edges.get(g["id"], {}).items())]

    index = {
        "generated_by": "tools/map_index.py",
        "source_config": config,
        "source_extract": extract,
        "region": "usa",
        "entry_point": "0x02000800",
        "groups": groups,
        "functions": all_functions,
        "function_count": len(all_functions),
        "call_edges": [[a, b] for (a, b) in sorted(edges)],
        "call_edge_count": len(edges),
    }

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(index, f, separators=(",", ":"))
    print(f"wrote {args.out}: {len(groups)} groups, {len(all_functions)} functions, {len(edges)} call edges")


if __name__ == "__main__":
    main()
