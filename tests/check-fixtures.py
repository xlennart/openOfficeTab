"""Check public Office fixtures for personal metadata and local paths."""
from pathlib import Path
from zipfile import ZipFile
import re
import subprocess
import xml.etree.ElementTree as ET

fixtures = Path(__file__).parent / "fixtures"
tracked = subprocess.check_output(["git", "ls-files", "-z"], cwd=fixtures.parent.parent).decode("utf-8").split("\0")
for name in tracked:
    parts = Path(name).parts
    assert "AGENTS.md" not in parts and ".aoci" not in parts, f"Private governance file tracked publicly: {name}"
    assert not (name.startswith("aoci.") and name.endswith(".txt")), f"Private cognition file tracked publicly: {name}"
print("OK: local project instructions and cognition are excluded from the public Git tree")
for filename in ("word-demo.docx", "excel-demo.xlsx", "powerpoint-demo.pptx"):
    with ZipFile(fixtures / filename) as archive:
        assert archive.testzip() is None, filename
        for name in archive.namelist():
            assert not name.lower().startswith(("docprops/thumbnail", "docprops/custom")), name
            if not name.endswith((".xml", ".rels")):
                continue
            content = archive.read(name).decode("utf-8")
            root = ET.fromstring(content)
            assert not re.search(r"(?:[A-Za-z]:\\|file:///|OneDrive|/Users/|\\\\[^<\s]+)", content), name
            if name == "docProps/core.xml":
                for element in root:
                    if element.tag.rsplit("}", 1)[-1] in ("creator", "lastModifiedBy"):
                        assert element.text == "openOfficetab", name
            if name == "word/document.xml" or name.startswith(("xl/sharedStrings", "ppt/slides/", "ppt/notesSlides/")):
                assert not any(element.text for element in root.iter() if element.tag.rsplit("}", 1)[-1] == "t"), name
    print(f"OK: {filename}")
