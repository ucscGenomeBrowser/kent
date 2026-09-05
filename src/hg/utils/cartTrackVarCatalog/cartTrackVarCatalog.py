#!/usr/bin/env python3
"""cartTrackVarCatalog.py - the registry of track-scoped cart variables.

Refs #37838.  This is the hand-curated catalog that backs two things:

  1. The JSON cart schema (#37838) - the hierarchy below is the shape the
     stored cart wants to grow into: track name at the top, then the vars
     every track has, then a per-type section, then nested leaf groups.
  2. The planned cart accessor layer - each entry names one variable, its
     value type, its separator, and where the tree reads it, so accessors
     can be generated or at least checked against reality.

Harvested mechanically from cart*ClosestToHome() and safef("%s.%s") call
sites in hg/lib, hg/hgTracks, hg/hgTrackUi and hg/cgilib, then curated by
hand: names that turned out to be table names, file suffixes or non-cart
strings were dropped, macro identifiers were resolved to their values, and
the type/enum/default columns were read out of the UI code.

Usage:
    cartTrackVarCatalog.py --json out.json
    cartTrackVarCatalog.py --html out.html
    cartTrackVarCatalog.py --check      # sanity checks, prints counts
    cartTrackVarCatalog.py --reconcile  # diff the catalog against the tree
    cartTrackVarCatalog.py --reconcile --verbose      # ... with the full diff
    cartTrackVarCatalog.py --update-baseline          # accept new tree names

--reconcile is the mode meant for a nightly cron: it prints nothing and exits 0
when nothing has changed, and exits 1 with a list when the tree has grown a
track-scoped name that is in neither the catalog nor the baseline.  --check is a
different thing, and not a substitute: it only reads this file, so it cannot see
the tree move at all.

Matching is on the variable name with its leading separator stripped, because
the harvester cannot always tell which separator a name is used with: the fourth
argument of cart*ClosestToHome() is a bare suffix, while a safef("%s.%s") site
carries the dot.  So <track>.foo and <track>_foo reconcile as one name here,
even though the catalog records the two spellings separately and the difference
between them is a live bug in at least one place (see _pairEndsByName).

The scan cannot tell a cart variable from a table name, a filename suffix or an
SQL fragment, so a good part of what it finds is not a cart variable at all
(.bai, _gold, .tbi).  Those live in BASELINE_FILE next to this script rather
than being argued with one at a time: reconcile complains only about a name in
neither the catalog nor the baseline.  Accept new ones with --update-baseline and
commit the file, which puts the decision in the git log.
"""

import argparse
import html
import json
import os
import re
import sys

# Harvested names that are not track-scoped cart variables.  See the docstring.
BASELINE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "cartVarsNotCataloged.txt")

# Floor on how many names a working scan finds; well under the real count.  See
# the check in reconcile().
MIN_TREE_NAMES = 150

# ---------------------------------------------------------------------------
# how a track-scoped name is built
# ---------------------------------------------------------------------------

NAMING = {
    "canonical": "<track>.<var>",
    "legacy": "<track>_<var>",
    "legacyNote":
        "The underscore form predates the dot form.  cartRemoveAllForTdb() "
        "(hg/lib/cart.c:3306) removes both prefixes plus the bare track name, "
        "and carries the comment 'All should be {track}.{varName}'.  Any "
        "rewrite should keep reading both.",
    "lookupOrder": [
        "<subtrack>.<var>",
        "<composite>.<view>.<var>",
        "<composite>.<var>",
    ],
    "lookupOrderSrc": "hg/lib/cart.c:cartLookUpVariableClosestToHome",
    "lookupOrderNote":
        "parentLevel=TRUE starts the search at the parent, so a subtrack "
        "setting always wins over its view, which wins over its composite.  "
        "The JSON form needs to preserve that three-level fallback, not "
        "flatten it.",
    "trackNamePrefixes": [
        {"prefix": "hub_<hubId>_", "what": "track hub track",
         "src": "hg/lib/trackHub.c"},
        {"prefix": "ct_", "what": "custom track",
         "src": "hg/lib/customTrack.c"},
        {"prefix": "dup_<n>_", "what": "duplicated track",
         "src": "hg/inc/dupTrack.h:DUP_TRACK_PREFIX"},
    ],
    "valueEncoding":
        "Everything is a string in the cart hash.  The 'type' column below is "
        "how the reader interprets it, and is what the JSON form could encode "
        "natively.  'list' vars are multi-valued: the same name appears more "
        "than once in the var=val encoding and must become a JSON array.",
}

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def v(name, type_, src, sep=".", values=None, default=None, note=None,
      tdb=None, multi=False, aliases=None, valuesSrc=None):
    """One catalog entry.

    name    variable name after the <track> prefix and separator
    type_   bool | int | float | string | enum | list | color | hidden
    src     file:line where the tree reads or writes it
    sep     '.' (canonical) or '_' (legacy)
    values  the values the CART may hold, exactly as the reader compares them
    aliases other spellings a writer may use -> the canonical cart value.
            Usually the trackDb vocabulary, which is often not the cart
            vocabulary; see the autoScale entry for the case that proved it.
    valuesSrc  the C array the values were checked against.  Name it for any
            enum whose value list is not obvious from src, so the next reader
            can re-verify instead of trusting this file.
    tdb     trackDb setting that supplies the default, if differently named
    """
    d = {"name": name, "type": type_, "sep": sep, "src": src}
    if values:
        d["values"] = values
    if aliases:
        d["aliases"] = aliases
    if valuesSrc:
        d["valuesSrc"] = valuesSrc
    if default is not None:
        d["default"] = default
    if tdb:
        d["tdbDefault"] = tdb
    if multi:
        d["multi"] = True
    if note:
        d["note"] = note
    return d


# ---------------------------------------------------------------------------
# LEVEL 2: variables every track can have, whatever its type
# ---------------------------------------------------------------------------

COMMON = {
    "visibility": {
        "what": "What the track is set to.  The only var that is the bare "
                "track name with no suffix at all.",
        "vars": [
            v("", "enum", "hg/lib/hui.c:9954", sep="",
              values=["hide", "dense", "squish", "pack", "full"],
              note="Bare <track>.  For a superTrack the values are "
                   "'show'/'hide' instead (hg/lib/hui.c:9887).  This is the "
                   "var #37838 discussed moving under a 'vis.' prefix."),
            v("_sel", "bool", "hg/lib/hui.c:5434", sep="_",
              note="Subtrack/composite-member checkbox.  Four-state in the "
                   "UI (checked, unchecked, checked-disabled) but stored 0/1; "
                   "see fourState* in hg/inc/hui.h."),
            v("_hideKids", "bool", "hg/hgTracks/hgTracks.c:7652", sep="_",
              note="Container collapsed in hgTracks, children not drawn."),
            v("_faux", "hidden", "hg/lib/hui.c:5524", sep="_",
              note="UI-only element id, not persisted state."),
            v("_toggle", "hidden", "hg/lib/hui.c:5529", sep="_",
              note="UI-only element id, not persisted state."),
        ],
    },
    "layout": {
        "what": "Where the track sits in the image and how tall it is.",
        "vars": [
            v("priority", "float", "hg/lib/hui.c:tdbAddPrioritiesFromCart",
              tdb="priority",
              note="User drag-reorder override of the trackDb priority."),
            v("group", "string", "hg/hgTracks/hgTracks.c:groupTracks",
              note="User moved the track into a different track group."),
            v("_imgOrd", "int", "hg/hgTracks/imageV2.c:flatTracksSort", sep="_",
              note="Row order in the image, set by drag-reorder in "
                   "hgTracks.js.  Distinct from priority."),
            v("heightPer", "int", "hg/inc/wiggle.h:HEIGHTPER",
              default="128", tdb="maxHeightPixels",
              note="Track height in pixels.  Shared by wig, lolly, interact, "
                   "long, sample and vcf haplotype displays."),
        ],
    },
    "color": {
        "what": "Per-track color override, offered for any track whose type "
                "supports it (tdbSupportsColorOverride).",
        "vars": [
            v("colorOverride", "color", "hg/lib/hui.c:colorTrackOption",
              note="RGB as '#rrggbb'."),
            v("colorOverrideOn", "bool", "hg/lib/hui.c:colorTrackOption"),
        ],
    },
    "ui": {
        "what": "State of the hgTrackUi page itself, not of the drawing.",
        "vars": [
            v("section_<section>_close", "bool",
              "hg/lib/jsHelper.c:411",
              note="One per collapsible section on the track's config page "
                   "(jsBeginCollapsibleSection).  Known sections include "
                   "colorByAttribute, superDescription, superMembers."),
            v("_button", "hidden", "hg/hgTracks/config.c:321", sep="_"),
            v("_defaultBut", "hidden", "hg/hgTracks/config.c:348", sep="_"),
            v("_hideAllBut", "hidden", "hg/hgTracks/config.c:331", sep="_"),
            v("_showAllBut", "hidden", "hg/hgTracks/config.c:342", sep="_"),
            v("_edit", "hidden", "hg/hgTracks/hgTracks.c:10121", sep="_"),
        ],
    },
    "dataFilters": {
        "what": "Filters offered for many types, not tied to one of them.",
        "vars": [
            v("nameFilter", "string", "hg/lib/hui.c:filterNameOption",
              note="Wildcard match on item name."),
            v("doMergeItems", "bool", "hg/inc/hui.h:MERGESPAN_CART_SETTING",
              tdb="mergeSpannedItems",
              note="Collapse items that span the whole window into one."),
            v("doWiggle", "bool", "hg/lib/hui.c:wigOption",
              note="Draw a bed/genePred/psl/bam type as a coverage wiggle.  "
                   "When on, the whole wig type group below applies too."),
            v("squishyPackPoint", "float", "hg/lib/hui.c:squishyPackOption",
              note="Row count past which pack degrades to squish."),
            v("doSnake", "bool", "hg/lib/hui.c:snakeOption",
              note="Draw a chain/psl as a snake."),
        ],
    },
}

# ---------------------------------------------------------------------------
# LEVEL 2b: containers - composite, view, superTrack, multiWig, faceted
# ---------------------------------------------------------------------------

CONTAINER = {
    "composite": {
        "what": "Vars a composite parent owns.  Subtrack selection lives on "
                "the child (<subtrack>_sel), not here.",
        "vars": [
            v("displaySubtracks", "enum", "hg/lib/hui.c:compositeUiSubtracks",
              values=["all", "selected"]),
            v("hideEmptySubtracks", "bool",
              "hg/lib/hui.c:compositeHideEmptySubtracks",
              tdb="hideEmptySubtracks"),
            v("sortOrder", "string", "hg/lib/hui.c:sortOrderGet",
              note="Subtrack table sort, e.g. 'cellType=+ view=-'."),
            v("facetSortOrder", "string", "hg/hgTrackUi/hgTrackUi.c:3314",
              note="Same thing for a faceted composite's table, and the same "
                   "'field=+ field2=-' syntax, which facetedComposite.js:921 "
                   "says it copied from sortOrder above.  Written only by "
                   "JavaScript (facetedComposite.js:931), sent even when empty "
                   "so the server clears a stale value, and read back in "
                   "hgTrackUi to override trackDb's defaultSortField.  The "
                   "read treats it as untrusted, because the JSON it lands in "
                   "goes inside a <script> block: anything outside "
                   "alphanumerics and _ . - + = space drops the whole value."),
            v("filterComp.<groupTag>", "list", "hg/lib/hui.c:3119", multi=True,
              note="One per ABC dimension of a filterComposite.  'All' means "
                   "every option selected."),
            v("subGroup<n>", "string", "hg/lib/hui.c:2905",
              note="trackDb-side dimension definition, read back when "
                   "rebuilding the matrix."),
        ],
    },
    "view": {
        "what": "A composite view is a middle namespace, not a var of its "
                "own: any type-layer var can appear as "
                "<composite>.<view>.<var>.",
        "vars": [
            v("<view>.<anyTypeVar>", "varies",
              "hg/lib/cart.c:cartLookUpVariableClosestToHome",
              note="Middle level of the three-level lookup."),
            v("_link", "hidden", "hg/lib/hui.c:hCompositeDisplayViewDropDowns",
              sep="_"),
        ],
    },
    "superTrack": {
        "what": "SuperTracks have no drawing settings of their own.",
        "vars": [
            v("", "enum", "hg/lib/hui.c:9887", sep="",
              values=["show", "hide"]),
            v("_check", "hidden", "hg/hgTrackUi/hgTrackUi.c:2861", sep="_"),
            v("_link", "hidden", "hg/hgTrackUi/hgTrackUi.c:2894", sep="_"),
        ],
    },
    "multiWig": {
        "what": "Overlay container.  Uses the wig type group plus these.",
        "vars": [
            v("aggregate", "enum", "hg/inc/wiggle.h:AGGREGATE",
              values=["none", "transparentOverlay", "solidOverlay",
                      "stacked", "add", "subtract"]),
            v("viewFunc", "enum", "hg/inc/wiggle.h:VIEWFUNC",
              values=["showAll", "addAll", "subtractAll"]),
            v("showSubtrackColorOnUi", "bool",
              "hg/inc/hui.h:SUBTRACK_COLOR_PATCH"),
        ],
    },
    "facetedComposite": {
        "what": "compositeTrack faceted.  Facet state is keyed off a var "
                "prefix, not the track name; see the exceptions section.",
        "vars": [
            v("_facet_selList", "string", "hg/lib/facetedTable.c:87", sep="_"),
            v("_facet_fieldName", "string", "hg/lib/facetedTable.c:103",
              sep="_"),
            v("_facet_fieldVal", "string", "hg/lib/facetedTable.c:111",
              sep="_"),
            v("_facet_op", "string", "hg/lib/facetedTable.c:119", sep="_"),
        ],
    },
}

# ---------------------------------------------------------------------------
# LEVEL 4: reusable leaf groups.  Types below reference these by key.
# ---------------------------------------------------------------------------

FAMILIES = {
    "numericFilter": {
        "what": "Per-field numeric filter.  Both spellings are live: the "
                "lower-case dotted form and the older camel-case suffix "
                "form.  isComplexSetting() (hg/lib/trackDb.c:269) is the "
                "authoritative list of these wildcard settings.",
        "pattern": ["filter.<field>", "<field>Filter"],
        "vars": [
            v("filter.<field>", "float", "hg/inc/bigBedFilter.h:23",
              note="Value, or 'min:max' colon pair."),
            v("filter.<field>Min", "float", "hg/inc/hui.h:_MIN"),
            v("filter.<field>Max", "float", "hg/inc/hui.h:_MAX"),
            v("filterLimits.<field>", "string", "hg/lib/trackDb.c:282",
              note="trackDb-side bounds; cart values outside them are "
                   "dropped (hg/hgTracks/bigBedTrack.c:88)."),
            v("filterByRange.<field>", "bool", "hg/lib/trackDb.c:286"),
            v("filterLabel.<field>", "string", "hg/inc/bigBedFilter.h:28"),
            v("filterPriority.<field>", "float", "hg/inc/bigBedFilter.h:29"),
            v("<field>FilterLimits", "string", "hg/lib/trackDb.c:298"),
            v("<field>FilterPriority", "float", "hg/lib/trackDb.c:302"),
        ],
    },
    "textFilter": {
        "what": "Per-field text filter, wildcard or regex.",
        "vars": [
            v("filterText.<field>", "string", "hg/inc/bigBedFilter.h:24"),
            v("filterType.<field>", "enum", "hg/inc/bigBedFilter.h:27",
              values=["wildcard", "regexp"]),
            v("<field>FilterText", "string", "hg/lib/trackDb.c:300"),
            v("<field>FilterType", "enum", "hg/lib/trackDb.c:296"),
        ],
    },
    "filterBy": {
        "what": "Multi-select filter on a categorical field.  Value 'All' "
                "means no filtering.",
        "vars": [
            v("filterBy.<field>", "list", "hg/lib/hui.c:4058", multi=True),
            v("filterValues.<field>", "string", "hg/inc/bigBedFilter.h:25",
              note="trackDb-side value list."),
            v("filterValuesDefault.<field>", "string",
              "hg/inc/bigBedFilter.h:26"),
            v("doAdvanced", "bool", "hg/lib/hui.c:filterBySetCfgUiGuts",
              note="Advanced filter box expanded."),
        ],
    },
    "highlightBy": {
        "what": "Same shape as the filter family, but highlights instead of "
                "hiding.  Every filter* name has a highlight* twin.",
        "vars": [
            v("highlightBy.<field>", "list", "hg/lib/hui.c:4058", multi=True),
            v("highlight.<field>", "float", "hg/inc/bigBedFilter.h:64"),
            v("highlightText.<field>", "string", "hg/inc/bigBedFilter.h:65"),
            v("highlightType.<field>", "enum", "hg/inc/bigBedFilter.h:68"),
            v("highlightLimits.<field>", "string", "hg/lib/trackDb.c:...",
              note="See isComplexSetting() for the full twin list."),
        ],
    },
    "score": {
        "what": "The old score-based filters, offered by scoreCfgUi for bed "
                "and friends.",
        "vars": [
            v("scoreFilter", "int", "hg/lib/hui.c:scoreCfgUi",
              tdb="scoreFilter"),
            v("scoreFilterMin", "int", "hg/lib/hui.c:7268"),
            v("scoreFilterMax", "int", "hg/lib/hui.c:7271"),
            v("filterTopScorersOn", "bool", "hg/lib/hui.c:7301"),
            v("filterTopScorersCt", "int", "hg/lib/hui.c:7307"),
            v("minGrayLevel", "int", "hg/inc/hui.h:MIN_GRAY_LEVEL",
              tdb="minGrayLevel"),
        ],
    },
    "cds": {
        "what": "Codon and base coloring, offered for every type that has "
                "aligned or CDS-bearing items.  hg/lib/hui.c:baseColorDropLists.",
        "vars": [
            v("baseColorDrawOpt", "enum", "hg/inc/hui.h:BASE_COLOR_VAR_SUFFIX",
              values=["none", "genomicCodons", "itemCodons", "diffCodons",
                      "itemBases", "diffBases"]),
            v("codonNumbering", "bool", "hg/inc/hui.h:CODON_NUMBERING_SUFFIX"),
            v("showDiffBasesAllScales", "bool",
              "hg/inc/hui.h:SHOW_DIFF_BASES_ALL_SCALES"),
            v("baseColorUseCds", "string", "hg/inc/hui.h:BASE_COLOR_USE_CDS",
              note="trackDb setting; read through cartOrTdb."),
            v("baseColorUseSequence", "string",
              "hg/inc/hui.h:BASE_COLOR_USE_SEQUENCE",
              note="trackDb setting; read through cartOrTdb."),
        ],
    },
    "indel": {
        "what": "Indel display, alignment types.  "
                "hg/lib/hui.c:indelShowOptionsWithNameExt.",
        "vars": [
            v("indelDoubleInsert", "bool", "hg/inc/hui.h:INDEL_DOUBLE_INSERT"),
            v("indelQueryInsert", "bool", "hg/inc/hui.h:INDEL_QUERY_INSERT"),
            v("indelPolyA", "bool", "hg/inc/hui.h:INDEL_POLY_A"),
        ],
    },
    "label": {
        "what": "Which label to draw.  Legacy tracks each grew their own "
                "checkbox set, which is why the same idea appears under so "
                "many names.  hg/lib/hui.c:labelCfgUi is the modern one.",
        "vars": [
            v("label", "list", "hg/lib/hui.c:labelMakeCheckBox", multi=True,
              tdb="labelFields",
              note="Modern bigBed form: one entry per chosen field."),
            v("label.gene", "bool", "hg/hgTracks/retroGene.c:121"),
            v("label.acc", "bool", "hg/hgTracks/transMapTracks.c:48"),
            v("label.orgCommon", "bool", "hg/hgTracks/transMapTracks.c:50"),
            v("accLabel", "bool", "hg/hgTrackUi/hgTrackUi.c:1511"),
            v("geneLabel", "bool", "hg/hgTrackUi/hgTrackUi.c:1510"),
            v("posLabel", "bool", "hg/hgTrackUi/hgTrackUi.c:1513"),
            v("sprotLabel", "bool", "hg/hgTrackUi/hgTrackUi.c:1512"),
        ],
    },
    "gencodeShow": {
        "what": "GENCODE / knownGene transcript-set filters.",
        "vars": [
            v("show.set", "enum", "hg/lib/hui.c:newGencodeShowOptions"),
            v("show.noncoding", "bool", "hg/lib/hui.c:newGencodeShowOptions"),
            v("show.pseudo", "bool", "hg/lib/hui.c:newGencodeShowOptions"),
            v("show.spliceVariants", "bool",
              "hg/lib/hui.c:newGencodeShowOptions"),
            v("show.comprehensive", "bool",
              "hg/hgTrackUi/hgTrackUi.c:1747"),
            v("hideNoncoding", "bool", "hg/inc/hui.h:HIDE_NONCODING_SUFFIX"),
            v("maxTrans", "int", "hg/lib/hui.c:gencodeMaxTransControl"),
        ],
    },
    "decorator": {
        "what": "A decorator is a nested namespace inside the track: "
                "<track>.decorator.<decoratorName>.<var>.  "
                "hg/lib/decoratorUi.c gins up a fake tdb so the filter "
                "families above work unchanged inside it.",
        "prefix": "decorator.<decoratorName>.",
        "vars": [
            v("decorator.<name>.blockMode", "enum",
              "hg/lib/decoratorUi.c:21",
              values=["Hide", "Overlay", "Adjacent"], default="Overlay"),
            v("decorator.<name>.glyphMode", "enum",
              "hg/lib/decoratorUi.c:23",
              values=["Hide", "Overlay", "Adjacent"], default="Overlay"),
            v("decorator.<name>.Overlaytoggle", "bool",
              "hg/lib/decoratorUi.c:19",
              note="Draw labels for blocks in overlay mode."),
            v("decorator.<name>.maxLabelBases", "int",
              "hg/lib/decoratorUi.c:26", default="200000"),
            v("decorator.<name>.filterBy.<field>", "list",
              "hg/lib/decoratorUi.c:176", multi=True,
              note="Whole filterBy family nests here."),
        ],
    },
    "species": {
        "what": "Per-species on/off, used by maf/wigMaf and by chain/net "
                "cross-species coloring.",
        "vars": [
            v("<species>", "bool", "hg/lib/hui.c:isSpeciesOn",
              note="One var per species in the maf, name is the db or "
                   "species name itself."),
            v("speciesOrder", "string", "hg/hgc/mafClick.c:609"),
        ],
    },
}

# ---------------------------------------------------------------------------
# LEVEL 3: the type layer.  Keyed by eCfgType where one exists
# (hg/inc/trackDb.h:439) plus the types that fall through cfgByCfgType.
# ---------------------------------------------------------------------------

TYPES = {
    "wig": {
        "cfgType": "cfgWig",
        "tdbTypes": ["wig", "bigWig", "bedGraph", "mathWig", "instaPort"],
        "cfgUi": "hg/lib/hui.c:wigCfgUi",
        "cart": "hg/lib/wiggleCart.c",
        "families": [],
        "vars": [
            v("minY", "float", "hg/inc/wiggle.h:MIN_Y", tdb="viewLimits"),
            v("maxY", "float", "hg/inc/wiggle.h:MAX_Y", tdb="viewLimits"),
            v("autoScale", "enum", "hg/lib/wiggleCart.c:448",
              values=["use vertical viewing range setting",
                      "auto-scale to data view",
                      "group auto-scale"],
              valuesSrc="hg/lib/hui.c:1973 wiggleScaleOptionsParent",
              aliases={"off": "use vertical viewing range setting",
                       "on": "auto-scale to data view",
                       "group": "group auto-scale"},
              tdb="autoScaleDefault",
              note="Two vocabularies, and this entry had the wrong one until "
                   "2026-08-08.  The cart holds one of the three long "
                   "literals.  on/off/group are the *trackDb* spellings, "
                   "translated to the literals at wiggleCart.c:454-459, and "
                   "they are never stored.  'cumulative' was in this list by "
                   "mistake: it is the C enum identifier wiggleScaleCumulative "
                   "(hui.h:576), not a value.  This matters more than a "
                   "documentation slip because wiggleScaleStringToEnum() "
                   "errAborts on an unknown value (hui.c:1985), so "
                   "autoScale=on in a cart CRASHES hgTracks instead of being "
                   "ignored.  A JSON writer must accept the aliases and store "
                   "the literal."),
            v("alwaysZero", "enum", "hg/inc/wiggle.h:ALWAYSZERO",
              values=["OFF", "ON"],
              valuesSrc="hg/lib/hui.c:2173 wiggleAlwaysZeroOptions"),
            v("lineBar", "enum", "hg/inc/wiggle.h:LINEBAR",
              values=["points", "bar"], tdb="graphTypeDefault",
              valuesSrc="hg/lib/hui.c:2017 wiggleGraphOptions"),
            v("transformFunc", "enum", "hg/inc/wiggle.h:TRANSFORMFUNC",
              values=["NONE", "LOG"],
              valuesSrc="hg/lib/hui.c:2145 wiggleTransformFuncOptions"),
            v("negateValues", "bool", "hg/inc/wiggle.h:DONEGATIVEMODE"),
            v("sequenceLogo", "bool", "hg/inc/wiggle.h:DOSEQUENCELOGOMODE"),
            v("horizGrid", "enum", "hg/inc/wiggle.h:HORIZGRID",
              values=["ON", "OFF"], tdb="gridDefault",
              valuesSrc="hg/lib/hui.c:2198 wiggleGridOptions"),
            v("yLineOnOff", "enum", "hg/inc/wiggle.h:YLINEONOFF",
              values=["OFF", "ON"],
              valuesSrc="hg/lib/hui.c:1943 wiggleYLineMarkOptions"),
            v("yLineMark", "float", "hg/inc/wiggle.h:YLINEMARK"),
            v("smoothingWindow", "enum", "hg/inc/wiggle.h:SMOOTHINGWINDOW",
              values=["OFF", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                      "11", "12", "13", "14", "15", "16"],
              valuesSrc="hg/lib/hui.c:1913 wiggleSmoothingOptions",
              note="The list used to be written 'off, 2, 3, 4, ..., 16', with "
                   "the ellipsis sitting in the values array as if it were a "
                   "value.  Enumerated now, because a generated validator "
                   "would have accepted the literal string '...'."),
            v("windowingFunction", "enum",
              "hg/inc/wiggle.h:WINDOWINGFUNCTION",
              values=["mean+whiskers", "maximum", "mean", "minimum", "sum"],
              valuesSrc="hg/lib/hui.c:1880 wiggleWindowingOptions"),
            v("aggregate", "enum", "hg/inc/wiggle.h:AGGREGATE",
              values=["none", "transparentOverlay", "solidOverlay",
                      "stacked", "add", "subtract"],
              valuesSrc="hg/lib/hui.c:2054 aggregateExtraValues "
                        "(hui.h:652 WIG_AGGREGATE_*)",
              note="Container-level, but readable at leaf level too."),
            v("viewFunc", "enum", "hg/inc/wiggle.h:VIEWFUNC",
              values=["showAll", "addAll", "subtractAll"],
              valuesSrc="hg/lib/hui.c:2118 viewFuncValues "
                        "(hui.h:636 WIG_VIEWFUNC_*)"),
            v("missingMethod", "enum", "hg/lib/hui.c:6257",
              note="How to render gaps in the data."),
            v("heightPer", "int", "hg/inc/wiggle.h:HEIGHTPER", default="128"),
        ],
        "tdbOnly": ["viewLimits", "viewLimitsMax", "defaultViewLimits",
                    "minLimit", "maxLimit", "maxHeightPixels", "gridDefault",
                    "autoScaleDefault", "graphType", "graphTypeDefault",
                    "spanList"],
    },
    "bedScore": {
        "cfgType": "cfgBedScore",
        "tdbTypes": ["bed", "bigBed", "bed<n> +", "bigNarrowPeak", "broadPeak"],
        "cfgUi": "hg/lib/hui.c:bedScoreCfgUi",
        "families": ["score", "numericFilter", "textFilter", "filterBy",
                     "highlightBy", "label", "cds", "decorator"],
        "vars": [
            v("colorField", "string", "hg/lib/hui.c:colorFieldsCfgUi",
              tdb="colorByStrand"),
            v("scoreLabel", "string", "hg/inc/hui.h:SCORE_LABEL"),
        ],
        "tdbOnly": ["noScoreFilter", "scoreMin", "labelFields",
                    "defaultLabelFields"],
    },
    "bedFilt": {
        "cfgType": "cfgBedFilt",
        "tdbTypes": ["bed (mrna-style filters)"],
        "cfgUi": "hg/lib/hui.c:bedFiltCfgUi",
        "families": [],
        "vars": [
            v("<filterName>", "string", "hg/lib/hui.c:bedFiltCfgUi",
              note="One var per filter declared in the track's mrnaUiData "
                   "(hg/hgTracks/bedTrack.c:427)."),
            v("<filterName>Type", "enum", "hg/hgTracks/bedTrack.c:434",
              values=["include", "exclude"]),
            v("<filterName>Logic", "enum", "hg/hgTracks/bedTrack.c:441",
              values=["and", "or"]),
        ],
    },
    "peak": {
        "cfgType": "cfgPeak",
        "tdbTypes": ["encodePeak", "narrowPeak", "broadPeak", "gappedPeak"],
        "cfgUi": "hg/lib/hui.c:encodePeakCfgUi",
        "families": ["score", "numericFilter"],
        "vars": [
            v("signalFilter", "float", "hg/inc/hui.h:SIGNAL_FILTER"),
            v("signalFilterLimits", "string", "hg/inc/hui.h:_LIMITS"),
            v("pValueFilter", "float", "hg/inc/hui.h:PVALUE_FILTER"),
            v("qValueFilter", "float", "hg/inc/hui.h:QVALUE_FILTER"),
        ],
    },
    "genePred": {
        "cfgType": "cfgGenePred",
        "tdbTypes": ["genePred", "bigGenePred"],
        "cfgUi": "hg/lib/hui.c:genePredCfgUi",
        "families": ["cds", "indel", "label", "gencodeShow", "numericFilter",
                     "filterBy", "score", "decorator"],
        "vars": [
            v("type", "enum", "hg/lib/hui.c:genePredCfgUi",
              note="Which of several gene ID sources to label with."),
            v("geneClasses", "string", "hg/inc/hui.h:GENEPRED_CLASS_VAR",
              note="trackDb-declared class list; per-class colors are the "
                   "trackDb settings gClass_<class>, not cart vars."),
            v("itemClassTbl", "string", "hg/inc/hui.h:GENEPRED_CLASS_TBL"),
        ],
    },
    "psl": {
        "cfgType": "cfgPsl",
        "tdbTypes": ["psl", "bigPsl", "psl xeno <db>", "chain (via snake)"],
        "cfgUi": "hg/lib/hui.c:pslCfgUi",
        "families": ["cds", "indel", "score", "label"],
        "vars": [
            v("color", "enum", "hg/lib/hui.c:crossSpeciesCfgUi",
              values=["on", "off"],
              note="Chromosome coloring for cross-species psl.  Default "
                   "'on' unless trackDb colorChromDefault off."),
            v("showPatentSequences", "bool",
              "hg/inc/hui.h:SHOW_PATENT_SEQUENCES_SUFFIX"),
            v("chromFilter", "string",
              "hg/hgTracks/pslTrack.c:connectedLfFromPslsInRange"),
            v("pslSequenceBases", "string", "hg/inc/hui.h:PSL_SEQUENCE_BASES",
              default="no"),
        ],
    },
    "chain": {
        "cfgType": "cfgChain",
        "tdbTypes": ["chain", "bigChain"],
        "cfgUi": "hg/lib/hui.c:chainCfgUi",
        "families": ["score"],
        "vars": [
            v("chainColor", "enum", "hg/inc/chainCart.h:OPT_CHROM_COLORS",
              values=["Chromosome", "Normalized Score", "Black"]),
            v("chromFilter", "string", "hg/inc/chainCart.h:OPT_CHROM_FILTER",
              note="Restrict to chains against one query chrom."),
            v("color", "enum", "hg/lib/hui.c:chainCfgUi",
              values=["on", "off"],
              note="Cross-species chromosome coloring, shared with psl."),
        ],
        "tdbOnly": ["chainNormScoreAvailable"],
    },
    "net": {
        "cfgType": "cfgNetAlign",
        "tdbTypes": ["netAlign"],
        "cfgUi": "hg/lib/hui.c:netAlignCfgUi",
        "families": [],
        "vars": [
            v("netColor", "enum", "hg/inc/netCart.h:NET_COLOR",
              values=["Chromosome", "Gray scale"]),
            v("netLevel", "enum", "hg/inc/netCart.h:NET_LEVEL",
              values=["All levels", "level 1 only", "level 2 only",
                      "level 3 only", "level 4 only", "level 5 only",
                      "level 6 only"]),
            v("netTopOnly", "bool", "hg/inc/hui.h:NET_OPT_TOP_ONLY"),
        ],
    },
    "wigMaf": {
        "cfgType": "cfgWigMaf",
        "tdbTypes": ["wigMaf", "bigMaf", "maf"],
        "cfgUi": "hg/lib/hui.c:wigMafCfgUi",
        "families": ["species", "wig (embedded conservation wiggle)"],
        "vars": [
            v("mafDot", "bool", "hg/inc/hui.h:MAF_DOT_VAR",
              note="Show dots for matching bases."),
            v("mafChain", "bool", "hg/inc/hui.h:MAF_CHAIN_VAR",
              note="Display chains between blocks."),
            v("mafShowSnp", "bool", "hg/inc/hui.h:MAF_SHOW_SNP"),
            v("mafGenePred", "string", "hg/inc/hui.h:MAF_GENEPRED_VAR",
              note="Gene track used for codon translation."),
            v("mafFrame", "enum", "hg/inc/hui.h:MAF_FRAMING_VAR"),
            v("codons", "enum", "hg/hgTracks/wigMafTrack.c:2085"),
            v("frames", "string", "hg/hgTracks/wigMafTrack.c:2079"),
            v("baseColors", "string", "hg/hgTracks/wigMafTrack.c:2405"),
            v("baseColorsOffset", "int",
              "hg/hgTracks/wigMafTrack.c:2407"),
            v("<wigTrack>.<wigVar>", "varies",
              "hg/hgTracks/wigMafTrack.c:2760",
              note="The conservation wiggle is configured as a nested "
                   "namespace under the wigMaf track."),
        ],
    },
    "bam": {
        "cfgType": "cfgBam",
        "tdbTypes": ["bam", "cram"],
        "cfgUi": "hg/lib/hui.c:bamCfgUi",
        "families": ["cds", "indel", "wig (when bamWigMode is on)"],
        "vars": [
            v("pairEndsByName", "bool", "hg/inc/hui.h:BAM_PAIR_ENDS_BY_NAME"),
            v("showNames", "bool", "hg/inc/hui.h:BAM_SHOW_NAMES"),
            v("minAliQual", "int", "hg/inc/hui.h:BAM_MIN_ALI_QUAL",
              default="0"),
            v("bamColorMode", "enum", "hg/inc/hui.h:BAM_COLOR_MODE",
              values=["gray", "strand", "tag", "off"], default="strand"),
            v("bamGrayMode", "enum", "hg/inc/hui.h:BAM_GRAY_MODE",
              values=["aliQual", "baseQual", "unpaired"], default="aliQual"),
            v("bamColorTag", "string", "hg/inc/hui.h:BAM_COLOR_TAG",
              default="YC"),
            v("bamWigMode", "bool", "hg/inc/hui.h:BAMWIG_MODE",
              note="Draw coverage as a wiggle; pulls in the wig group."),
        ],
    },
    "vcf": {
        "cfgType": "cfgVcf",
        "tdbTypes": ["vcfTabix", "vcf", "vcfPhasedTrio"],
        "cfgUi": "hg/lib/vcfUi.c:vcfCfgUi",
        "families": ["filterBy"],
        "vars": [
            v("hapClusterEnabled", "bool", "hg/inc/vcfUi.h:16"),
            v("hapClusterHeight", "int", "hg/inc/vcfUi.h:13"),
            v("hapClusterMethod", "enum", "hg/inc/vcfUi.h:17",
              values=["centerWeighted", "fileOrder", "treeFile"]),
            v("hapClusterColorBy", "enum", "hg/inc/vcfUi.h:23",
              values=["altOnly", "function", "refAlt", "base"]),
            v("hapClusterTreeAngle", "enum", "hg/inc/vcfUi.h:30",
              values=["triangle", "rectangle"]),
            v("sampleColorFile", "string", "hg/inc/vcfUi.h:35"),
            v("applyMinQual", "bool", "hg/inc/vcfUi.h:40"),
            v("minQual", "float", "hg/inc/vcfUi.h:44"),
            v("minFreq", "float", "hg/inc/vcfUi.h:52"),
            v("excludeFilterValues", "list", "hg/inc/vcfUi.h:49", multi=True),
            v("showHardyWeinberg", "bool", "hg/inc/vcfUi.h:38"),
            v("vcfSampleOrder", "string", "hg/inc/vcfUi.h:57",
              note="Drag-and-drop sample order, comma separated."),
            v("doDefaultLabel", "bool", "hg/inc/vcfUi.h:59"),
            v("doAliasLabel", "bool", "hg/inc/vcfUi.h:60"),
            v("hideParents", "bool", "hg/inc/vcfUi.h:61"),
            v("sortChildBelow", "bool", "hg/inc/vcfUi.h:63"),
            v("vcfPhasedColorBy", "enum", "hg/inc/vcfUi.h:65",
              values=["mendelDiff", "deNovo", "function", "noColor"]),
            v("centerVariantChrom", "hidden", "hg/lib/vcfUi.c:33",
              note="Haplotype-sorting anchor.  Position state stored on the "
                   "track, which is unusual and worth flagging in a "
                   "rewrite."),
            v("centerVariantPos", "hidden", "hg/lib/vcfUi.c:35"),
            v("centerVariantName", "hidden", "hg/lib/vcfUi.c:39"),
        ],
    },
    "snake": {
        "cfgType": "cfgSnake",
        "tdbTypes": ["halSnake", "chain (snake mode)", "pslSnake"],
        "cfgUi": "hg/lib/snakeUi.c:snakeCfgUi",
        "families": ["score"],
        "vars": [
            v("showSnpWidth", "int", "hg/inc/snakeUi.h:12"),
            v("colorBy", "enum", "hg/inc/snakeUi.h:24",
              values=["byStrand", "byChromosome", "none"]),
            v("chromFilter", "string",
              "hg/hgTracks/halSnakeTrack.c:snakeLoadItems"),
            v("coalescent", "string", "hg/hgTracks/halSnakeTrack.c:1764",
              note="HAL only."),
            v("color", "enum", "hg/hgTracks/halSnakeTrack.c:snakeMethods"),
        ],
    },
    "long": {
        "cfgType": "cfgLong",
        "tdbTypes": ["longTabix"],
        "cfgUi": "hg/lib/longRange.c:longRangeCfgUi",
        "families": [],
        "vars": [
            v("heightPer", "int", "hg/inc/longRange.h:14", default="200"),
            v("minScore", "float", "hg/inc/longRange.h:18", default="0"),
        ],
    },
    "interact": {
        "cfgType": "cfgInteract",
        "tdbTypes": ["interact", "bigInteract"],
        "cfgUi": "hg/lib/interactUi.c:interactCfgUi",
        "families": ["score", "filterBy"],
        "vars": [
            v("heightPer", "int", "hg/inc/interactUi.h:9", default="200"),
            v("minScore", "float", "hg/inc/interactUi.h:13"),
            v("draw", "enum", "hg/inc/interactUi.h:38",
              values=["line", "ellipse", "curve"]),
            v("dashes", "bool", "hg/inc/interactUi.h:35",
              note="Dash the line to show direction."),
            v("endsVisible", "enum", "hg/inc/interactUi.h:44",
              values=["two", "one", "any"]),
            v("cluster", "enum", "hg/inc/interactUi.h:50",
              values=["none", "source", "target"]),
            v("detailsBoxesEnabled", "bool", "hg/inc/interactUi.h:31"),
        ],
        "tdbOnly": ["interactDirectional", "offsetSource", "offsetTarget",
                    "clusterSource", "clusterTarget", "interactUp"],
    },
    "hic": {
        "cfgType": "cfgHic",
        "tdbTypes": ["hic"],
        "cfgUi": "hg/lib/hicUi.c:hicCfgUi",
        "families": [],
        "vars": [
            v("drawMode", "enum", "hg/inc/hicUi.h:31",
              values=["triangle", "square", "arc"]),
            v("inverted", "bool", "hg/inc/hicUi.h:36"),
            v("normalization", "string", "hg/inc/hicUi.h:37"),
            v("resolution", "string", "hg/inc/hicUi.h:38"),
            v("autoscale", "bool", "hg/inc/hicUi.h:39",
              note="Lower case, unlike the wig autoScale."),
            v("max", "float", "hg/inc/hicUi.h:40", tdb="saturationScore"),
            v("color", "color", "hg/inc/hicUi.h:42", default="#ff0000"),
            v("bgColor", "color", "hg/inc/hicUi.h:43", default="#ffffff"),
            v("maxDistance", "int", "hg/inc/hicUi.h:47",
              tdb="hicDistanceMax"),
            v("minDistance", "int", "hg/inc/hicUi.h:48",
              tdb="hicDistanceMin"),
            v("hicArcLimit", "int", "hg/inc/hicUi.h:50"),
            v("hicArcLimitEnabled", "bool", "hg/inc/hicUi.h:51"),
        ],
    },
    "barChart": {
        "cfgType": "cfgBarChart",
        "tdbTypes": ["barChart", "bigBarChart"],
        "cfgUi": "hg/lib/barChartUi.c:barChartCfgUi",
        "families": ["score", "filterBy", "label"],
        "vars": [
            v("colorScheme", "enum", "hg/inc/barChartUi.h:13",
              values=["rainbow", "user"]),
            v("logTransform", "bool", "hg/inc/barChartUi.h:22"),
            v("maxViewLimit", "float", "hg/inc/barChartUi.h:29",
              tdb="maxLimit"),
            v("categories", "list", "hg/inc/barChartUi.h:38", multi=True,
              note="Which categories to show; 'All' means every one."),
            v("noWhiteout", "bool", "hg/inc/barChartUi.h:58"),
        ],
        "tdbOnly": ["barChartBars", "barChartLabel", "barChartColors",
                    "barChartCategoryUrl", "barChartUnit", "barChartMetric",
                    "barChartMaxSize", "barChartSizeWindows",
                    "barChartBarMinWidth", "barChartBarMinPadding",
                    "barChartLimit"],
    },
    "lolly": {
        "cfgType": "cfgLollipop",
        "tdbTypes": ["bigLolly"],
        "cfgUi": "hg/lib/hui.c:lollyCfgUi",
        "families": ["score", "numericFilter", "filterBy", "label"],
        "vars": [
            v("heightPer", "int", "hg/lib/hui.c:lollyCfgUi"),
            v("autoScale", "enum", "hg/hgTracks/lollyTrack.c:485",
              values=["use vertical viewing range setting",
                      "auto-scale to data view",
                      "group auto-scale"],
              valuesSrc="hg/lib/hui.c:1973 wiggleScaleOptionsParent",
              aliases={"off": "use vertical viewing range setting",
                       "on": "auto-scale to data view",
                       "group": "group auto-scale"},
              note="Same var, same reader and same fix as wig.autoScale: "
                   "lollyTrack.c:485 calls wigFetchAutoScaleWithCart(), so the "
                   "cart vocabulary is the wiggle one, not on/off.  This entry "
                   "carried on/off until 2026-08-08."),
            v("minY", "float", "hg/lib/hui.c:lollyCfgUi"),
            v("maxY", "float", "hg/lib/hui.c:lollyCfgUi"),
            v("popMethod", "enum", "hg/lib/hui.c:lollyCfgUi",
              tdb="popMethod"),
        ],
    },
    "bigDbSnp": {
        "cfgType": "cfgBigDbSnp",
        "tdbTypes": ["bigDbSnp"],
        "cfgUi": "hg/lib/hui.c:bigDbSnpCfgUi",
        "families": ["filterBy", "label"],
        "vars": [
            v("minMaf", "float", "hg/lib/hui.c:4954"),
            v("freqProj", "enum", "hg/lib/hui.c:freqSourceSelect",
              note="Which frequency project supplies MAF."),
            v("label.rsId", "bool", "hg/hgTracks/variation.c:2712"),
            v("label.majMin", "bool", "hg/hgTracks/variation.c:2723"),
            v("label.refAlt", "bool", "hg/hgTracks/variation.c:2715"),
            v("label.func", "bool", "hg/hgTracks/variation.c:2737"),
            v("label.maf", "bool", "hg/hgTracks/variation.c:2730"),
            v("geneTrack", "string", "hg/lib/hui.c:4881"),
            v("geneTracks", "list", "hg/lib/hui.c:4875", multi=True),
        ],
    },
    "bigRmsk": {
        "cfgType": "cfgBigRmsk",
        "tdbTypes": ["bigRmsk"],
        "cfgUi": "hg/lib/hui.c:bigRmskCfgUi",
        "families": [],
        "prefix": "bigrmsk.",
        "vars": [
            v("bigrmsk.showUnalignedExtents", "bool",
              "hg/inc/bigRmskUi.h:10"),
            v("bigrmsk.showLabels", "bool", "hg/inc/bigRmskUi.h:14"),
            v("bigrmsk.origPackViz", "bool", "hg/inc/bigRmskUi.h:18"),
            v("bigrmsk.nameFilter", "string", "hg/inc/bigRmskUi.h:22",
              default="*"),
            v("bigrmsk.regexpFilter", "string", "hg/inc/bigRmskUi.h:26"),
        ],
    },
    "gtexGene": {
        "cfgType": "(cfgBedScore plus gtexGeneUi)",
        "tdbTypes": ["bed 12 + (gtexGene*)"],
        "cfgUi": "hg/lib/gtexUi.c:gtexGeneUi",
        "families": ["score"],
        "vars": [
            v("colorScheme", "enum", "hg/inc/gtexUi.h:9",
              values=["rainbow", "gtex"]),
            v("logTransform", "bool", "hg/inc/gtexUi.h:18"),
            v("maxViewLimit", "float", "hg/inc/gtexUi.h:23"),
            v("samples", "enum", "hg/inc/gtexUi.h:29",
              values=["all", "sex", "age"]),
            v("comparison", "enum", "hg/inc/gtexUi.h:39",
              values=["mirror", "difference"]),
            v("graphType", "enum", "hg/inc/gtexUi.h:45",
              values=["raw", "normalized"]),
            v("tissues", "list", "hg/inc/gtexUi.h:51", multi=True),
            v("codingOnly", "bool", "hg/inc/gtexUi.h:54"),
            v("showExons", "bool", "hg/inc/gtexUi.h:58"),
            v("noWhiteout", "bool", "hg/inc/gtexUi.h:62"),
            v("label", "enum", "hg/inc/gtexUi.h:66",
              values=["name", "accession", "both"]),
        ],
    },
    "gtexEqtlCluster": {
        "cfgType": "(cfgBedScore plus gtexEqtlClusterUi)",
        "tdbTypes": ["bed 9 + (gtexEqtlCluster*)"],
        "cfgUi": "hg/lib/gtexUi.c:gtexEqtlClusterUi",
        "families": ["score"],
        "vars": [
            v("effect", "float", "hg/inc/gtexUi.h:74"),
            v("prob", "float", "hg/inc/gtexUi.h:76"),
            v("gene", "string", "hg/inc/gtexUi.h:78"),
            v("tissueColor", "bool", "hg/inc/gtexUi.h:79"),
            v("tissues", "list", "hg/inc/gtexUi.h:51", multi=True),
        ],
    },
    "expRatio": {
        "cfgType": "(falls through cfgByCfgType)",
        "tdbTypes": ["expRatio", "array"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:expRatioUi",
        "families": [],
        "vars": [
            v("color", "enum", "hg/hgTrackUi/hgTrackUi.c:1360",
              note="Red/green vs blue/yellow etc."),
            v("type", "enum", "hg/hgTracks/expRatioTracks.c:lfsFromAffyBed"),
            v("combine", "enum", "hg/lib/microarray.c:1167"),
            v("subset", "enum", "hg/lib/microarray.c:1174"),
            v("expDrawExons", "bool", "hg/hgTrackUi/hgTrackUi.c:1347"),
            v("heightPer", "int", "hg/hgTrackUi/hgTrackUi.c:2026"),
        ],
    },
    "sample": {
        "cfgType": "(falls through cfgByCfgType)",
        "tdbTypes": ["sample"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:genericWiggleUi",
        "families": [],
        "vars": [
            v("heightPer", "int", "hg/hgTracks/sampleTracks.c:sampleTotalHeight"),
            v("min.cutoff", "float", "hg/hgTrackUi/hgTrackUi.c:2029"),
            v("max.cutoff", "float", "hg/hgTrackUi/hgTrackUi.c:2030"),
            v("linear.interp", "enum", "hg/hgTrackUi/hgTrackUi.c:2027"),
            v("interp.gap", "int", "hg/hgTrackUi/hgTrackUi.c:2031"),
            v("fill", "bool", "hg/hgTrackUi/hgTrackUi.c:2028"),
            v("anti.alias", "bool", "hg/hgTracks/sampleTracks.c:272"),
        ],
    },
    "chromGraph": {
        "cfgType": "(falls through cfgByCfgType)",
        "tdbTypes": ["chromGraph"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:chromGraphUi",
        "families": [],
        "note": "Exception: names are cgs_<track>_<var>, prefix first.  See "
                "the exceptions section.",
        "vars": [
            v("pixels", "int", "hg/lib/chromGraph.c:chromGraphVarName",
              sep="cgs_<track>_"),
            v("minVal", "float", "hg/lib/chromGraph.c:301",
              sep="cgs_<track>_"),
            v("maxVal", "float", "hg/lib/chromGraph.c:303",
              sep="cgs_<track>_"),
            v("maxGapToFill", "int", "hg/lib/chromGraph.c:299",
              sep="cgs_<track>_"),
        ],
    },
    "factorSource": {
        "cfgType": "(falls through cfgByCfgType)",
        "tdbTypes": ["factorSource"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:factorSourceUi",
        "families": ["score", "filterBy"],
        "vars": [
            v("highlightMotifs", "bool", "hg/hgTrackUi/hgTrackUi.c:2778"),
            v("showCellAbbrevs", "bool", "hg/hgTrackUi/hgTrackUi.c:2789"),
            v("showExpCounts", "bool", "hg/hgTrackUi/hgTrackUi.c:2785"),
        ],
    },
    "ld": {
        "cfgType": "(name/type special case)",
        "tdbTypes": ["ld2", "hapmapLd*", "rertyHumanDiversityLd"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:ldUi",
        "families": [],
        "note": "All underscore-separated, one of the oldest var sets.",
        "vars": [
            v("_val", "enum", "hg/hgTracks/variation.c:ldDrawItems", sep="_",
              values=["rsquared", "dprime", "lod"]),
            v("_trm", "bool", "hg/hgTracks/variation.c:ldDrawItems", sep="_",
              note="Trim to triangle."),
            v("_inv", "bool", "hg/hgTracks/variation.c:ldDrawItems", sep="_",
              note="Invert the display."),
            v("_pos", "color", "hg/hgTracks/variation.c:ldShadesInit",
              sep="_"),
            v("_out", "enum", "hg/hgTracks/variation.c:getOutlineColor",
              sep="_"),
            v("_gap", "bool", "hg/hgTracks/variation.c:ldDrawDense", sep="_"),
        ],
    },
    "gvf": {
        "cfgType": "(falls through cfgByCfgType)",
        "tdbTypes": ["gvf"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:gvUi",
        "families": [],
        "vars": [
            v("_<origin>", "bool", "hg/hgTracks/gvfTrack.c:gvfItemName",
              sep="_",
              note="One per variant origin: _germ _som _dnovo _bip _mat "
                   "_pat _unip _unk."),
        ],
    },
    "snp125": {
        "cfgType": "(name special case, snpVersion >= 125)",
        "tdbTypes": ["snp125 .. snp15x"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:snp125Ui",
        "families": [],
        "vars": [
            v("include_<attribute>", "list",
              "hg/cgilib/snp125Ui.c:605", multi=True,
              note="Attribute is one of molType class valid func locType "
                   "exceptions bitfields.  Values are the kept categories."),
            v("colorSource", "enum", "hg/cgilib/snp125Ui.c:687",
              values=["locType", "class", "valid", "func", "molType",
                      "exceptions", "bitfields", "alleleFreq"]),
            v("<attribute><Value>", "color",
              "hg/hgTrackUi/hgTrackUi.c:snp125RemoveColorVars",
              note="One color var per attribute value, e.g. "
                   "<track>.funcMissense.  Nested leaf level keyed by "
                   "attribute then value."),
            v("minAvHet", "float", "hg/hgTrackUi/hgTrackUi.c:416"),
            v("maxWeight", "int", "hg/hgTrackUi/hgTrackUi.c:424"),
            v("minSubmitters", "int", "hg/hgTrackUi/hgTrackUi.c:440"),
            v("minMinorAlFreq", "float", "hg/hgTrackUi/hgTrackUi.c:444"),
            v("maxMinorAlFreq", "float", "hg/hgTrackUi/hgTrackUi.c:448"),
            v("minAlFreq2N", "int", "hg/hgTrackUi/hgTrackUi.c:455"),
            v("extendedNames", "bool", "hg/hgTrackUi/hgTrackUi.c:726"),
            v("allelesDbSnpStrand", "bool", "hg/hgTrackUi/hgTrackUi.c:745"),
        ],
    },
    "blast": {
        "cfgType": "(name special case)",
        "tdbTypes": ["blastHg*KG, blastDm*, blastSacCer1SG, mrnaMap*, "
                     "mrnaXeno*"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:blastUi",
        "families": ["label", "cds"],
        "vars": [
            v("cmode", "enum", "hg/hgTracks/simpleTracks.c:blastColor",
              note="Color mode: black, by identity, by species."),
        ],
    },
    "pubs": {
        "cfgType": "(name special case, startsWith pubs)",
        "tdbTypes": ["pubs*"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:pubsUi",
        "families": [],
        "vars": [
            v("pubsFilterKeywords", "string",
              "hg/hgTracks/pubsTracks.c:pubsLoadKeywordYearItems"),
            v("pubsFilterYear", "string",
              "hg/hgTracks/pubsTracks.c:pubsLoadKeywordYearItems"),
            v("pubsFilterPublisher", "string",
              "hg/hgTracks/pubsTracks.c:pubsLoadKeywordYearItems"),
            v("pubsColorBy", "enum", "hg/hgTracks/pubsTracks.c:pubsMakeExtra"),
        ],
    },
    "transMap": {
        "cfgType": "(name special case, transMapAln*)",
        "tdbTypes": ["psl (transMap)"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:transMapUI",
        "families": ["label", "cds", "indel"],
        "vars": [],
    },
    "retroGene": {
        "cfgType": "(name special case, ucscRetro*, retroMrnaInfo*)",
        "tdbTypes": ["psl (retro)"],
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:retroGeneUI",
        "families": ["label", "cds", "indel"],
        "vars": [],
    },
    "mrna": {
        "cfgType": "(name special case: mrna, est, xenoMrna, intronEst, ...)",
        "tdbTypes": ["psl"],
        "cfgUi": "hg/lib/hui.c:mrnaCfgUi",
        "families": ["cds", "indel", "bedFilt"],
        "vars": [],
    },
}

# ---------------------------------------------------------------------------
# LEVEL 3b: tracks whose vars are written as literals rather than composed
# from tdb->track.  They still sit under <track>., so they belong in the
# hierarchy, but grep for the suffix alone will not find them.
# ---------------------------------------------------------------------------

BY_TRACK_NAME = {
    "stsMap": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:stsMapUi",
        "vars": [
            v("filter", "enum", "hg/hgTrackUi/hgTrackUi.c:91",
              values=["blue", "red", "green", "black"]),
            v("type", "enum", "hg/hgTrackUi/hgTrackUi.c:91"),
        ],
    },
    "stsMapMouseNew / stsMapRat": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:stsMapMouseUi",
        "vars": [
            v("filter", "enum", "hg/hgTrackUi/hgTrackUi.c:101"),
            v("type", "enum", "hg/hgTrackUi/hgTrackUi.c:101"),
        ],
    },
    "cbr_waba": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:cbrWabaUi",
        "vars": [
            v("filter", "enum", "hg/hgTrackUi/hgTrackUi.c:1223"),
            v("type", "enum", "hg/hgTrackUi/hgTrackUi.c:1223"),
            v("start", "int", "hg/hgTrackUi/hgTrackUi.c:1223"),
            v("end", "int", "hg/hgTrackUi/hgTrackUi.c:1223"),
        ],
    },
    "fishClones": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:fishClonesUi",
        "vars": [
            v("filter", "enum", "hg/hgTrackUi/hgTrackUi.c:1236"),
            v("type", "enum", "hg/hgTrackUi/hgTrackUi.c:1236"),
        ],
    },
    "recombRate / recombRateRat / recombRateMouse": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:recombRateUi",
        "vars": [v("type", "enum", "hg/hgTrackUi/hgTrackUi.c:1246")],
    },
    "cghNci60": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:cghNci60Ui",
        "vars": [
            v("color", "enum", "hg/hgTrackUi/hgTrackUi.c:1270",
              values=["rg", "rb", "gr"]),
            v("type", "enum", "hg/hgTrackUi/hgTrackUi.c:1270"),
        ],
    },
    "rosetta": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:rosettaUi",
        "vars": [
            v("et", "enum", "hg/hgTrackUi/hgTrackUi.c:1456"),
            v("type", "enum", "hg/hgTrackUi/hgTrackUi.c:1456"),
        ],
    },
    "switchDbTss": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:switchDbScoreUi",
        "vars": [
            v("scoreFilter", "int", "hg/hgTrackUi/hgTrackUi.c:1475",
              default="10"),
            v("pseudo", "bool", "hg/hgTrackUi/hgTrackUi.c:1475"),
        ],
    },
    "affyTranscriptome": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:affyTranscriptomeUi",
        "vars": [
            v("fill", "bool", "hg/hgTrackUi/hgTrackUi.c:2479"),
            v("heightPer", "int", "hg/hgTrackUi/hgTrackUi.c:2479",
              default="100"),
        ],
    },
    "ancientR": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:ancientRUi",
        "vars": [v("minLength", "int", "hg/hgTrackUi/hgTrackUi.c:2500",
                   default="50")],
    },
    "affyTransfrags": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:affyTransfragUi",
        "vars": [
            v("skipDups", "bool", "hg/hgTrackUi/hgTrackUi.c:2511"),
            v("skipPseudos", "bool", "hg/hgTrackUi/hgTrackUi.c:2511"),
        ],
    },
    "oreganno": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:oregannoUi",
        "vars": [
            v("hgvs", "bool", "hg/hgTrackUi/hgTrackUi.c:952",
              note="Via labelMakeCheckBox, which is generic: the caller "
                   "picks the suffix."),
            v("common", "bool", "hg/hgTrackUi/hgTrackUi.c:953"),
        ],
        "note": "Its type filters are NOT track-scoped: they use the global "
                "names in oregannoTypeString[].",
    },
    "lrg": {
        "cfgUi": "hg/lib/hui.c:lrgCfgUi",
        "vars": [],
        "note": "cds + indel families only.",
    },
    "gene ID label configs": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:geneIdConfig and friends",
        "vars": [
            v("label", "list", "hg/hgTrackUi/hgTrackUi.c:geneIdConfig",
              multi=True,
              note="Shared shape for refGene, ncbiGene, xenoRefGene, "
                   "knownGene, ensGene, ensGeneNonCoding, vegaGeneComposite, "
                   "rgdGene2, hg17Kg, refSeqComposite, omimGene, omimGene2, "
                   "omimLocation, ucscRetro*, transMapAln*.  Each supplies a "
                   "different value list."),
        ],
    },
    "transRegCode": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c:transRegCodeUi",
        "vars": [],
        "note": "Explanatory text only, no controls.",
    },
    "wikiTrack / quickLiftChain / hgPcrResult / cgapSage": {
        "cfgUi": "hg/hgTrackUi/hgTrackUi.c",
        "vars": [],
        "note": "No cart vars of their own at UI time.",
    },
}

# ---------------------------------------------------------------------------
# Vars that configure exactly one track but are NOT scoped by track name.
# These are the ones an accessor layer cannot generate from tdb->track.
# ---------------------------------------------------------------------------

GLOBAL_BUT_TRACK_SPECIFIC = [
    v("dbRIP.genoRegion", "enum", "hg/inc/hui.h:GENO_REGION", sep="",
      default="any", note="retroposons / dbRIP* tracks.  'dbRIP' is a "
                          "pseudo-namespace, not a track name."),
    v("dbRIP.polySource", "enum", "hg/inc/hui.h:POLY_SOURCE", sep="",
      default="don't care"),
    v("dbRIP.polySubFamily", "enum", "hg/inc/hui.h:POLY_SUBFAMILY", sep="",
      default="any"),
    v("dbRIP.ethnicGroup", "enum", "hg/inc/hui.h:ETHNIC_GROUP", sep="",
      default="any"),
    v("dbRIP.ethnicExcInc", "enum", "hg/inc/hui.h:ETHNIC_GROUP_EXCINC",
      sep="", values=["include", "exclude"]),
    v("tfbsConsSitesCutoff", "float", "hg/inc/hui.h:TFBS_SITES_CUTOFF",
      sep="", default="2.33", note="tfbsConsSites track."),
    v("ucsfdemoER", "enum", "hg/inc/hui.h:UCSF_DEMO_ER", sep="",
      default="no filter", note="CGHBreastCancerUCSF track."),
    v("ucsfdemoPR", "enum", "hg/inc/hui.h:UCSF_DEMO_PR", sep="",
      default="no filter"),
    v("hapmapSnps_popCount", "enum", "hg/inc/hui.h:HAP_POP_COUNT", sep=""),
    v("hapmapSnps_isMixed", "enum", "hg/inc/hui.h:HAP_POP_MIXED", sep=""),
    v("hapmapSnps_monomorphic_<pop>", "bool",
      "hg/inc/hui.h:HAP_MONO_PREFIX", sep=""),
    v("hapmapSnps_ortho_<species>", "bool",
      "hg/inc/hui.h:HAP_ORTHO_PREFIX", sep=""),
    v("hapmapSnps_orthoQual_<species>", "int",
      "hg/inc/hui.h:HAP_ORTHO_QUAL_PREFIX", sep=""),
    v("exprssn.color", "enum", "hg/hgTrackUi/hgTrackUi.c:1287", sep="",
      values=["rg", "rb"],
      note="Shared by affy, affyAllExon and rosetta: one namespace, three "
           "tracks."),
    v("hgt.affyPhase2.tnfg", "enum", "hg/hgTrackUi/hgTrackUi.c:2409", sep="",
      note="affyTxnPhase2 track, parked in the hgt.* namespace."),
    v("snp125ColorSource and the snp125*OldColorVars arrays", "string",
      "hg/cgilib/snp125Ui.c:snp125OldColorVarToNew", sep="",
      note="Pre-<track>. spellings, still read for backward compatibility "
           "and rewritten to the track-scoped form."),
]

# ---------------------------------------------------------------------------
# Other CGIs that keep per-track state in the same cart.  hgTracks and
# hgTrackUi are not the only writers, so an accessor layer that lives in
# hg/lib has to cover these too.
# ---------------------------------------------------------------------------

OTHER_CGIS = {
    "hgc": {
        "what": "The details page reads most of the same vars hgTracks does, "
                "but it also owns a few of its own, and in two places it "
                "spells a shared var differently.",
        "vars": [
            v("speciesOrder", "string", "hg/hgc/mafClick.c:609",
              note="maf and rnaFold details pages; user-chosen species "
                   "order."),
            v("vis", "enum", "hg/hgc/mafClick.c:584",
              note="Per-species visibility on the maf details page."),
            v("_pairEndsByName", "bool", "hg/hgc/bamClick.c:271", sep="_",
              note="INCONSISTENCY: hui.c writes this as "
                   "<track>.pairEndsByName (dot), hgc reads it with an "
                   "underscore, so the details page does not follow the UI "
                   "setting.  Worth reconciling before either form is baked "
                   "into a schema."),
            v("_geneTrack", "list", "hg/hgc/hgc.c:19635", sep="_",
              multi=True,
              note="snp125-era gene tracks for functional annotation, "
                   "written by hgTrackUi.c:191.  Not to be confused with the "
                   "bigDbSnp <track>.geneTrack: separate features, each "
                   "internally consistent."),
            v("interProXref", "bool", "hg/hgc/lowelab.c:753",
              note="Lowe lab tracks."),
        ],
    },
    "hgc extended DNA": {
        "what": "The Extended DNA page (hg/hgc/hgc.c:doGetDnaExtended1) keeps "
                "one case/color set per track.  A real per-track group that "
                "no cfgUi function knows about.",
        "vars": [
            v("_case", "bool", "hg/hgc/hgc.c:5746", sep="_",
              note="Upper case this track's bases."),
            v("_u", "bool", "hg/hgc/hgc.c:5748", sep="_", note="Underline."),
            v("_b", "bool", "hg/hgc/hgc.c:5750", sep="_", note="Bold."),
            v("_i", "bool", "hg/hgc/hgc.c:5752", sep="_", note="Italic."),
            v("_red", "int", "hg/hgc/hgc.c:5754", sep="_", note="0-255."),
            v("_green", "int", "hg/hgc/hgc.c:5756", sep="_", note="0-255."),
            v("_blue", "int", "hg/hgc/hgc.c:5758", sep="_", note="0-255."),
        ],
    },
    "hgTables": {
        "what": "hgTables addresses the SAME data by db and table rather than "
                "by track, so its per-dataset state does not live under the "
                "track name at all.  This is the biggest structural mismatch "
                "in the cart: two CGIs, two namespaces, one dataset.",
        "vars": [
            v("hgta_fil.v.<db>.<table>.<field>.pat", "string",
              "hg/hgTables/filterFields.c:646", sep="",
              note="Filter text box.  Sibling types are .dd (dropdown), "
                   ".cmp (comparison), .rawQuery, .maxOutput."),
            v("hgta_fs.check.<db>.<table>.<field>", "bool",
              "hg/hgTables/filterFields.c:251", sep="",
              note="Output field selection."),
            v("hgta_subtrackMerge*", "string",
              "hg/hgTables/hgTables.h:534", sep="",
              note="Primary, Op, MoreThreshold, LessThreshold, WigOp, "
                   "RequireAll, UseMinScore, MinScore."),
            v("_sel", "bool", "hg/hgTables/compositeTrack.c:67", sep="_",
              note="The one place hgTables does use the track-scoped form: "
                   "it reads subtrack selection the same way hgTracks does."),
            v("hgta_track, hgta_table, hgta_database", "string",
              "hg/hgTables/hgTables.h:467", sep="",
              note="Current selection, session-scoped rather than "
                   "per-track."),
        ],
    },
}

# ---------------------------------------------------------------------------
# Track-scoped state that does NOT start with the track name.  These are the
# cases a "<trackName> at the top" hierarchy has to special-case.
# ---------------------------------------------------------------------------

EXCEPTIONS = [
    {"pattern": "cgs_<track>_<var>",
     "what": "chromGraph settings",
     "src": "hg/lib/chromGraph.c:chromGraphVarName",
     "why": "Prefix comes first, so a prefix scan for '<track>.' misses it."},
    {"pattern": "hgtgroup_<group>_close",
     "what": "Track group open/closed on the main page",
     "src": "hg/hgTrackUi/hgTrackUi.c:3917",
     "why": "Keyed by track group, not by track."},
    {"pattern": "hapmapSnps_monomorphic_<pop>, hapmapSnps_ortho_<species>, "
                "hapmapSnps_orthoQual_<species>",
     "what": "HapMap SNP filters",
     "src": "hg/hgTrackUi/hgTrackUi.c:2608",
     "why": "Shared across the whole hapmapSnps track set, keyed by "
            "population/species rather than by track."},
    {"pattern": "complement_<db>, hgt.baseTitle_<db>",
     "what": "Base Position ruler options",
     "src": "hg/hgTrackUi/hgTrackUi.c:2155",
     "why": "Assembly-scoped, not track-scoped, even though they are edited "
            "from the ruler's config page."},
    {"pattern": "dup_tracks",
     "what": "List of duplicated tracks",
     "src": "hg/inc/dupTrack.h:DUP_TRACKS_VAR",
     "why": "Points at a trash file that defines the dup_<n>_<track> "
            "namespaces."},
    {"pattern": "<track>.sha1, multiRegionBedPadding, virtWinFull",
     "what": "Multi-region state",
     "src": "hg/inc/hui.h:176",
     "why": "Mixed: the sha1 is track-scoped but the rest is session-scoped."},
    {"pattern": "hgt.oligoMatch, hgt.cutters, hgt.gcOnFly, hgt.motifs",
     "what": "Settings for the synthetic tracks (Short Match, Restriction "
             "Enzymes, GC on the fly, ruler motifs)",
     "src": "hg/inc/hui.h:83-127",
     "why": "These tracks store their state in the hgt.* CGI namespace "
            "rather than under their own track name."},
    {"pattern": "<varPrefix>_facet_*, <varPrefix>_order, <varPrefix>_page",
     "what": "Faceted table and sortable table state",
     "src": "hg/lib/facetedTable.c, hg/lib/tablesTables.c",
     "why": "varPrefix is usually but not always the track name."},
    {"pattern": "hgt_<track>_filterType, hgt_<track>_filterPmId",
     "what": "Database of Genomic Variants filters",
     "src": "hg/hgTrackUi/hgTrackUi.c:dgvUi",
     "why": "Track name sits in the middle of the name, so neither a "
            "'<track>.' prefix scan nor a suffix scan finds it."},
    {"pattern": "<track>.<view>.<var>",
     "what": "A composite view",
     "src": "hg/lib/cart.c:cartLookUpVariableClosestToHome",
     "why": "Not really an exception, but worth stating: the middle "
            "component is a view name, so a two-component parse of "
            "'<track>.<var>' is wrong for composites."},
]

# ---------------------------------------------------------------------------
# assemble
# ---------------------------------------------------------------------------

def build():
    return {
        "ticket": 37838,
        "what": "Track-scoped cart variables, arranged as the hierarchy a "
                "JSON cart and a cart accessor layer would use.",
        "naming": NAMING,
        "levels": {
            "1_trackName": {
                "what": "Top of the hierarchy.  Every variable below is "
                        "scoped by one track name (or by a composite/view "
                        "above that track).",
                "prefixes": NAMING["trackNamePrefixes"],
            },
            "2_common": {
                "what": "Applies to a track of any type.",
                "groups": COMMON,
            },
            "2b_container": {
                "what": "Applies to a track that contains other tracks.",
                "groups": CONTAINER,
            },
            "3_byType": {
                "what": "Keyed by eCfgType (hg/inc/trackDb.h:439) where one "
                        "exists, otherwise by the type or track-name special "
                        "case that supplies the UI.",
                "types": TYPES,
            },
            "3b_byTrackName": {
                "what": "Tracks whose vars are hard-coded literals rather "
                        "than composed from tdb->track.  Same <track>.<var> "
                        "shape, but invisible to a search for the suffix.",
                "tracks": BY_TRACK_NAME,
            },
            "4_families": {
                "what": "Leaf groups shared by several types.  A type "
                        "references these instead of repeating them; several "
                        "are themselves keyed by field or by name, which is "
                        "where the hierarchy goes one level deeper.",
                "groups": FAMILIES,
            },
        },
        "otherCgis": {
            "what": "CGIs other than hgTracks/hgTrackUi that keep per-track "
                    "or per-dataset state in the same cart.",
            "groups": OTHER_CGIS,
        },
        "notScopedByTrackName": {
            "what": "Configures one track, but the name has no track "
                    "component, so an accessor keyed on tdb->track cannot "
                    "reach it.",
            "vars": GLOBAL_BUT_TRACK_SPECIFIC,
        },
        "exceptions": EXCEPTIONS,
    }


def counts(cat):
    n_common = sum(len(g["vars"]) for g in COMMON.values())
    n_cont = sum(len(g["vars"]) for g in CONTAINER.values())
    n_fam = sum(len(g["vars"]) for g in FAMILIES.values())
    n_type = sum(len(t["vars"]) for t in TYPES.values())
    n_name = sum(len(t["vars"]) for t in BY_TRACK_NAME.values())
    n_glob = len(GLOBAL_BUT_TRACK_SPECIFIC)
    n_cgi = sum(len(g["vars"]) for g in OTHER_CGIS.values())
    return {
        "common": n_common,
        "container": n_cont,
        "families": n_fam,
        "types": n_type,
        "byTrackName": n_name,
        "notScopedByTrack": n_glob,
        "otherCgis": n_cgi,
        "typeCount": len(TYPES),
        "total": (n_common + n_cont + n_fam + n_type + n_name + n_glob
                  + n_cgi),
    }


# ---------------------------------------------------------------------------
# reconcile against the tree
# ---------------------------------------------------------------------------

def all_vars():
    """Every catalog entry, from every section."""
    out = []
    for group in (COMMON, CONTAINER, FAMILIES, TYPES, BY_TRACK_NAME,
                  OTHER_CGIS):
        for g in group.values():
            out.extend(g["vars"])
    out.extend(GLOBAL_BUT_TRACK_SPECIFIC)
    return out


ELLIPSIS = ("...", "…", "etc", "etc.")


def check_values(out=sys.stdout):
    """Sanity checks on enum value lists.  Returns the number of errors.

    The autoScale entry (fixed 2026-08-08) had three separate faults that all
    came from curating a value list by eye: it recorded the trackDb vocabulary
    instead of the cart vocabulary, it included a C enum identifier as if it
    were a value, and nothing pointed at the array the values had to match.
    A wrong value here is not a documentation slip - every wiggle
    *StringToEnum() errAborts on an unknown string, so it crashes hgTracks.
    These checks catch the mechanical part of that mistake.
    """
    bad = 0
    noSrc = []
    for e in all_vars():
        vals = e.get("values")
        name = e["name"] or "<bare track name>"
        # A placeholder in a values list becomes a real accepted value the
        # moment anything generates a validator from this file.
        for val in vals or []:
            if val.strip().lower() in ELLIPSIS:
                print("%s: %r in values is a placeholder, not a value; "
                      "enumerate the list" % (name, val), file=sys.stderr)
                bad += 1
        # An alias has to resolve to something the reader will accept.
        for alias, target in (e.get("aliases") or {}).items():
            if not vals:
                print("%s: alias %r on an entry with no values"
                      % (name, alias), file=sys.stderr)
                bad += 1
            elif target not in vals:
                print("%s: alias %r -> %r, which is not in values"
                      % (name, alias, target), file=sys.stderr)
                bad += 1
            elif alias in vals:
                print("%s: alias %r is also a value, so it cannot be "
                      "translated" % (name, alias), file=sys.stderr)
                bad += 1
        if e["type"] == "enum" and vals and not e.get("valuesSrc"):
            noSrc.append(name)
    # The filename rule in the harvester decides that a name is not a cart
    # variable, so it must never be able to say that about one the catalog
    # describes.  Nothing warns if it starts to: --reconcile checks
    # cataloged() first, so such a name would simply stop being reported and
    # keep its row.  Checked here instead, where a new extension in
    # FILE_SUFFIXES or a new catalog row shows up as an error.
    h = harvestModule()
    if h is not None:
        for e in all_vars():
            if e["name"] and h.fileNameLike(e["name"]):
                print("%s: the harvester's filename rule would read this "
                      "cataloged name as a filename; the two disagree about "
                      "what it is" % e["name"], file=sys.stderr)
                bad += 1
    print("values ok" if not bad else "values: %d problem(s)" % bad, file=out)
    # Not an error.  It is the backlog: an enum whose values nobody has
    # checked against the C array that actually gates them.
    print("enums with no valuesSrc: %d (unverified value lists)" % len(noSrc),
          file=out)
    return bad


def key(name):
    """A harvested and a cataloged name reduced to a comparable form.

    The leading separator goes because the harvester cannot always report it;
    see the module docstring.
    """
    return name.lstrip("._")


WILDCARD_RE = re.compile(r"<[a-zA-Z]+>|\*")


def as_pattern(n):
    """Compile a catalog name with <field>/<name>/* stand-ins into a regex."""
    rx = re.escape(n)
    rx = re.sub(r"\\<[a-zA-Z]+\\>", r"[A-Za-z0-9_.]+", rx)
    rx = rx.replace(r"\*", r"[A-Za-z0-9_.]*")
    return re.compile(r"\A%s\Z" % rx)


def leaf_of(n):
    """The trailing literal component of a wildcard name, or None.

    The harvester sees only the innermost suffix of a nested name: what the
    catalog calls decorator.<name>.blockMode arrives as blockMode, because the
    decorator and its name are supplied at run time.  So a wildcard name also
    registers its last component, but only where that component is a component
    -- preceded by a separator, or the whole name after a leading stand-in.
    That keeps filter.<field>Max from registering a bare "Max", which would
    match anything.
    """
    parts = WILDCARD_RE.split(n)
    tail = parts[-1]
    if not tail or WILDCARD_RE.search(tail):
        return None
    if tail.startswith(".") or tail.startswith("_"):
        return tail[1:] or None
    if len(parts) == 2 and not parts[0]:        # "<field>FilterType"
        return tail
    return None


def catalog_keys():
    """(literal names, compiled patterns) the catalog already accounts for.

    A name with <field> or <species> in it is a family stand-in rather than a
    literal, so it becomes a pattern: filter.<field> has to match the
    filter.score the tree actually reads.
    """
    literals, patterns = set(), []
    for e in all_vars():
        n = key(e["name"])
        if not n:
            continue                    # the bare track name, no suffix
        if WILDCARD_RE.search(n):
            patterns.append(as_pattern(n))
            leaf = leaf_of(n)
            if leaf:
                literals.add(leaf)
        else:
            literals.add(n)
    # The exceptions section records names built prefix-first or keyed by
    # something other than a track; they are described, just not as entries.
    for e in EXCEPTIONS:
        for pat in e["pattern"].split(","):
            n = key(pat.strip())
            if not n:
                continue
            patterns.append(as_pattern(n))
            leaf = leaf_of(n)
            if leaf:
                literals.add(leaf)
    return literals, patterns


def cataloged_test():
    """A predicate: does the catalog already account for this harvested name?

    Built once and shared by --reconcile and --update-baseline, so the two
    cannot disagree about what is already described.
    """
    literals, patterns = catalog_keys()

    def cataloged(n):
        if n in literals or any(p.match(n) for p in patterns):
            return True
        # The harvested name can itself be a wildcard, from a cartRemoveLike()
        # or a format string with the track name in the middle: hgTrackUi's
        # "*_*_sel" is the _sel the catalog already describes.
        if "*" in n:
            leaf = leaf_of(n)
            if leaf and leaf in literals:
                return True
            rx = as_pattern(n)
            return any(rx.match(c) for c in literals)
        return False

    return cataloged, literals, patterns


def read_baseline(path=BASELINE_FILE):
    """The harvested names already known not to be cart variables."""
    names = set()
    try:
        with open(path) as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if line:
                    names.add(line)
    except OSError:
        pass                    # no baseline yet: every harvested name is new
    return names


def write_baseline(names, sites=None, path=BASELINE_FILE):
    """Write the baseline, annotating each name with the file that reads it.

    The comment carries the file but not the line, so that ordinary edits above
    a call site do not rewrite hundreds of lines here and bury the one name that
    actually changed.
    """
    with open(path, "w") as f:
        f.write("""\
# cartVarsNotCataloged.txt - names that harvestCartVars.py finds at a
# cart*ClosestToHome() or safef("%s.%s") call site but that cartTrackVarCatalog.py
# does not describe as a track-scoped cart variable.  Refs #37838.
#
# Most are not cart variables at all: the scan cannot tell one from a table
# name or an SQL fragment, so _gold comes out of it too.  Some are cart
# variables that simply have not been cataloged yet.
#
# Filenames are NOT in here.  A "%s.tmp" built from a filename has the same
# shape as a "%s.heightPer" built from a track name, and 15 names of that kind
# used to sit below with a new one arriving every few weeks.  The harvester now
# reads them from the trailing extension instead: harvestCartVars.py
# --filenames lists what that rule claims and explains it.
#
# cartTrackVarCatalog.py --reconcile complains about any harvested name in
# neither the catalog nor this file, so this is what keeps a nightly run quiet
# until something actually changes.  Regenerate with --update-baseline, then read
# the diff before committing: a name appearing here is a decision that it is not
# a cart variable worth cataloging, and a name disappearing means its call site
# went away.
#
# The first version of this file was accepted wholesale, as a snapshot of the
# gap on the day reconcile learned to fail.  So a name being in here is not
# evidence that anybody has looked at it; only the ones added since, which
# arrive a few at a time in a reviewable diff, carry that weight.
#
# Names are stored with the leading separator stripped, which is how reconcile
# compares them.
""")
        sites = sites or {}
        for n in sorted(names):
            where = sites.get(n, "").rsplit(":", 1)[0]
            if where:
                f.write("%-34s # %s\n" % (n, where))
            else:
                f.write("%s\n" % n)


def harvestModule():
    """The harvester next door, or None if it cannot be imported.

    Imported by name inside a function rather than at module scope on purpose.
    registryPages loads this file by path, and in that process the sibling
    directory is not on sys.path, so a top-level import would break a consumer
    that only wants the catalog and never asks for a harvest.
    """
    try:
        import harvestCartVars as h
    except ImportError:
        print("harvestCartVars.py not importable from here", file=sys.stderr)
        return None
    return h


def harvested():
    """(name -> file:line) for every literal name the tree yields, or None.

    Keyed the same way the catalog is, so the two are directly comparable.
    """
    h = harvestModule()
    if h is None:
        return None
    out = {}
    for name, src in h.resolved(h.harvest(quiet=True)).items():
        k = key(name)
        if k:
            out.setdefault(k, src)
    return out


def reconcile(cat, out=sys.stdout, verbose=False):
    """Diff the catalog against the names the tree actually builds.

    The one thing here that needs a person is a name the tree builds that is in
    neither the catalog nor the baseline, because that is a track-scoped
    variable somebody added without saying what it is.  A catalog entry with no
    call site found is not: several are read through a helper or spelled with a
    macro the scan cannot follow, and printing them on every run is what would
    make a nightly unreadable.  So that half goes out only under --verbose.
    Silent and 0 means nothing new.
    """
    tree = harvested()
    if tree is None:
        return 1

    # A scan that finds almost nothing is a broken scan, not a clean tree, and
    # the difference matters: pointed at the wrong KENT_SRC or an empty clone,
    # everything below would come up empty and report all clear forever.
    if len(tree) < MIN_TREE_NAMES:
        print("only %d names found: expected at least %d, so the scan is "
              "broken rather\nthan the tree being clean.  Check KENT_SRC."
              % (len(tree), MIN_TREE_NAMES), file=out)
        return 1

    cataloged, literals, patterns = cataloged_test()
    baseline = read_baseline()
    h = harvestModule()          # harvested() above already proved it imports

    # A filename is not a cart variable, and the scan cannot tell the two
    # apart: "%s.tmp" built from a filename has the shape of "%s.heightPer"
    # built from a track name.  h.fileNameLike() answers that from the
    # trailing extension, so those names no longer need a baseline line each.
    # The test comes after cataloged(), so a name the catalog describes is
    # never hidden by it, and h.fileNameLike is checked against the catalog's
    # own literals by --check.
    files = sorted(n for n in tree
                   if not cataloged(n) and h.fileNameLike(n))
    new = sorted(n for n in tree if not cataloged(n) and n not in baseline
                 and not h.fileNameLike(n))
    only_cat = sorted(n for n in literals if n not in tree)

    if verbose:
        print("catalog literal names   %d" % len(literals), file=out)
        print("catalog patterns        %d" % len(patterns), file=out)
        print("harvested names         %d" % len(tree), file=out)
        print("baseline names          %d" % len(baseline), file=out)
        print("\nin the catalog, no call site found (%d)" % len(only_cat),
              file=out)
        print("    (expected for a name read through a helper or built from a "
              "macro the\n     scan cannot follow; anything else is an entry "
              "whose read has gone away)", file=out)
        for n in only_cat:
            print("    %s" % n, file=out)
        known = sorted(n for n in tree if n in baseline)
        print("\nharvested, in the baseline rather than the catalog (%d)"
              % len(known), file=out)
        for n in known:
            print("    %-30s %s" % (n, tree[n]), file=out)
        print("\nharvested, read as a filename rather than a cart variable "
              "(%d)" % len(files), file=out)
        print("    (harvestCartVars.py --filenames explains the rule)",
              file=out)
        for n in files:
            print("    %-30s %s" % (n, tree[n]), file=out)

    if new:
        print("\nbuilt by the tree, in neither the catalog nor the baseline "
              "(%d):" % len(new), file=out)
        print("    (add an entry to cartTrackVarCatalog.py if it is a "
              "track-scoped cart\n     variable, otherwise accept it with "
              "--update-baseline)", file=out)
        for n in new:
            print("    %-30s %s" % (n, tree[n]), file=out)
    return len(new)


# ---------------------------------------------------------------------------
# HTML rendering
# ---------------------------------------------------------------------------

CSS = """
body { font: 14px/1.5 -apple-system, "Segoe UI", Helvetica, Arial, sans-serif;
       margin: 0; color: #12191f; background: #fff; }
header { background: #14385c; color: #fff; padding: 18px 28px; }
header h1 { margin: 0 0 4px; font-size: 20px; font-weight: 600; }
header p { margin: 0; font-size: 13px; color: #c5d6e8; }
main { max-width: 1080px; margin: 0 auto; padding: 22px 28px 60px; }
h2 { font-size: 17px; margin: 30px 0 6px; padding-bottom: 5px;
     border-bottom: 2px solid #14385c; }
h3 { font-size: 15px; margin: 20px 0 4px; color: #14385c; }
p.what { margin: 4px 0 10px; color: #4a5764; font-size: 13px;
         max-width: 78ch; }
table { border-collapse: collapse; width: 100%; margin: 6px 0 14px;
        font-size: 13px; }
th { text-align: left; background: #eef2f6; padding: 5px 8px;
     border-bottom: 1px solid #c9d3dd; font-weight: 600; }
td { padding: 5px 8px; border-bottom: 1px solid #eceff2;
     vertical-align: top; }
tr:hover td { background: #f7fafd; }
code, .n { font-family: ui-monospace, Menlo, Consolas, monospace;
           font-size: 12.5px; }
.n { font-weight: 600; color: #0b3d62; white-space: nowrap; }
.t { color: #7a3ba8; white-space: nowrap; }
.src { color: #6b7885; font-size: 11.5px; white-space: nowrap; }
.note { color: #4a5764; font-size: 12.5px; }
.vals { color: #276749; font-size: 12px; }
.legacy { background: #fff8e1; }
.pill { display: inline-block; background: #e8eef5; color: #14385c;
        border-radius: 9px; padding: 1px 8px; font-size: 11.5px;
        margin-right: 5px; }
.tree { font-family: ui-monospace, Menlo, Consolas, monospace;
        font-size: 12.5px; background: #f7fafd; border: 1px solid #dde5ec;
        padding: 12px 14px; white-space: pre; overflow-x: auto; }
#filter { width: 320px; padding: 6px 9px; font-size: 13px;
          border: 1px solid #b9c4cf; border-radius: 4px; margin: 10px 0; }
.hidden { display: none; }
.count { color: #6b7885; font-weight: 400; font-size: 13px; }
"""

TREE = """<track>                                  visibility (bare name)
 |
 +- common to any track ................. _sel  _imgOrd  _hideKids
 |                                        priority  group  heightPer
 |                                        colorOverride[On]
 |                                        nameFilter  doWiggle  doMergeItems
 |                                        section_<section>_close
 |
 +- container layer ..................... composite: displaySubtracks,
 |                                          hideEmptySubtracks, sortOrder,
 |                                          filterComp.<groupTag>
 |                                        view:      <view>.<anyTypeVar>
 |                                        multiWig:  aggregate, viewFunc
 |
 +- type layer (eCfgType) ............... wig | bedScore | bedFilt | peak |
 |                                        genePred | psl | chain | net |
 |                                        wigMaf | bam | vcf | snake | long |
 |                                        interact | hic | barChart | lolly |
 |                                        bigDbSnp | bigRmsk  (+ non-eCfgType:
 |                                        gtex, expRatio, sample, chromGraph,
 |                                        factorSource, ld, gvf, snp125,
 |                                        blast, pubs, transMap, mrna)
 |
 +- by track name (literal vars) ........ stsMap.filter  cghNci60.color
 |                                        switchDbTss.pseudo  rosetta.et ...
 |
 +- leaf groups (shared, and nested) .... filter.<field>[Min|Max]
                                          filterBy.<field>       (list)
                                          filterText.<field>
                                          filterType.<field>
                                          highlight*.<field>     (twins)
                                          cds: baseColorDrawOpt, codonNumbering
                                          label: label, label.<kind>
                                          decorator.<name>.<var>  <- nested
                                          <species>              (wigMaf)
                                          <attribute><Value>     (snp125 color)

other writers of the same cart:
  hgc ................................... <track>.speciesOrder  <track>.vis
                                          <track>_case/_u/_b/_i/_red/_green/_blue
  hgTables .............................. hgta_fil.v.<db>.<table>.<field>.<type>
                                          hgta_fs.check.<db>.<table>.<field>
                                          (addresses data by db.table, NOT by
                                           track - the big mismatch)
"""


def esc(s):
    return html.escape(str(s), quote=False)


def var_rows(vars_, prefix=None):
    """prefix replaces the generic <track> stand-in, for the by-track-name
    section where the real track name is part of the literal var name."""
    stand_in = esc(prefix) if prefix else "&lt;track&gt;"
    out = []
    for e in vars_:
        sep = e.get("sep", ".")
        name = e["name"]
        if sep == "":
            # empty name = the bare track name (visibility); otherwise the
            # var has an absolute name with no track component at all
            full = stand_in if not name else esc(name)
        elif sep in (".", "_"):
            # legacy entries already carry their leading underscore
            joiner = "" if name.startswith(sep) else sep
            full = stand_in + esc(joiner) + esc(name)
        else:
            full = esc(sep) + esc(name)
        cls = ' class="legacy"' if sep == "_" else ""
        bits = []
        if e.get("multi"):
            bits.append('<span class="pill">multi</span>')
        if e.get("values"):
            bits.append('<span class="vals">' +
                        esc(" | ".join(e["values"])) + "</span>")
        if e.get("aliases"):
            bits.append('<span class="note">also accepts ' +
                        esc(", ".join("%s = %s" % (a, t) for a, t
                                      in sorted(e["aliases"].items()))) +
                        "</span>")
        if e.get("valuesSrc"):
            bits.append('<span class="src">values: ' +
                        esc(e["valuesSrc"]) + "</span>")
        if e.get("default") is not None:
            bits.append('<span class="note">default ' +
                        esc(e["default"]) + "</span>")
        if e.get("tdbDefault"):
            bits.append('<span class="note">trackDb: ' +
                        esc(e["tdbDefault"]) + "</span>")
        if e.get("note"):
            bits.append('<span class="note">' + esc(e["note"]) + "</span>")
        out.append(
            "<tr%s><td class='n'>%s</td><td class='t'>%s</td>"
            "<td>%s</td><td class='src'>%s</td></tr>"
            % (cls, full, esc(e["type"]), " ".join(bits), esc(e["src"])))
    return "\n".join(out)


def table(vars_, prefix=None):
    if not vars_:
        return "<p class='note'>No vars of its own; see the leaf groups it "\
               "pulls in.</p>"
    return ("<table><tr><th>variable</th><th>type</th><th>values / notes</th>"
            "<th>read at</th></tr>%s</table>" % var_rows(vars_, prefix))


def render_html(cat):
    c = counts(cat)
    p = []
    p.append("<title>Track cart variables (#37838)</title>")
    p.append("<style>%s</style>" % CSS)
    p.append("<header><h1>Track-scoped cart variables</h1>"
             "<p>Refs #37838 &mdash; arranged as the hierarchy a JSON cart "
             "and a cart accessor layer would use. %d variables across %d "
             "track types.</p></header>" % (c["total"], c["typeCount"]))
    p.append("<main>")

    p.append("<h2>The shape</h2>")
    p.append("<div class='tree'>%s</div>" % esc(TREE))

    p.append("<h2>How a name is built</h2>")
    n = cat["naming"]
    p.append("<p class='what'>Canonical form <code>%s</code>. Legacy form "
             "<code>%s</code> (highlighted in yellow below). %s</p>"
             % (esc(n["canonical"]), esc(n["legacy"]), esc(n["legacyNote"])))
    p.append("<p class='what'><b>Lookup order</b> (%s): %s. %s</p>"
             % (esc(n["lookupOrderSrc"]),
                " &rarr; ".join("<code>%s</code>" % esc(x)
                                for x in n["lookupOrder"]),
                esc(n["lookupOrderNote"])))
    p.append("<p class='what'><b>Values.</b> %s</p>"
             % esc(n["valueEncoding"]))
    p.append("<p class='what'><b>Track name prefixes.</b> %s</p>"
             % ", ".join("<code>%s</code> (%s)" % (esc(x["prefix"]),
                                                   esc(x["what"]))
                         for x in n["trackNamePrefixes"]))

    p.append("<input id='filter' placeholder='filter variables...'>")

    p.append("<h2>Level 2 &mdash; common to any track "
             "<span class='count'>(%d)</span></h2>" % c["common"])
    for k, g in COMMON.items():
        p.append("<h3>%s</h3><p class='what'>%s</p>%s"
                 % (esc(k), esc(g["what"]), table(g["vars"])))

    p.append("<h2>Level 2b &mdash; containers "
             "<span class='count'>(%d)</span></h2>" % c["container"])
    for k, g in CONTAINER.items():
        p.append("<h3>%s</h3><p class='what'>%s</p>%s"
                 % (esc(k), esc(g["what"]), table(g["vars"])))

    p.append("<h2>Level 3 &mdash; by track type "
             "<span class='count'>(%d in %d types)</span></h2>"
             % (c["types"], c["typeCount"]))
    for k, t in TYPES.items():
        p.append("<h3>%s</h3>" % esc(k))
        meta = ["<span class='pill'>%s</span>" % esc(t["cfgType"])]
        meta.append("<span class='note'>tdb type: %s</span>"
                    % esc(", ".join(t["tdbTypes"])))
        p.append("<p class='what'>%s<br><span class='src'>%s</span>%s</p>"
                 % (" ".join(meta), esc(t["cfgUi"]),
                    ("<br><span class='note'>%s</span>" % esc(t["note"]))
                    if t.get("note") else ""))
        if t.get("families"):
            p.append("<p class='what'><b>Also uses:</b> %s</p>"
                     % ", ".join("<code>%s</code>" % esc(f)
                                 for f in t["families"]))
        p.append(table(t["vars"]))
        if t.get("tdbOnly"):
            p.append("<p class='what'><b>trackDb only</b> (no cart var, but "
                     "supplies defaults): %s</p>"
                     % ", ".join("<code>%s</code>" % esc(x)
                                 for x in t["tdbOnly"]))

    p.append("<h2>Level 3b &mdash; by track name "
             "<span class='count'>(%d)</span></h2>" % c["byTrackName"])
    p.append("<p class='what'>These write their variables as literals "
             "(<code>\"stsMap.filter\"</code>) instead of composing them from "
             "<code>tdb-&gt;track</code>.  Same shape, but a suffix search "
             "will not find them, which is why they are easy to miss in a "
             "rewrite.</p>")
    for k, t in BY_TRACK_NAME.items():
        p.append("<h3>%s</h3><p class='what'><span class='src'>%s</span>%s</p>"
                 % (esc(k), esc(t["cfgUi"]),
                    ("<br><span class='note'>%s</span>" % esc(t["note"]))
                    if t.get("note") else ""))
        # use the real track name in the rendered var when the key is a
        # single track rather than a group of them
        pfx = k if re.fullmatch(r"[A-Za-z0-9_]+", k) else None
        p.append(table(t["vars"], pfx))

    p.append("<h2>Not scoped by track name "
             "<span class='count'>(%d)</span></h2>" % c["notScopedByTrack"])
    p.append("<p class='what'>Each of these configures exactly one track (or "
             "one small family of tracks) but carries no track component in "
             "its name, so an accessor keyed on <code>tdb-&gt;track</code> "
             "cannot reach it and a JSON cart cannot file it under the "
             "track.  They need either a rename or an explicit alias "
             "table.</p>")
    p.append(table(GLOBAL_BUT_TRACK_SPECIFIC))

    p.append("<h2>Level 4 &mdash; shared leaf groups "
             "<span class='count'>(%d)</span></h2>" % c["families"])
    for k, g in FAMILIES.items():
        p.append("<h3>%s</h3><p class='what'>%s</p>%s"
                 % (esc(k), esc(g["what"]), table(g["vars"])))

    p.append("<h2>Other CGIs "
             "<span class='count'>(%d)</span></h2>" % c["otherCgis"])
    p.append("<p class='what'>hgTracks and hgTrackUi are not the only writers "
             "of per-track cart state, so an accessor layer in hg/lib has to "
             "account for these as well.</p>")
    for k, g in OTHER_CGIS.items():
        p.append("<h3>%s</h3><p class='what'>%s</p>%s"
                 % (esc(k), esc(g["what"]), table(g["vars"])))

    p.append("<h2>Exceptions &mdash; track state not under the track name</h2>")
    p.append("<p class='what'>These are the cases that break a strict "
             "<code>&lt;track&gt;.&lt;var&gt;</code> hierarchy, so any "
             "accessor layer or JSON schema has to handle them "
             "deliberately.</p>")
    p.append("<table><tr><th>pattern</th><th>what</th><th>why it is "
             "awkward</th><th>src</th></tr>")
    for e in EXCEPTIONS:
        p.append("<tr><td class='n'>%s</td><td>%s</td>"
                 "<td class='note'>%s</td><td class='src'>%s</td></tr>"
                 % (esc(e["pattern"]), esc(e["what"]), esc(e["why"]),
                    esc(e["src"])))
    p.append("</table>")

    p.append("</main>")
    p.append("""<script>
document.getElementById('filter').addEventListener('input', function() {
    var q = this.value.toLowerCase();
    document.querySelectorAll('table tr').forEach(function(tr) {
        if (tr.querySelector('th')) return;
        tr.classList.toggle('hidden', q && tr.textContent.toLowerCase().indexOf(q) < 0);
    });
});
</script>""")
    return "\n".join(p)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--json")
    ap.add_argument("--html")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--reconcile", action="store_true")
    ap.add_argument("--verbose", action="store_true",
                    help="with --reconcile, also print the standing drift "
                         "that needs no action")
    ap.add_argument("--update-baseline", dest="updateBaseline",
                    action="store_true",
                    help="rewrite %s from the current tree; read the diff "
                         "before committing it"
                         % os.path.basename(BASELINE_FILE))
    args = ap.parse_args()
    cat = build()
    if args.updateBaseline:
        tree = harvested()
        if tree is None:
            return 1
        cataloged, _, _ = cataloged_test()
        h = harvestModule()      # harvested() above already proved it imports
        was = read_baseline()
        # The same two exclusions --reconcile makes, or accepting the backlog
        # would write back every filename the harvester reads as a name.
        now = set(n for n in tree
                  if not cataloged(n) and not h.fileNameLike(n))
        write_baseline(now, tree)
        print("wrote %s: %d names, %d added, %d dropped"
              % (BASELINE_FILE, len(now), len(now - was), len(was - now)))
        return 0
    if args.reconcile:
        return 1 if reconcile(cat, verbose=args.verbose) else 0
    if args.json:
        with open(args.json, "w") as f:
            json.dump(cat, f, indent=1)
        print("wrote %s" % args.json)
    if args.html:
        with open(args.html, "w") as f:
            f.write(render_html(cat))
        print("wrote %s" % args.html)
    if args.check or not (args.json or args.html):
        c = counts(cat)
        for k in sorted(c):
            print("%-12s %s" % (k, c[k]))
        # every family referenced by a type must exist
        bad = 0
        for name, t in TYPES.items():
            for fam in t.get("families", []):
                key = fam.split(" ")[0]
                if key not in FAMILIES and key not in TYPES:
                    print("unknown family %r referenced by type %r"
                          % (fam, name), file=sys.stderr)
                    bad += 1
        if bad:
            return 1
        print("families ok")
        bad += check_values()
        if bad:
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
