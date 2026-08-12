#!/usr/bin/env python3
"""
gffAttrsToIx.py - build extra ixIxx search-index lines from the attrsOut
file that gff3ToGenePred writes (-attrsOut=asmId.geneAttrs.ncbi.txt),
picking up the real NCBI gene names/descriptions that gpToIx.pl never
sees (it only looks at the genePred's own name/name2 columns).

Companion to gpToIx.pl, not a replacement:
   gpToIx.pl        -> aliases from genePred name/name2 (accession, symbol)
   gffAttrsToIx.py  -> aliases from GFF3 attributes (description, product,
                       gene_synonym, Dbxref, locus_tag, ...)

Key fact this relies on (verified against this asmHub's own
geneAttrs.ncbi.txt): when gff3ToGenePred is run with -useName, the
attribute rows for an mRNA record are written keyed by the *resolved*
genePred item name (e.g. "GCSAML"), not the raw GFF3 "rna-..." id.  That
means the first column of attrsOut already matches the genePred name
field directly -- no id-juggling needed to join transcript attributes.
The gene-level block, by contrast, stays keyed by the raw "gene-..." id,
and is reached from the transcript block via its "Parent" attribute.

usage:
   gffAttrsToIx.py asmId.geneAttrs.ncbi.txt asmId.ncbiGene.genePred.gz \
       | sort -u > extra.ix.txt
"""

import sys
import gzip
import re
from collections import defaultdict

# GFF3/attrsOut attribute names worth surfacing as search terms, and
# whether they come off the transcript (rna-) record or the gene record.
TX_ATTRS = ("product", "Note", "gene")
GENE_ATTRS = ("Name", "description", "gene", "gene_synonym", "locus_tag")

# Dbxref source prefixes worth stripping so the bare id becomes searchable
# (same list gff3ToGenePred/ncbiRefSeqOtherAttrs.pl already know about).
DBXREF_SOURCES = ("GeneID", "MIM", "HGNC", "MGI", "WormBase", "XenBase",
                   "BGD", "RGD", "SGD", "ZFIN", "FlyBase", "miRBase",
                   "NCBIOrtholog")


def openMaybeGz(path):
    return gzip.open(path, "rt") if path.endswith(".gz") else open(path)


def loadAttrs(attrsFile):
    """id -> {attr: [values]}"""
    attrs = defaultdict(lambda: defaultdict(list))
    with openMaybeGz(attrsFile) as fh:
        for line in fh:
            if line.startswith("#"):
                continue
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 3:
                continue
            itemId, attr, val = parts[0], parts[1], parts[2]
            attrs[itemId][attr].append(val)
    return attrs


def genePredNames(gpFile):
    """distinct values from column 1 (name) of the genePred"""
    names = []
    seen = set()
    with openMaybeGz(gpFile) as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            name = line.split(None, 1)[0]
            if name not in seen:
                seen.add(name)
                names.append(name)
    return names


def dbxrefTerms(values):
    terms = []
    for val in values:
        for xref in val.split(","):
            bare = xref
            for source in DBXREF_SOURCES:
                prefix = source + ":"
                if xref.lower().startswith(prefix.lower()):
                    bare = xref[len(prefix):]
                    break
            terms.append(bare)
    return terms


def noSuffix(name):
    """strip a trailing .NNN version suffix, same trick as gpToIx.pl"""
    stripped = re.sub(r"\.[0-9]+$", "", name)
    return stripped if stripped != name else None


def buildAliases(name, attrs):
    txRec = attrs.get(name, {})
    terms = []

    for attr in TX_ATTRS:
        terms.extend(txRec.get(attr, []))
    terms.extend(dbxrefTerms(txRec.get("Dbxref", [])))

    for geneId in txRec.get("Parent", []):
        geneRec = attrs.get(geneId, {})
        for attr in GENE_ATTRS:
            terms.extend(geneRec.get(attr, []))
        terms.extend(dbxrefTerms(geneRec.get("Dbxref", [])))

    suffixless = noSuffix(name)
    if suffixless:
        terms.append(suffixless)

    # searchIndex (bigBed -extraIndex=name) does an exact, case-sensitive
    # memcmp lookup (src/lib/bPlusTree.c), but searchTrix lowercases
    # everything it indexes (tolowers() in ixIxx.c) -- so a lowercase copy
    # of the item's own name lets "thoc3" find THOC3 via trix even though
    # the exact-name index only matches "THOC3".
    if name.lower() != name:
        terms.append(name.lower())

    # dedup, drop empties and anything identical to the item name itself
    seen = set()
    out = []
    for term in terms:
        term = term.strip()
        if not term or term == name or term in seen:
            continue
        seen.add(term)
        out.append(term)
    return out


def main():
    if len(sys.argv) != 3:
        sys.stderr.write(
            "usage: gffAttrsToIx.py <asmId.geneAttrs.ncbi.txt> "
            "<asmId.ncbiGene.genePred[.gz]>\n"
            "then combine with gpToIx.pl output and run ixIxx:\n"
            "  cat gpToIx.out gffAttrsToIx.out | sort -u > ix.txt\n"
            "  ixIxx ix.txt out.ix out.ixx\n")
        sys.exit(255)

    attrsFile, gpFile = sys.argv[1], sys.argv[2]
    attrs = loadAttrs(attrsFile)

    for name in genePredNames(gpFile):
        aliases = buildAliases(name, attrs)
        if aliases:
            print(name + "\t" + "\t".join(aliases))


if __name__ == "__main__":
    main()
