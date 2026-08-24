#!/usr/bin/env python3
"""
Read Diane's ENCSR574CRQ_biosample.tsv (#36998 attachment) and emit a
view-based trackDb composite for the ENCODE bulk RNA-seq bigWig signal
tracks on mm10. The structure mirrors the Wold Lab's own hg19 RNA-seq
track (wgEncodeCaltechRnaSeq): the composite has two view containers
("Unique reads" and "All reads") with their own display settings, and
tissue/age/rep stay as subGroups underneath.

Per-(tissue, age) colors are read from the local .facets file and
emitted as RGB on each subtrack, matching the bigBarChart gradient.

Replicate numbers come from ENCSR574CRQ_replicates.tsv, which
fetchReplicateNumbers.py builds from the ENCODE portal. The biosample TSV
has no replicate column, so that file has to be built first.

Tissue subGroup tags carry a two-digit prefix (t01_thymus ... t17_neural_tube)
to give the biological order the data authors asked for (#36998 note-79).
sortOrder compares tag strings, so without the prefix the matrix sorts
alphabetically.

The composite is hidden by default. When the user enables it, rep1 +
unique-reads subtracks turn on (one signal per (tissue, age) sample,
78 tracks); rep2 and all-reads stay off and can be enabled via the
trackUi page.

Output goes to stdout. Redirect to a file.
"""

import sys

DEFAULT_TSV = '/hive/data/outside/woldlab/mouseDevTimecourse/mm10/ENCSR574CRQ_biosample.tsv'
FACETS = '/hive/data/outside/woldlab/mouseDevTimecourse/mm10/mouse_development_M21.facets'
REPLICATES = '/hive/data/outside/woldlab/mouseDevTimecourse/mm10/ENCSR574CRQ_replicates.tsv'

# Biological order the data authors asked for, not alphabetical order.
TISSUE_ORDER = [
    'thymus',
    'spleen',
    'liver',
    'heart',
    'skeletal muscle tissue',
    'urinary bladder',
    'adrenal gland',
    'kidney',
    'lung',
    'stomach',
    'intestine',
    'limb',
    'embryonic facial prominence',
    'forebrain',
    'midbrain',
    'hindbrain',
    'neural tube',
]

# Tissue names abbreviated to fit the shortLabel budget (see MAX_SHORT_LABEL).
# The tissues sampled at embryonic ages get a 5-character age string, leaving
# only 6 characters for the name; the five P0-only tissues have room for 9.
TISSUE_SHORT = {
    'adrenal gland': 'adrenal',
    'urinary bladder': 'bladder',
    'embryonic facial prominence': 'face',
    'skeletal muscle tissue': 'muscle',
    'neural tube': 'ntube',
    'forebrain': 'fbrain',
    'midbrain': 'mbrain',
    'hindbrain': 'hbrain',
    'intestine': 'intest',
    'stomach': 'stomch',
}

# One-letter view code. A longer suffix does not fit, and truncating one gives
# sibling subtracks the same label.
VIEW_CODE = {'unique': 'U', 'all': 'A'}

# A shortLabel a few characters over the nominal 17 is fine by house convention;
# 23 is the practical ceiling. What is NOT fine is two subtracks that differ only
# past column 17: hgTracks draws the left-hand label in a 17-character area by
# default (goldenPath/help/hgTracksHelp.html), so those render identically in the
# browser image even though their trackDb labels differ. Hence two separate
# checks: a length ceiling, and uniqueness of the first LEFT_LABEL_WIDTH
# characters, which is the invariant that actually protects the display.
MAX_SHORT_LABEL = 23
LEFT_LABEL_WIDTH = 17
MAX_LONG_LABEL = 80


def tissue_short(name):
    return TISSUE_SHORT.get(name, name)


def tissue_key(name):
    return name.replace(' ', '_')


def tissue_tag(name):
    """subGroup tag carrying the biological sort order, e.g. t14_forebrain."""
    return 't%02d_%s' % (TISSUE_ORDER.index(name) + 1, tissue_key(name))


def tissue_label(name):
    """Sentence-cased subGroup label; trackDb renders the underscores as spaces."""
    key = tissue_key(name)
    return key[:1].upper() + key[1:]


def age_label(life_stage, age):
    if life_stage == 'embryonic':
        return 'e' + str(age)
    if life_stage == 'postnatal':
        if float(age) == 0:
            return 'P0'
        return 'P' + str(age)
    return life_stage + '_' + str(age)


def age_key(life_stage, age):
    return age_label(life_stage, age).replace('.', '_')


def accession(url):
    return url.rsplit('/', 1)[-1].replace('.bigWig', '')


def hex_to_rgb(h):
    h = h.lstrip('#')
    return ','.join(str(int(h[i:i + 2], 16)) for i in (0, 2, 4))


def load_colors():
    """Build a (tissue, timepoint_label) -> hex color map from the .facets file."""
    colors = {}
    with open(FACETS) as f:
        f.readline()
        for line in f:
            cols = line.rstrip('\n').split('\t')
            if len(cols) < 5:
                continue
            tissue, timepoint, hex_color = cols[2], cols[3], cols[4]
            # Keyed lower-case: the .facets tissue is sentence-cased for display
            # but the biosample TSV supplies it lower-case.
            colors[(tissue.lower(), timepoint)] = hex_color
    return colors


def load_replicates():
    """Build a file accession -> ENCODE biological replicate number map."""
    reps = {}
    with open(REPLICATES) as f:
        f.readline()
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            acc, rep = line.split('\t')
            reps[acc] = int(rep)
    return reps


def main():
    tsv_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_TSV
    colors = load_colors()
    replicates = load_replicates()
    seen_labels = {}

    rows = []
    with open(tsv_path) as f:
        header = f.readline().rstrip('\n').split('\t')
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            rows.append(dict(zip(header, line.split('\t'))))

    unknown = sorted({t for t in (r['biosample_term_name'] for r in rows)
                      if t not in TISSUE_ORDER})
    if unknown:
        sys.exit('tissue missing from TISSUE_ORDER: %s' % ', '.join(unknown))

    ages = sorted(
        {(row['mouse_life_stage'], row['age']) for row in rows},
        key=lambda p: (p[0], float(p[1])),
    )

    # Composite parent stanza
    print('    track developmentTimecourseSignalMm10')
    print('    parent mouseDevTimecourse')
    # Sorts after the four bigBarChart siblings, which take 1-4. Without an
    # explicit value this inherits the superTrack's 0.6 and floats to the top.
    print('    priority 5')
    print('    compositeTrack on')
    print('    type bigWig')
    print('    shortLabel Timecourse Signal')
    print('    longLabel ENCODE mouse development time course bulk RNA-seq signal')
    print('    visibility hide')
    print('    group regulation')
    print('    html developmentTimecourseSignalMm10')
    print('    subGroup1 view Views unique=Unique_reads all=All_reads')

    tissue_grp = ' '.join(tissue_tag(t) + '=' + tissue_label(t) for t in TISSUE_ORDER)
    print('    subGroup2 tissue Tissue ' + tissue_grp)

    age_grp = ' '.join(age_key(ls, age) + '=' + age_label(ls, age) for ls, age in ages)
    # Display label is Timepoint, matching the bigBarChart facet filter and the
    # .facets column. The group name stays 'age' because dimensions and sortOrder
    # reference it by name.
    print('    subGroup3 age Timepoint ' + age_grp)

    print('    subGroup4 rep Replicate rep1=Rep_1 rep2=Rep_2')
    print('    dimensions dimX=age dimY=tissue dimA=rep')
    print('    dimensionAchecked rep1')
    print('    sortOrder view=+ tissue=+ age=+ rep=+')
    print('    dragAndDrop subTracks')
    print('    noInherit on')
    print()

    views = (
        ('unique', 'Unique', 'Unique reads', 'signal_of_unique_reads', 'full'),
        ('all', 'All', 'All reads', 'signal_of_all_reads', 'hide'),
    )

    for view_key, view_cap, view_label, url_col, view_visibility in views:
        view_container = 'developmentTimecourseSignalMm10View' + view_cap
        print('        track ' + view_container)
        print('        view ' + view_key)
        print('        parent developmentTimecourseSignalMm10')
        print('        shortLabel ' + view_label)
        print('        type bigWig')
        print('        visibility ' + view_visibility)
        print('        autoScale on')
        print('        maxHeightPixels 100:32:8')
        print()

        for row in rows:
            tissue = row['biosample_term_name']
            ls = row['mouse_life_stage']
            age = row['age']

            t_tag = tissue_tag(tissue)
            a_key = age_key(ls, age)
            a_lbl = age_label(ls, age)
            t_short = tissue_short(tissue)

            url = row[url_col]
            acc = accession(url)
            big_data_url = '/gbdb/mm10/mouseDevTimecourse/' + acc + '.bigWig'

            if acc not in replicates:
                sys.exit('%s has no replicate number in %s; rerun '
                         'fetchReplicateNumbers.py' % (acc, REPLICATES))
            rep_num = replicates[acc]

            hex_color = colors.get((tissue.lower(), a_lbl))
            if hex_color is None:
                sys.exit('no color in %s for (%s, %s)' % (FACETS, tissue, a_lbl))
            rgb = hex_to_rgb(hex_color)

            track = 'developmentTimecourseSignalMm10_' + acc

            # Check rep1 in both views, and leave rep2 unchecked. Whether the
            # all-reads subtracks actually draw is controlled by their view's
            # visibility, which is 'hide'; so the default image is unchanged at 78
            # unique-reads rep1 tracks. Encoding the view here as well would leave
            # the All reads view with nothing checked, and switching it to full
            # would then appear to do nothing.
            parent_state = 'on' if rep_num == 1 else 'off'

            # Title Case on the tissue and the replicate marker; e14.5 / P0 stay
            # as written since they are standard developmental stage notation.
            short = (t_short.capitalize() + ' ' + a_lbl + ' R' + str(rep_num)
                     + ' ' + VIEW_CODE[view_key])
            if len(short) > MAX_SHORT_LABEL:
                sys.exit('shortLabel %d chars, limit %d: %s'
                         % (len(short), MAX_SHORT_LABEL, short))
            clipped = short[:LEFT_LABEL_WIDTH]
            if clipped in seen_labels and seen_labels[clipped] != short:
                sys.exit('shortLabels "%s" and "%s" are identical in the first %d '
                         'characters, so hgTracks draws them the same. Shorten the '
                         'tissue abbreviation in TISSUE_SHORT.'
                         % (seen_labels[clipped], short, LEFT_LABEL_WIDTH))
            seen_labels[clipped] = short

            long_ = ('ENCODE mouse ' + tissue + ' ' + a_lbl
                     + ' rep' + str(rep_num) + ' ' + view_label
                     + ' (' + acc + ')')
            if len(long_) > MAX_LONG_LABEL:
                sys.exit('longLabel %d chars, limit %d: %s'
                         % (len(long_), MAX_LONG_LABEL, long_))

            print('            track ' + track)
            print('            parent ' + view_container + ' ' + parent_state)
            print('            subGroups view=' + view_key + ' tissue=' + t_tag
                  + ' age=' + a_key + ' rep=rep' + str(rep_num))
            print('            type bigWig')
            print('            shortLabel ' + short)
            print('            longLabel ' + long_)
            print('            bigDataUrl ' + big_data_url)
            print('            color ' + rgb)
            print()


if __name__ == '__main__':
    main()
