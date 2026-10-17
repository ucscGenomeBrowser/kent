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

OTTO_MAVEDB = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                            '..', '..', '..', 'utils', 'otto', 'mavedb'))
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


# Accessions reach hgsql() by string interpolation, so they are checked against this
# first. Today they can only arrive via PROTEIN_TERM in makeMaveMdVariants.py, whose
# character class already excludes quotes, but that regex is far from the query and a
# future edit to it should not be able to open this up silently.
def pickDisplayCall(calls, primaryTitle=None):
    """Choose which calibration's call drives the color and the filters.

    MaveDB curates a `primary` flag on score calibrations, with its own promote and demote API
    endpoints, so where that primary actually classifies the variant it is their editorial
    choice and we use it as-is.

    Two things complicate it. Some score sets have no primary designated at all. Others
    designate one that carries no functional classifications, so it says nothing about any
    variant and cannot be displayed even though it exists. Those are different situations and
    the item says which one happened, because "no primary designated" on a score set that has
    one is simply false.

    `calls` is a list of dicts with keys: urn, title, primary, ruo, funcClass, acmgOutcome,
    acmgCriterion, acmgStrength, odds. `primaryTitle` is the title of the score set's
    MaveDB-designated primary calibration, or None if it has none. Returns (chosen, source).
    """
    if not calls:
        return None, 'none'
    primary = [c for c in calls if c['primary']]
    if primary:
        return primary[0], 'MaveDB primary calibration'

    if primaryTitle:
        context = ('MaveDB\'s primary calibration (%s) classifies no variants, so it cannot be '
                   'shown' % primaryTitle)
    else:
        context = 'no primary calibration designated'

    def rank(call):
        code = call['acmgOutcome']
        return ACMG_SEVERITY.index(code) if code in ACMG_SEVERITY else len(ACMG_SEVERITY)

    scored = [c for c in calls if c['acmgOutcome']]
    if scored:
        chosen = min(scored, key=rank)
        if len(scored) == 1 and len(calls) == 1:
            return chosen, '%s; showing the only other calibration' % context
        return chosen, '%s; showing the strongest of %d others' % (context, len(calls))
    classed = [c for c in calls if c['funcClass']]
    if classed:
        return classed[0], '%s; showing the only other calibration with a classification' % context
    return calls[0], context


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
