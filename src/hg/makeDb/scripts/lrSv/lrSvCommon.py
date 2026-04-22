"""Shared helpers for lrSv subtrack converters.

The lrSv supertrack aggregates SVs from many studies. To enable uniform
supertrack-level filtering and mouseovers, every subtrack's bigBed must:

  1. Store these four fields, named exactly like this:
        svType  - string, uppercase (DEL, INS, INV, CPX, DUP, CNV, CTX,
                  INSDEL, MIXED, BND, MEI, TRA, LOSS, GAIN, BOTH, ...).
        svLen   - int,  feature length on the reference (chromEnd-chromStart).
        insLen  - int,  length of inserted sequence (0 for DEL/INV/CPX/INSDEL;
                  reported absolute length for INS).
        AC      - int,  allele count. Required. Tracks that don't publish
                  AC use a documented sentinel (e.g. deCODE: 50).

  2. Use a canonical `name` column of the form:
        TYPE-LEN[:AC]
     where LEN is a short form of the feature length (svLen for DEL/INV/...,
     insLen for INS, omitted for CTX/BND):
        len <  1000 bp  -> integer bp, e.g. "200"
        len >= 1000 bp  -> "Xk" or "X.Xk" with at most one decimal,
                           trailing ".0" stripped, e.g. "1k", "5.5k", "15k"
     :AC is appended when AC is known and >= 0; otherwise omitted.

Importing:
    from lrSvCommon import svName, shortLen, normalizeSvType, insLenFor

Everything in this module has no external deps.
"""

from __future__ import annotations
from typing import Optional

# Canonical types. Anything not listed here is upper-cased and passed through.
TYPE_ALIASES = {
    "COMPLEX": "CPX",
    "CPX":     "CPX",
    "DEL":     "DEL",
    "INS":     "INS",
    "INV":     "INV",
    "DUP":     "DUP",
    "CNV":     "CNV",
    "CTX":     "CTX",
    "TRA":     "TRA",
    "BND":     "BND",
    "MEI":     "MEI",
    "INSDEL":  "INSDEL",
    "MIXED":   "MIXED",
    "LOSS":    "LOSS",
    "GAIN":    "GAIN",
    "BOTH":    "BOTH",
}

# Types for which the length segment in the name is dropped:
# CTX/BND have no meaningful single-position length.
NO_LEN_TYPES = {"CTX", "BND"}


def normalizeSvType(raw: Optional[str]) -> str:
    """Return the canonical upper-case svType string for a raw VCF/TSV value."""
    if raw is None:
        return "UNK"
    s = str(raw).strip()
    if not s or s == "." or s == "?":
        return "UNK"
    upper = s.upper()
    return TYPE_ALIASES.get(upper, upper)


def shortLen(lenBp: Optional[int]) -> str:
    """Short text form for a length in bp.

    <1000 bp  -> integer bp as string, e.g. "200"
    >=1000 bp -> "Xk" or "X.Xk" (at most one decimal, no trailing .0),
                  e.g. "1k", "5.5k", "15k", "1.5k"
    None or <=0 -> empty string.
    """
    if lenBp is None:
        return ""
    try:
        n = int(lenBp)
    except (TypeError, ValueError):
        return ""
    if n <= 0:
        return ""
    if n < 1000:
        return str(n)
    k = round(n / 1000.0, 1)
    if k == int(k):
        return f"{int(k)}k"
    return f"{k:g}k"


def svName(svType: str, featLen: Optional[int], ac: Optional[int] = None) -> str:
    """Build the canonical name column value.

    svType  - raw or canonical (will be normalized to upper-case)
    featLen - length to display; typically svLen for DEL/INV/CPX/DUP/...,
              insLen for INS. Pass None (or <=0) and the length segment
              is omitted entirely. For CTX/BND, the length is always
              dropped regardless of featLen.
    ac      - None or negative -> the ":AC" suffix is omitted.
    """
    t = normalizeSvType(svType)
    core = t
    if t not in NO_LEN_TYPES:
        lenStr = shortLen(featLen)
        if lenStr:
            core = f"{t}-{lenStr}"
    if ac is not None:
        try:
            acInt = int(ac)
            if acInt >= 0:
                return f"{core}:{acInt}"
        except (TypeError, ValueError):
            pass
    return core


def insLenFor(svType: str, refLen: int, altLen: int,
              svlenField: Optional[int] = None) -> int:
    """Compute the insLen value for a record.

    For INS / MEI: uses abs(altLen - refLen) as the inserted-sequence length,
      but if svlenField is provided and positive, prefers that (matches
      SVLEN reported by the caller).
    For everything else: 0.
    """
    t = normalizeSvType(svType)
    if t in ("INS", "MEI"):
        if svlenField is not None:
            try:
                v = int(svlenField)
                if v > 0:
                    return v
            except (TypeError, ValueError):
                pass
        return max(0, int(altLen) - int(refLen))
    return 0
