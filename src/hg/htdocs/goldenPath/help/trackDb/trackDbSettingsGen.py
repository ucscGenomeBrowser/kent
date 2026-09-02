#!/usr/bin/env python3
"""Generate machine-readable trackDb settings data from the trackDb help docs.

trackDbLibrary.shtml gives each setting's name, types, description and example.
trackDbHub.v3.html says which ones hubs support, and at what level, by which table the
row sits in.

  --import  read those two pages, write trackDbSettings.yaml
  --json    read trackDbSettings.yaml, write trackDbSettings.json

Run "make settings" after editing the docs. Do not hand-edit the yaml: --import rewrites
it, so corrections go in this script. Longer term the yaml should be the source the reader
HTML is built from too; today it only feeds the json.
"""
import sys
import os
import re
import json
import argparse
from bs4 import BeautifulSoup
import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
HUB_VERSION = "v3"
LIB = os.path.join(HERE, "trackDbLibrary.shtml")
HUB = os.path.join(HERE, "trackDbHub.%s.html" % HUB_VERSION)
YAML_OUT = os.path.join(HERE, "trackDbSettings.yaml")
JSON_OUT = os.path.join(HERE, "trackDbSettings.json")

# Stanzas a setting can appear in. hub.txt and genomes.txt are stanzas of their own, so a
# trackDb "common" setting spans the four track roles, not all six.
ROLE_ORDER = ["hub", "genome", "super", "composite", "view", "leaf"]
ALL_ROLES = ["super", "composite", "view", "leaf"]

# Doc sections named for their anchor because the keyword collides with a trackDb setting
# of the same name. The keyword is what goes in a hub file, so map them back.
DOC_NAME_TO_KEYWORD = {
    "hubShortLabel": "shortLabel",
    "hubLongLabel": "longLabel",
    "hubGenome": "genome",
    "trackDbFile": "trackDb",
    "groupsFile": "groups",
    "type_for_hubs": "type",
}

# The docs use HTML entities that decode to non-ASCII punctuation. Fold them: "format" is
# a syntax template someone pastes into a hub file, and the generated files are ASCII.
UNICODE_TO_ASCII = {
    "\u00a0": " ",     # nbsp
    "\u2013": "-",     # ndash
    "\u2014": "-",     # mdash
    "\u2018": "'",     # lsquo
    "\u2019": "'",     # rsquo
    "\u201c": '"',     # ldquo
    "\u201d": '"',     # rdquo
    "\u2026": "...",   # hellip
}


def toAscii(text):
    """Fold the docs' punctuation to ASCII, warning about anything left over."""
    for uni, plain in UNICODE_TO_ASCII.items():
        text = text.replace(uni, plain)
    extra = sorted(set(c for c in text if ord(c) > 126))
    if extra:
        print("warning: no ASCII spelling for %s, add it to UNICODE_TO_ASCII" %
              ", ".join("%r %s" % (c, hex(ord(c))) for c in extra), file=sys.stderr)
    return text


def prettify(tableId):
    """Turn a settingsTable id into a human category label."""
    return tableId.replace("_-_", " - ").replace("_", " ").strip()


def roleHints(tableId):
    """Guess which stanzas a table's settings apply to, from its id."""
    t = tableId.lower()
    if "supertrack" in t:
        return ["super"]
    if "view" in t and "composite" in t:
        return ["view"]
    if "composite" in t or "faceted" in t or "subgroup" in t:
        return ["composite"]
    if "aggregate" in t or "overlay" in t:
        return ["composite", "view", "leaf"]
    if "common" in t:
        return list(ALL_ROLES)
    if "hub_file" in t:
        return ["hub"]
    if "genomes_file" in t:
        return ["genome"]
    if "deprecated" in t:
        return []  # unknown, doImport falls back to the types
    return ["leaf"]  # type-specific tables (bam, bigBed, bigWig, vcfTabix, ...)


def contextOf(tableId):
    """Which file a table's settings go in: hub.txt, genomes.txt, or a trackDb file."""
    t = tableId.lower()
    if "genomes_file" in t:
        return "genomes"
    if "hub_file" in t:
        return "hub"
    return "trackDb"


def firstSentence(text, cap=160):
    """First sentence, length-capped, for the one-line summary."""
    text = re.sub(r"\s+", " ", text).strip()
    if not text:
        return ""
    m = re.search(r"\.(\s|$)", text)
    s = text[:m.start() + 1] if m else text
    if len(s) > cap:
        s = s[:cap - 3].rstrip() + "..."
    return s


def parseLibrary():
    """name -> {types, format, required, description, summary, examples}."""
    with open(LIB, encoding="utf-8") as f:
        soup = BeautifulSoup(f.read(), "html.parser")
    blurbs = {}
    for span in soup.find_all("span", class_="types"):
        div = span.parent
        classes = div.get("class") or []
        if not classes:
            continue
        name = classes[0]
        if name.endswith("_intro") or name.endswith("_example"):
            continue
        types = [c for c in span.get("class", []) if not c.startswith("types")]
        if not types:
            types = ["all"]
        fmtDiv = div.find("div", class_="format")
        fmtCode = fmtDiv.find("code") if fmtDiv else None
        fmt = toAscii(fmtCode.get_text(" ", strip=True)) if fmtCode else name

        req = div.find("p", class_="isRequired")
        reqText = req.get_text(" ", strip=True).lower() if req else ""
        required = "yes" in reqText or "for hubs" in reqText

        descParts = []
        for p in div.find_all("p"):
            if "isRequired" in (p.get("class") or []):
                continue
            txt = p.get_text(" ", strip=True)
            if txt.lower().startswith("example"):
                break
            if txt:
                descParts.append(txt)
        description = toAscii(re.sub(r"\s+", " ", " ".join(descParts)).strip())
        examples = []
        for pre in div.find_all("pre"):
            ex = toAscii(pre.get_text().strip())
            if ex:
                examples.append(ex)

        blurbs[name] = {
            "types": types,
            "format": fmt,
            "required": required,
            "description": description,
            "summary": firstSentence(description),
            "examples": examples,
        }
    return blurbs


def parseHubSpec():
    """Walk the hub spec tables in order.

    Returns (order, home, hints): setting names in document order, home[name] =
    {category, context, level}, hints[name] = set of roles.
    """
    with open(HUB, encoding="utf-8") as f:
        soup = BeautifulSoup(f.read(), "html.parser")
    # Scan the whole document rather than the #specification subtree: html.parser
    # closes that div early at a malformed tag, hiding the later container tables.
    order = []
    home = {}
    hints = {}
    for table in soup.find_all("table", class_="settingsTable"):
        tid = table.get("id", "")
        if tid == "toc":
            continue
        category = prettify(tid)
        context = contextOf(tid)
        tHints = roleHints(tid)
        for td in table.find_all("td"):
            classes = td.get("class") or []
            if not classes:
                continue
            name = classes[0]
            code = td.find("code")
            if code is None:
                continue
            level = None
            for c in (code.get("class") or []):
                if c.startswith("level-"):
                    level = c[len("level-"):]
            if name not in home:
                order.append(name)
                home[name] = {"category": category, "context": context, "level": level}
                hints[name] = set()
            hints[name].update(tHints)
    return order, home, hints


def doImport():
    blurbs = parseLibrary()
    order, home, hints = parseHubSpec()
    entries = []
    missing = []
    for name in order:
        b = blurbs.get(name)
        if b is None:
            missing.append(name)
            continue
        info = home[name]
        roles = [r for r in ROLE_ORDER if r in hints[name]]
        if not roles and info["context"] == "trackDb":
            if b["types"] == ["all"]:
                # A copy, or safe_dump writes yaml aliases to one shared list.
                roles = list(ALL_ROLES)
            else:
                roles = ["leaf"]
        entries.append({
            "name": name,
            "types": b["types"],
            "roles": roles,
            "category": info["category"],
            "context": info["context"],
            "level": info["level"],
            "required": b["required"],
            "summary": b["summary"],
            "description": b["description"],
            "format": b["format"],
            "examples": b["examples"],
        })

    with open(YAML_OUT, "w", encoding="utf-8") as f:
        f.write("# Generated by trackDbSettingsGen.py --import, which rewrites the whole file.\n")
        f.write("# Edit the trackDb help docs instead, then run 'make settings'.\n")
        f.write("# 'roles' is a guess from which doc table a setting sits in; check the\n")
        f.write("# container-only ones.\n\n")
        yaml.safe_dump(entries, f, sort_keys=False, width=100)
    print("imported %d settings -> %s" % (len(entries), YAML_OUT))
    if missing:
        print("  (%d hub rows had no library blurb, skipped): %s" %
              (len(missing), ", ".join(missing)))


def doJson():
    with open(YAML_OUT, encoding="utf-8") as f:
        entries = yaml.safe_load(f)
    categories = []
    settings = []
    for e in entries:
        if not e["roles"]:
            continue
        cat = e["category"]
        if cat not in categories:
            categories.append(cat)
        example = e["examples"][0] if e["examples"] else ""
        # "fmt" is the syntax to fill in, "ex" the doc's example of it filled in.
        settings.append({
            "key": DOC_NAME_TO_KEYWORD.get(e["name"], e["name"]),
            "category": cat,
            "roles": e["roles"],
            "types": "all" if e["types"] == ["all"] else e["types"],
            "level": e["level"],
            "fmt": e["format"],
            "ex": example,
            "desc": e["summary"],
        })
    out = {"version": HUB_VERSION, "categories": categories, "settings": settings}
    with open(JSON_OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)
        f.write("\n")
    print("wrote %d settings in %d categories -> %s" %
          (len(settings), len(categories), JSON_OUT))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--import", dest="doImport", action="store_true",
                    help="write trackDbSettings.yaml from the help docs")
    ap.add_argument("--json", dest="doJson", action="store_true",
                    help="write trackDbSettings.json from trackDbSettings.yaml")
    args = ap.parse_args()
    if not (args.doImport or args.doJson):
        ap.error("specify --import and/or --json")
    if args.doImport:
        doImport()
    if args.doJson:
        doJson()


if __name__ == "__main__":
    main()
