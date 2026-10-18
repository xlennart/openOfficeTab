"""Catch untranslated native UI keys and broken dynamic Ribbon label wiring."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parent.parent
catalog = (root / "src/localization.h").read_text(encoding="utf-8")
literal = r'L"(?:\\.|[^"\\])*"'
keys = set(re.findall(r'\{(' + literal + r'),L"', catalog))
missing = []
for path in (root / "src").glob("*"):
    if path.suffix not in (".cpp", ".h") or path.name == "localization.h":
        continue
    for key in re.findall(literal, path.read_text(encoding="utf-8")):
        if re.search(r"[\u4e00-\u9fff]", key) and key != 'L"简体中文"' and key not in keys:
            missing.append((path.name, key))
assert not missing, missing
addin = (root / "src/addin.cpp").read_text(encoding="utf-8")
markup = re.search(r'LR"\((<customUI.*?</customUI>)\)"', addin, re.S).group(1)
xml = ET.fromstring(markup)
assert xml.attrib["onLoad"] == "RibbonLoaded"
ids = set(re.findall(r'\{L"(oot\w+)",', catalog))
for control in xml.iter():
    if control.attrib.get("id") in ids:
        assert control.attrib["getLabel"] == "GetLabel"
        assert "label" not in control.attrib
assert ids == {control.attrib.get("id") for control in xml.iter() if control.attrib.get("getLabel")}
print(f"Native UI catalog coverage and {len(ids)} dynamic Ribbon labels verified.")
