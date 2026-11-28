#!/usr/bin/env python3
"""The options model: declare once, check everywhere.

`docs/options.json` is the declaration. This tool has two modes:

  python3 tools/options_model.py          # check (exit 1 on any drift)
  python3 tools/options_model.py --write  # regenerate docs/OPTIONS.md's table

The checks enforce the M3 contract:

  * every option in code (settings_load / settings_save in
    runtime/frontend_sdl.c) is either in the model or in
    `non_enhancement_keys` with a reason — no option exists in code only;
  * every option that changes the picture is kept in the bug report's
    display.txt (report_display's KEEP list), so a report replays with the
    settings that changed what the game considered visible;
  * every option with a menu_label is present in runtime/menu_items.c (the one
    list both menus are built from), and every other one gives a reason;
  * the maturity levels each carry entry/exit criteria, every experimental
    option has a recorded decision with exit criteria, and the README's
    experimental list covers exactly the experimental options;
  * docs/OPTIONS.md's generated table, the local settings.ini (when there is
    one) and the golden corpus' option map agree with the model.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODEL = ROOT / "docs" / "options.json"
OPTIONS_MD = ROOT / "docs" / "OPTIONS.md"
FRONTEND = ROOT / "runtime" / "frontend_sdl.c"
MENU_ITEMS = ROOT / "runtime" / "menu_items.c"
README = ROOT / "README.md"
SETTINGS = ROOT / "settings.ini"
CORPUS = ROOT / "tests" / "golden" / "corpus.json"

BEGIN = "<!-- options:begin (generated from docs/options.json by tools/options_model.py) -->"
END = "<!-- options:end -->"


def load_model() -> dict:
    return json.loads(MODEL.read_text(encoding="utf-8"))


def function_body(source: str, name: str) -> str:
    start = source.index(name)
    # the next top-level function definition after the opening brace
    rest = source[start:]
    match = re.search(r"\n(?:static\s+)?(?:void|int|char|float|double)\s+\w+\s*\(", rest[1:])
    # fall back to the end of file for the last function
    return rest[: match.start() + 1] if match else rest


def code_keys() -> tuple[set[str], set[str]]:
    source = FRONTEND.read_text(encoding="utf-8")
    load = function_body(source, "static void settings_load")
    save = function_body(source, "static void settings_save")
    read = set(re.findall(r'!strcmp\(key, "([a-z0-9_]+)"\)', load))
    written = set(re.findall(r'fprintf\(f, "([a-z0-9_]+) = ', save))
    return read, written


def display_keys() -> set[str]:
    source = FRONTEND.read_text(encoding="utf-8")
    body = function_body(source, "static void report_display")
    match = re.search(r"KEEP\[\]\s*=\s*\{(.*?)\};", body, re.S)
    if not match:
        raise SystemExit("cannot find report_display's KEEP list")
    return set(re.findall(r'"([a-z0-9_]+)"', match.group(1)))


def menu_source() -> str:
    return MENU_ITEMS.read_text(encoding="utf-8")


def experimental_section() -> str:
    text = README.read_text(encoding="utf-8")
    start = text.index("**Experimental options**")
    end = text.find("\n**", start + 10)
    return text[start:] if end < 0 else text[start:end]


def render_table(model: dict) -> str:
    lines = [
        "| id | settings key | default | maturity | interacts with | in bug report | rollback |",
        "|---|---|---|---|---|---|---|",
    ]
    for opt in model["options"]:
        interacts = ", ".join(f"`{i}`" for i in opt["interacts_with"]) or "—"
        keeps = "yes" if opt["affects_picture"] else "no"
        lines.append(
            f"| `{opt['id']}` | `{opt['settings_key']}` | `{opt['default']}` | "
            f"{opt['maturity']} | {interacts} | {keeps} | {opt['rollback']} |"
        )
    return "\n".join(lines)


def check() -> list[str]:
    model = load_model()
    errors: list[str] = []
    options = model["options"]
    ids = [o["id"] for o in options]
    keys = [o["settings_key"] for o in options]

    # --- the model itself
    for label, values in (("id", ids), ("settings key", keys)):
        if len(values) != len(set(values)):
            errors.append(f"duplicate {label} in docs/options.json")
    levels = model["maturity_levels"]
    for opt in options:
        where = f"option {opt['id']}"
        for field in ("maturity", "rollback", "affects_picture"):
            if field not in opt:
                errors.append(f"{where}: missing {field}")
        if opt.get("maturity") not in levels:
            errors.append(f"{where}: unknown maturity {opt.get('maturity')!r}")
        if not str(opt.get("rollback", "")).strip():
            errors.append(f"{where}: empty rollback")
        if not isinstance(opt.get("affects_picture"), bool):
            errors.append(f"{where}: affects_picture must be a boolean")
        if ("menu_label" in opt) == ("menu_absent_reason" in opt):
            errors.append(f"{where}: give exactly one of menu_label / menu_absent_reason")
    for name, level in levels.items():
        for field in ("meaning", "entry", "exit"):
            if not level.get(field):
                errors.append(f"maturity {name}: missing {field} criteria")

    # --- the code
    read, written = code_keys()
    declared = set(keys)
    extras = model["non_enhancement_keys"]
    overlap = declared & set(extras)
    for key in sorted(overlap):
        errors.append(f"settings key {key} is both modelled and in non_enhancement_keys")
    accounted = declared | (set(extras) - overlap)
    for key in sorted(read - accounted):
        errors.append(f"settings_load reads {key!r}: not in the model or non_enhancement_keys")
    for key in sorted(written - accounted):
        errors.append(f"settings_save writes {key!r}: not in the model or non_enhancement_keys")
    for key in sorted(declared - read):
        errors.append(f"model option {key!r} is never read by settings_load")
    for key in sorted(declared - written):
        errors.append(f"model option {key!r} is never written by settings_save")
    for key in sorted(set(extras) - (read | written)):
        errors.append(f"non_enhancement_keys lists {key!r}, which the code does not use")

    # --- replay metadata
    kept = display_keys()
    for opt in options:
        if opt["affects_picture"] and opt["settings_key"] not in kept:
            errors.append(
                f"option {opt['id']} changes the picture but {opt['settings_key']!r} "
                "is missing from report_display's KEEP list (reports would not replay it)"
            )

    # --- the menu
    menus = menu_source()
    for opt in options:
        label = opt.get("menu_label")
        if label is not None and label not in menus:
            errors.append(f"option {opt['id']}: menu label {label!r} is not in runtime/menu_items.c")

    # --- maturity bookkeeping
    experimental = {o["id"] for o in options if o["maturity"] == "experimental"}
    decisions = {d["option"]: d for d in model["experimental_decisions"]}
    missing = experimental - set(decisions)
    for opt_id in sorted(missing):
        errors.append(f"experimental option {opt_id}: no decision recorded")
    for opt_id, decision in decisions.items():
        if opt_id not in ids:
            errors.append(f"decisions mention {opt_id}, which is not an option")
        elif opt_id not in experimental and "value" not in decision:
            errors.append(f"decisions mention {opt_id}, which is not experimental")
        if not decision.get("decision") or not decision.get("why"):
            errors.append(f"decision {opt_id}: needs decision and why")
        if not decision.get("exit_criteria"):
            errors.append(f"decision {opt_id}: needs exit_criteria")
    section = experimental_section()
    for opt_id in sorted(experimental):
        opt = next(o for o in options if o["id"] == opt_id)
        token = opt["settings_key"] if opt["settings_key"] in section else None
        if token is None:
            errors.append(f"experimental option {opt_id}: not mentioned in README's experimental list")
    if "Dynamic Widescreen" not in section:
        errors.append("README's experimental list does not mention layout's Dynamic Widescreen value")

    # --- docs, tracked settings, corpus
    md = OPTIONS_MD.read_text(encoding="utf-8")
    if BEGIN not in md or END not in md:
        errors.append("docs/OPTIONS.md is missing the generated-table markers")
    else:
        current = md.split(BEGIN, 1)[1].split(END, 1)[0].strip()
        if current != render_table(model):
            errors.append("docs/OPTIONS.md's table is stale (run tools/options_model.py --write)")

    # settings.ini is written by the game on first run and is not tracked, so a
    # fresh checkout has none: check the local file only when it exists. The
    # keys the game writes are enforced against settings_save above.
    if SETTINGS.is_file():
        local = set(re.findall(r"(?m)^([a-z0-9_]+)\s*=", SETTINGS.read_text(encoding="utf-8")))
        for key in sorted(declared - local):
            errors.append(f"settings key {key!r} is missing from the local settings.ini")

    corpus = json.loads(CORPUS.read_text(encoding="utf-8"))
    corpus_map = corpus.get("option_to_key", {})
    expected = {o["id"]: o["settings_key"] for o in options if o["id"] in corpus_map}
    for opt_id, key in expected.items():
        if corpus_map.get(opt_id) != key:
            errors.append(f"golden corpus maps {opt_id} to {corpus_map.get(opt_id)!r}, model says {key!r}")
    for opt_id in sorted(set(corpus_map) - set(ids)):
        errors.append(f"golden corpus maps {opt_id!r}, which is not in the model")
    return errors


def write() -> int:
    model = load_model()
    md = OPTIONS_MD.read_text(encoding="utf-8")
    if BEGIN not in md or END not in md:
        print("docs/OPTIONS.md has no generated-table markers", file=sys.stderr)
        return 1
    head, rest = md.split(BEGIN, 1)
    tail = rest.split(END, 1)[1]
    OPTIONS_MD.write_text(f"{head}{BEGIN}\n\n{render_table(model)}\n\n{END}{tail}", encoding="utf-8")
    print("wrote docs/OPTIONS.md's table from docs/options.json")
    return 0


def main() -> int:
    if "--write" in sys.argv[1:]:
        return write()
    errors = check()
    if errors:
        print(f"options model: {len(errors)} problem(s)")
        for error in errors:
            print("  - " + error)
        return 1
    model = load_model()
    print(
        f"options model: {len(model['options'])} options, "
        f"{len(model['non_enhancement_keys'])} non-enhancement keys, "
        f"{sum(1 for o in model['options'] if o['maturity'] == 'experimental')} experimental — OK"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
