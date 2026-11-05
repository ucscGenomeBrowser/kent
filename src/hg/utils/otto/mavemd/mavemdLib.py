#!/usr/bin/env python3
"""MaveMD-specific helpers: calibration choice and the colors that follow from it.

Everything to do with coordinates lives in mavedbPlace.py, under utils/otto/mavedb, and is
shared with the MaveDB track so the two place a variant identically. Importing this module
puts that directory on sys.path, so a MaveMD script can then `import mavedbPlace as mp`.

What is left here is the part MaveMD does not share: which of a score set's calibrations
drives the display, how that choice is worded for the reader, and the ACMG and functional
class palettes.
"""

import os
import sys

# The MaveDB otto job's placement code, in the sibling directory. The same relative hop
# works in the kent tree (utils/otto/mavemd -> utils/otto/mavedb) and in the installed
# copies (/hive/data/outside/otto/mavemd -> .../mavedb), so this file does not need to know
# which one it is running from.
OTTO_MAVEDB = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                            '..', 'mavedb'))
if OTTO_MAVEDB not in sys.path:
    sys.path.insert(0, OTTO_MAVEDB)

# Standard amino acids ordered by class, matching the MaveDB and popEVE heatmap tracks,
# with Ter appended as a final row (MaveMD carries ~17k nonsense measurements).
STANDARD_AAS = list('AVLIMFYWRHKDESTNQGCP')
HEATMAP_ROWS = STANDARD_AAS + ['*']

# Two palettes, because the track carries two different kinds of statement.
#
# An ACMG evidence code is a calibrated clinical claim, and it gets the 11-class RdBu
# diverging ramp: red for pathogenic evidence, blue for benign, darker with strength.
# Red against blue stays separable under all three common kinds of color blindness.
#
# The strength ladder is MaveDB's own StrengthOfEvidenceProvided enum, not an assumption:
# VERY_STRONG, STRONG, MODERATE_PLUS, MODERATE, SUPPORTING.
ACMG_COLORS = {
    'PS3_very_strong':   '#67001f',
    'PS3':               '#b2182b',
    'PS3_moderate_plus': '#d6604d',
    'PS3_moderate':      '#f4a582',
    'PS3_supporting':    '#fddbc7',
    'PS3_not_met':       '#e8e8e8',
    'BS3_not_met':       '#e8e8e8',
    'BS3_supporting':    '#d1e5f0',
    'BS3_moderate':      '#92c5de',
    'BS3_moderate_plus': '#4393c3',
    'BS3':               '#2166ac',
    'BS3_very_strong':   '#053061',
}

# A functional class with no ACMG code is a measurement, not a clinical claim, so it is drawn
# in its own palette rather than in a pale red or blue that would read as weak evidence.
# Which palette is a live design question; CLASS_PALETTES holds the candidates and
# --classPalette on the heatmap and variant builders selects one.
#
#   purple  purple/green. Unambiguous, but reads as an unrelated dataset next to a
#           calibrated map.
#   grey    colour means clinical evidence, grey means measured without it. Clean in
#           principle, but the track already spends #e8e8e8 on "evidence not met" and
#           #d9d9d9 on "no call", so the pale end of this ramp collides with both.
#   brown   the brown half of BrBG. Colourblind-safe, off the red/blue evidence ramp, and
#           does not collide with the greys already in use.
CLASS_PALETTES = {
    'purple': {'abnormal': '#762a83', 'indeterminate': '#bdbdbd', 'normal': '#7fbf7b'},
    'grey':   {'abnormal': '#252525', 'indeterminate': '#969696', 'normal': '#f7f7f7'},
    'brown':  {'abnormal': '#8c510a', 'indeterminate': '#d8b365', 'normal': '#f6e8c3'},
}
CLASS_COLORS = dict(CLASS_PALETTES['purple'])


def setClassPalette(name):
    """Choose the palette used for measurements that carry no ACMG evidence code."""
    if name not in CLASS_PALETTES:
        raise ValueError('unknown class palette %r; choose from %s'
                         % (name, ', '.join(sorted(CLASS_PALETTES))))
    CLASS_COLORS.clear()
    CLASS_COLORS.update(CLASS_PALETTES[name])


NO_CALL_COLOR = '#d9d9d9'

# Order used only when a score set has no MaveDB-designated primary calibration and we
# have to say which of several calls to show. Strongest pathogenic first, then strongest
# benign, with "not met" last because it is the absence of evidence either way.
#
# This list ranks every pathogenic call above every benign one, so if two calibrations ever
# disagree in direction the pathogenic one wins regardless of strength. No variant in the
# collection currently has calibrations that disagree in direction, so the bias is latent;
# if one appears, this ordering is the thing to revisit.
ACMG_SEVERITY = ['PS3_very_strong', 'PS3', 'PS3_moderate_plus', 'PS3_moderate',
                 'PS3_supporting',
                 'BS3_very_strong', 'BS3', 'BS3_moderate_plus', 'BS3_moderate',
                 'BS3_supporting',
                 'PS3_not_met', 'BS3_not_met']


def pickDisplayCall(calls, primaryTitle=None, primaryEmpty=False):
    """Choose which calibration's call drives the color and the filters.

    MaveDB's own variant page tiers them (Benjamin Capodanno, Oct 2026): only a calibration
    that actually classifies the variant can provide a call; prefer one that is not research
    use only, the primary first, then an investigator-provided one, then the rest; and within
    a tier take the strongest evidence. A research-use-only calibration is used only where
    nothing else classifies the variant, and the item says so.

    Tiering before strength is the whole point and it costs us evidence codes. An
    investigator-provided calibration often gives a functional class and no ACMG code while
    ExCALIBR, which is research use only, gives both, so the call shown is the weaker-looking
    one. That is MaveDB's editorial judgement and matching it keeps the track saying what
    their own variant page says.

    `calls` is a list of dicts with keys: urn, title, primary, investigator, ruo, funcClass,
    acmgOutcome, acmgCriterion, acmgStrength, odds, and holds only the calibrations that
    classify this variant. `primaryTitle` is the title of the score set's MaveDB-designated
    primary calibration, or None if it has none; `primaryEmpty` says that primary carries no
    functional classifications at all, which is a curation gap rather than a judgement about
    this one variant. Returns (chosen, source).
    """
    if not calls:
        return None, 'none'

    def rank(call):
        code = call['acmgOutcome']
        return ACMG_SEVERITY.index(code) if code in ACMG_SEVERITY else len(ACMG_SEVERITY)

    primary = [c for c in calls if c['primary'] and not c['ruo']]
    if primary:
        return min(primary, key=rank), 'MaveDB primary calibration'

    if not primaryTitle:
        context = 'no primary calibration designated'
    elif primaryEmpty:
        context = ('MaveDB\'s primary calibration (%s) classifies no variants, so it cannot '
                   'be shown' % primaryTitle)
    else:
        context = ('MaveDB\'s primary calibration (%s) does not classify this variant'
                   % primaryTitle)

    for group, what in ((
            [c for c in calls if not c['ruo'] and c['investigator']], 'investigator-provided'),
            ([c for c in calls if not c['ruo'] and not c['investigator']], 'other')):
        if group:
            if len(group) == 1:
                return group[0], '%s; showing the %s calibration' % (context, what)
            return (min(group, key=rank),
                    '%s; showing the strongest of %d %s calibrations' % (context, len(group), what))

    return (min(calls, key=rank),
            '%s; nothing outside research use only classifies this variant, showing the '
            'strongest of %d' % (context, len(calls)))


def calibrationLabel(title, ruo):
    """Calibration name for display, carrying the research-use-only marker.

    researchUseOnly is a fixed property of the calibration method (ExCALIBR and the two
    BBI-GMM calibrations are always marked, the rest never are), and no calibration MaveDB
    flags primary is ever marked, so this one string is enough to tell a reader the call
    they are looking at came from a method MaveDB does not consider clinical-grade. The
    full reasoning for which calibration won is in the calibrationSource field.
    """
    if not title:
        return 'none'
    return '%s, research use only' % title if ruo else title


def cellColor(acmgOutcome, funcClass):
    """Color for a heatmap cell or a variant item."""
    if acmgOutcome and acmgOutcome in ACMG_COLORS:
        return ACMG_COLORS[acmgOutcome]
    if funcClass and funcClass in CLASS_COLORS:
        return CLASS_COLORS[funcClass]
    return NO_CALL_COLOR


def hexToBedRgb(color):
    """'#rrggbb' -> 'r,g,b' for the BED itemRgb field."""
    color = color.lstrip('#')
    return '%d,%d,%d' % (int(color[0:2], 16), int(color[2:4], 16), int(color[4:6], 16))
