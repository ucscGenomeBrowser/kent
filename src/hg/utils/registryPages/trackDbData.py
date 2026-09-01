#!/usr/bin/env python3
"""trackDbData.py - line the trackDb settings documentation up against the cart.

Refs #37908, #37838.  Two files in the tree say which track types a setting
applies to, and they were written independently:

  trackDbLibrary.shtml, through the generated trackDbSettings.json, carries a
  `types` list per setting.  That list is what `make settings` publishes to the
  hub wizard, and #37908 exists because about sixty of those lists were wrong.

  cartTrackVarCatalog files each cart variable under the config code that reads
  it, and records the trackDb types that code serves (`tdbTypes`).  It was built
  by reading hui.c and the per-type Ui functions, not by reading the docs.

Where a trackDb setting and a cart variable are the same knob, the two files are
answering the same question from different evidence, so they can be compared.
That is all this module does.

Two ways a setting and a cart variable are joined:

  same name    the doc key and the cart variable's suffix are the same word.
               `autoScale` in trackDb is `<track>.autoScale` in the cart.

  tdbDefault   the cart catalog names the trackDb setting a variable takes its
               default from, when the two are spelled differently.  `heightPer`
               takes its default from `maxHeightPixels`.

The first is strong evidence: it is one knob under two names.  The second is
weaker, and is reported apart from it.  A variable can be offered for a type
whose default comes from somewhere else entirely, so a type the cart knows is
not automatically a type the trackDb setting governs.

Nothing here decides anything.  It produces a candidate list, the same shape the
#37908 tier walks worked from, and every row still has to be read.
"""

import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import registryData as rd                                          # noqa: E402


SETTINGS_JSON = "hg/htdocs/goldenPath/help/trackDb/trackDbSettings.json"

# The cart catalog has one bucket for every kind of container, because one block
# of code draws them all.  The docs name the container types separately.  These
# are the doc spellings that answer the cart's single "container".
CONTAINER_TYPES = {"multiWig", "compositeTrack", "composite", "container",
                   "superTrack", "view", "subGroups", "faceted"}

# A row placed at level 2 applies to every track, which the docs write as "all".
ALL = "all"


def loadSettings(src=None):
    """The generated trackDb settings, keyed by setting name."""
    src = src or rd.kentSrc()
    path = os.path.join(src, SETTINGS_JSON)
    if not os.path.exists(path):
        raise SystemExit("cannot find %s\nrun 'make settings' in that directory first" % path)
    with open(path) as f:
        doc = json.load(f)
    return {s["key"]: s for s in doc["settings"]}, doc.get("version", "")


def normalizeTypes(entries):
    """Turn the cart catalog's tdbTypes into bare trackDb type names.

    The catalog writes a type list for people, so an entry can carry a gloss or
    hold several names: "bed<n> +", "chain (via snake)", "bed 12 + (gtexGene*)".
    Take the name and drop the rest.
    """
    out = set()
    for raw in entries:
        for piece in raw.split(","):
            name = re.sub(r"\(.*?\)", "", piece).strip()
            if not name:
                continue
            name = name.split()[0].replace("<n>", "").rstrip("*.")
            if name:
                out.add(name)
    return out


def cartIndex(src=None):
    """Every track cart variable, with the trackDb types its config code serves.

    Keyed by the catalog's own suffix, since that is what a trackDb setting name
    is compared against.  A variable read by several config functions collects
    the types of all of them.
    """
    cat = rd.catalogJson("track", src or rd.kentSrc())
    index = {}

    def note(v, types, group, level):
        e = index.setdefault(v["name"], {"types": set(), "groups": [],
                                         "tdbDefault": set(), "levels": set(),
                                         "srcs": set()})
        e["types"] |= set(types)
        e["groups"].append(group)
        e["levels"].add(level)
        if v.get("src"):
            e["srcs"].add(v["src"])
        if v.get("tdbDefault"):
            e["tdbDefault"].add(v["tdbDefault"])

    levels = cat["levels"]
    for name, group in levels["2_common"]["groups"].items():
        for v in group.get("vars", []):
            note(v, [ALL], "common/" + name, "common")
    for name, group in levels["2b_container"]["groups"].items():
        for v in group.get("vars", []):
            note(v, ["container"], "container/" + name, "container")
    for name, group in levels["3_byType"]["types"].items():
        types = normalizeTypes(group.get("tdbTypes", []))
        for v in group.get("vars", []):
            note(v, types, "type/" + name, "byType")
    for name, group in levels["3b_byTrackName"]["tracks"].items():
        for v in group.get("vars", []):
            note(v, [], "track/" + name, "byTrackName")
    for name, group in levels["4_families"]["groups"].items():
        for v in group.get("vars", []):
            note(v, [], "family/" + name, "family")
    return index


def docTypes(setting):
    """The setting's type list, with every container spelling folded to one name."""
    types = setting["types"]
    types = {types} if isinstance(types, str) else set(types)
    return {"container" if t in CONTAINER_TYPES else t for t in types}


def pairUp(settings, cart):
    """Join trackDb settings to cart variables, by name and by tdbDefault.

    Returns a list of pairs, each with the two type lists and what they differ
    by.  `comparable` is false when one side has nothing to say: a cart variable
    filed by track name or in a wildcard family has no type list at all, and a
    row that applies to every track cannot disagree about which types it covers.
    """
    pairs = []

    def addPair(setting, var, how):
        dt = docTypes(settings[setting])
        ct = set(cart[var]["types"])
        comparable = bool(ct) and ALL not in ct and ALL not in dt
        pairs.append({
            "setting": setting, "var": var, "how": how,
            "docTypes": sorted(dt), "cartTypes": sorted(ct),
            "groups": sorted(set(cart[var]["groups"])),
            "srcs": sorted(cart[var]["srcs"]),
            "comparable": comparable,
            "missing": sorted(ct - dt) if comparable else [],
            "extra": sorted(dt - ct) if comparable else [],
            "desc": settings[setting].get("desc", ""),
            "level": settings[setting].get("level", ""),
        })

    for setting in settings:
        if setting in cart:
            addPair(setting, setting, "same name")
    for var, e in cart.items():
        for setting in sorted(e["tdbDefault"]):
            if setting in settings and setting != var:
                addPair(setting, var, "tdbDefault")

    pairs.sort(key=lambda p: (p["setting"].lower(), p["var"].lower()))
    return pairs


def byMissingType(pairs, how=None):
    """Group the candidates by the type the docs do not list.

    One missing type is usually one edit repeated across a family of settings:
    every bam setting that should also say cram, every vcf setting that should
    also say vcfPhasedTrio.  Grouping this way is how the fix gets made.
    """
    out = {}
    for p in pairs:
        if how and p["how"] != how:
            continue
        for t in p["missing"]:
            out.setdefault(t, []).append(p)
    return dict(sorted(out.items(), key=lambda kv: (-len(kv[1]), kv[0])))


def unpaired(settings, cart, pairs):
    """The two sides that did not join: no runtime override, and no doc row."""
    pairedSettings = {p["setting"] for p in pairs}
    pairedVars = {p["var"] for p in pairs}
    return (sorted(s for s in settings if s not in pairedSettings),
            sorted(v for v in cart if v not in pairedVars))


def load(src=None):
    """Everything the correlation page needs, in one call."""
    src = src or rd.kentSrc()
    settings, version = loadSettings(src)
    cart = cartIndex(src)
    pairs = pairUp(settings, cart)
    noOverride, noSetting = unpaired(settings, cart, pairs)
    return {"settings": settings, "version": version, "cart": cart, "pairs": pairs,
            "noOverride": noOverride, "noSetting": noSetting}


def main():
    """Print the candidate list.  registryPages.py draws the page."""
    data = load()
    pairs = data["pairs"]
    strong = [p for p in pairs if p["how"] == "same name"]
    weak = [p for p in pairs if p["how"] == "tdbDefault"]
    print("trackDb settings   %d  (trackDbSettings.json %s)"
          % (len(data["settings"]), data["version"]))
    print("track cart names   %d" % len(data["cart"]))
    print("paired             %d  (%d by name, %d by tdbDefault)"
          % (len(pairs), len(strong), len(weak)))
    print("comparable         %d" % sum(1 for p in pairs if p["comparable"]))
    print("agree exactly      %d"
          % sum(1 for p in pairs if p["comparable"] and not p["missing"] and not p["extra"]))
    print()
    for label, group in (("same name", strong), ("tdbDefault", weak)):
        missing = byMissingType(group)
        print("--- %s: types the cart serves that the docs do not list" % label)
        if not missing:
            print("    none")
        for t, ps in missing.items():
            print("  %-16s %s" % (t, ", ".join(sorted({p["setting"] for p in ps}))))
        print()
    print("trackDb settings with no cart variable   %d" % len(data["noOverride"]))
    print("cart variables with no trackDb setting   %d" % len(data["noSetting"]))


if __name__ == "__main__":
    main()
