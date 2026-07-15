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

The composite is hidden by default. When the user enables it, rep1 +
unique-reads subtracks turn on (one signal per (tissue, age) sample,
78 tracks); rep2 and all-reads stay off and can be enabled via the
trackUi page.

Output goes to stdout. Redirect to a file.
"""

import sys

DEFAULT_TSV = '/hive/data/outside/woldlab/mouseDevTimecourse/mm10/ENCSR574CRQ_biosample.tsv'
FACETS = '/hive/data/outside/woldlab/mouseDevTimecourse/mm10/mouse_development_M21.facets'

TISSUE_SHORT = {
    'adrenal gland': 'adrenal',
    'urinary bladder': 'bladder',
    'embryonic facial prominence': 'face',
    'skeletal muscle tissue': 'muscle',
    'neural tube': 'neuraltube',
}


def tissue_short(name):
    return TISSUE_SHORT.get(name, name)


def tissue_key(name):
    return name.replace(' ', '_')


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
            colors[(tissue, timepoint)] = hex_color
    return colors


def main():
    tsv_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_TSV
    colors = load_colors()

    rows = []
    with open(tsv_path) as f:
        header = f.readline().rstrip('\n').split('\t')
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            rows.append(dict(zip(header, line.split('\t'))))

    # Replicate numbers (1 or 2) per experiment by biosample order.
    exp_biosamples = {}
    for row in rows:
        exp = row['experiment']
        if exp not in exp_biosamples:
            exp_biosamples[exp] = []
        exp_biosamples[exp].append(row['biosample'])
    for exp in exp_biosamples:
        exp_biosamples[exp].sort()

    tissues = sorted({row['biosample_term_name'] for row in rows})
    ages = sorted(
        {(row['mouse_life_stage'], row['age']) for row in rows},
        key=lambda p: (p[0], float(p[1])),
    )

    # Composite parent stanza
    print('    track developmentTimecourseSignalMm10')
    print('    parent mouseDevTimecourse')
    print('    compositeTrack on')
    print('    type bigWig')
    print('    shortLabel timecourse signal')
    print('    longLabel ENCODE mouse development time course bulk RNA-seq signal')
    print('    visibility hide')
    print('    group regulation')
    print('    subGroup1 view Views unique=Unique_reads all=All_reads')

    tissue_grp = ' '.join(tissue_key(t) + '=' + tissue_key(t) for t in tissues)
    print('    subGroup2 tissue Tissue ' + tissue_grp)

    age_grp = ' '.join(age_key(ls, age) + '=' + age_label(ls, age) for ls, age in ages)
    print('    subGroup3 age Age ' + age_grp)

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
            exp = row['experiment']
            bs = row['biosample']
            tissue = row['biosample_term_name']
            ls = row['mouse_life_stage']
            age = row['age']
            rep_num = exp_biosamples[exp].index(bs) + 1

            t_key = tissue_key(tissue)
            a_key = age_key(ls, age)
            a_lbl = age_label(ls, age)
            t_short = tissue_short(tissue)

            url = row[url_col]
            acc = accession(url)
            big_data_url = '/gbdb/mm10/mouseDevTimecourse/' + acc + '.bigWig'

            hex_color = colors.get((tissue, a_lbl), '#888888')
            rgb = hex_to_rgb(hex_color)

            track = 'developmentTimecourseSignalMm10_' + acc

            # Default on for rep1 + unique-reads (78 subtracks visible when the
            # composite is enabled). rep2 and all-reads remain off; users can
            # enable them from the trackUi page.
            parent_state = 'on' if (rep_num == 1 and view_key == 'unique') else 'off'

            short = t_short + ' ' + a_lbl + ' r' + str(rep_num) + ' ' + view_key[:3]
            short = short[:20]

            long_ = ('ENCODE mouse ' + tissue + ' ' + a_lbl
                     + ' rep' + str(rep_num) + ' ' + view_label
                     + ' (' + acc + ')')
            long_ = long_[:80]

            print('            track ' + track)
            print('            parent ' + view_container + ' ' + parent_state)
            print('            subGroups view=' + view_key + ' tissue=' + t_key
                  + ' age=' + a_key + ' rep=rep' + str(rep_num))
            print('            type bigWig')
            print('            shortLabel ' + short)
            print('            longLabel ' + long_)
            print('            bigDataUrl ' + big_data_url)
            print('            color ' + rgb)
            print()


if __name__ == '__main__':
    main()
