#!/usr/bin/env python3
"""Check that docs/firmware/LOG_EVENTS.md lists exactly the log names the firmware prints.

lib/rideon_core/src/EventLog.cpp is the source of truth. This fails (exit 1) when an
event, component, symptom or category name is in the code but not in its table in
LOG_EVENTS.md, or the other way round, or a reset reason is missing from the BOOT row.

Usage: python3 tools/check_log_events.py   (from the repo root)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
code = (ROOT / "lib/rideon_core/src/EventLog.cpp").read_text()
doc = (ROOT / "docs/firmware/LOG_EVENTS.md").read_text()
errors = []


def code_names(array):
    m = re.search(r"const char\* const " + array + r"\[\] = \{(.*?)\};", code, re.S)
    if not m:
        errors.append(f"EventLog.cpp: {array} not found")
        return []
    return re.findall(r'"([^"]*)"', m[1])


def doc_section(title):
    m = re.search(r"^## " + title + r"\n(.*?)(?=^## |\Z)", doc, re.S | re.M)
    if not m:
        errors.append(f"LOG_EVENTS.md: no '## {title}' section")
        return ""
    return m[1]


def doc_first_column(title):
    names = []
    for line in doc_section(title).splitlines():
        m = re.match(r"\|\s*`([^`]+)`", line)
        if m:
            names.append(m[1])
    return names


def compare(what, in_code, in_doc):
    for n in in_code:
        if n not in in_doc:
            errors.append(f"LOG_EVENTS.md: {what} `{n}` is printed by the firmware but not documented")
    for n in in_doc:
        if n not in in_code:
            errors.append(f"LOG_EVENTS.md: {what} `{n}` is documented but the firmware has no such name")


compare("event", code_names("EVENT_NAMES"), doc_first_column("Events"))
compare("component", code_names("COMPONENT_NAMES"), doc_first_column("Components"))
compare("symptom", code_names("SYMPTOM_NAMES"), doc_first_column("Symptoms"))

categories = re.findall(r"`([A-Z]+)`", "\n".join(
    line.split("|")[1] for line in doc_section("Categories").splitlines() if line.startswith("| `")))
compare("category", code_names("CATEGORY_NAMES"), categories)

boot_row = next((l for l in doc_section("Events").splitlines() if l.startswith("| `BOOT`")), "")
for reason in code_names("RESET_NAMES"):
    if f"`{reason}`" not in boot_row:
        errors.append(f"LOG_EVENTS.md: reset reason `{reason}` is missing from the BOOT row")

if errors:
    print("\n".join(errors))
    sys.exit(1)
print("LOG_EVENTS.md matches EventLog.cpp")
