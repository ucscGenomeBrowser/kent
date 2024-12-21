#!/usr/bin/env python

import argparse, sys, math, re
import pandas as pd
import functools

TGU_resistance_drugs = ['AMI', 'CPR', 'EMB', 'ETH', 'FQ', 'INH', 'KAN',
                        'LZD', 'PAS', 'PTO', 'PZA', 'RIF', 'SM']
TGU_resistance_cols = [ 'TGU_' + drug for drug in TGU_resistance_drugs ]

CRyPTIC_resistance_drugs = ['AMI', 'BDQ', 'CFZ', 'DLM', 'EMB', 'ETH', 'INH',
                            'KAN', 'LEV', 'LZD', 'MXF', 'RIF', 'RFB']
CRyPTIC_resistance_cols = [ 'CRyPTIC_' + drug for drug in CRyPTIC_resistance_drugs ]
CRyPTIC_MIC_cols = [ 'CRyPTIC_' + drug + '_MIC' for drug in CRyPTIC_resistance_drugs ]
CRyPTIC_PQ_cols = [ 'CRyPTIC_' + drug + '_PHENOTYPE_QUALITY' for drug in CRyPTIC_resistance_drugs ]

Finci_resistance_drugs = ['INH', 'RMP', 'EMB', 'PZA', 'AMK', 'CAP', 'KAN', 'MFX', 'LFX']
Finci_resistance_cols = [ 'Finci_' + drug for drug in Finci_resistance_drugs ]
Finci_MIC_cols = [ 'Finci_' + drug + '_MIC' for drug in Finci_resistance_drugs ]

output_column_order = ['BioSample', 'BioProject', 'run_accession', 
                       'host_common_name', 'host_scientific_name', 'sample_source',
                       'date_isolation', 'country', 'region',
                       'literature_shorthand', 'literature_lineage', 'literature_sublineage', 
                       'tbprofiler_lineage', 'tbprofiler_resistance',
                       'TGU_lineage', 'TGU_coinfection', 'TGU_resistance',
                       'CRyPTIC_UNIQUEID', 'CRyPTIC_resistance', 'CRyPTIC_MIC',
                       'Finci_WHO_resistance', 'Finci_resistance', 'Finci_MIC']

def looksResistant(word):
    """word might be 'Resistant', 'Susceptible', 'R', 'S', 'I' or 'i'.  Return True if it starts
    with R, False if it starts with R, S, I or i, otherwise raise ValueError."""
    if isinstance(word, float) and math.isnan(word):
        return False
    elif word[0] == 'R':
        return True
    elif word[0] in 'SIi':
        return False
    else:
        raise ValueError(f"Expected 'Resistant', 'Susceptible', 'R', 'S', 'I' or 'i' but got '{word}'")


def collapse_resistance_columns(row, drugs, prefix):
    resistants = []
    for drug in drugs:
        if looksResistant(row[prefix + drug]):
            resistants.append(drug)
    if len(resistants) == 0:
        resistants.append('None')
    return ', '.join(resistants)


collapse_TGU = functools.partial(collapse_resistance_columns,
                                 drugs=TGU_resistance_drugs, prefix='TGU_')
collapse_CRyPTIC = functools.partial(collapse_resistance_columns,
                                 drugs=CRyPTIC_resistance_drugs, prefix='CRyPTIC_')
collapse_Finci = functools.partial(collapse_resistance_columns,
                                 drugs=Finci_resistance_drugs, prefix='Finci_')


def collapse_MIC_columns(row, drugs, prefix):
    values = []
    is_all_nan = True
    for drug in drugs:
        val = row[prefix + drug + '_MIC']
        if isinstance(val, float) and math.isnan(val):
            val = ""
        else:
            is_all_nan = False
        values.append(drug + ":" + str(val))
    if is_all_nan:
        return ""
    else:
        return ', '.join(values)


collapse_CRyPTIC_MIC = functools.partial(collapse_MIC_columns,
                                         drugs=CRyPTIC_resistance_drugs, prefix='CRyPTIC_')
collapse_Finci_MIC = functools.partial(collapse_MIC_columns,
                                       drugs=Finci_resistance_drugs, prefix='Finci_')


def parse_host(host):
    """Ash chose an interesting encoding for host that is hard to explain within triple-quotes:
    one " followed by scientific name (or empty string) followed by "" followed by common name
    followed by three " characters, just like this comment string will end.
    For the full encoding where scientific name is present, pd.read_csv strips the
    outer "s and then changes the ""s to "s, so in here it's like 'Homo sapiens "human"'.
    However when the scientific name is not given, the surrounding triple quotes are all removed.
    So... maybe it was not Ash's strange encoding's, but pandas's?
    Parse the encoding and return scientific name (or "") and common name (or "")."""
    # Empty string in, empty string out
    if isinstance(host, float) and math.isnan(host):
        return "", ""
    else:
        match = re.search(r'^([^"]*)"([^"]+)"$', host)
        if match:
            return match.group(1).strip(), match.group(2).strip()
        else:
            return "", host

def extract_scientific_name(host):
    sci_name, common_name = parse_host(host)
    return sci_name

def extract_common_name(host):
    sci_name, common_name = parse_host(host)
    return common_name


def main():
    parser = argparse.ArgumentParser(description="""
Read in metadata from Ash O'Farrell, collapse each source's per-drug resistance columns
into one combined list-column, output TSV on stdout.
"""
    )
    parser.add_argument('ash_metadata', help="TSV from Ash O'Farrell keyed to BioSample ID")
    args = parser.parse_args()

    # Read input
    ash_df = pd.read_csv(args.ash_metadata, sep='\t', dtype='unicode')

    # Drop unused columns
    ash_df = ash_df.drop(['other_id', 'TGU_metadata'] + CRyPTIC_PQ_cols, axis=1)

    # Collapse many drug resistance columns into one list column per source
    ash_df['TGU_resistance'] = ash_df[TGU_resistance_cols].apply(collapse_TGU, axis=1)
    ash_df['CRyPTIC_resistance'] = ash_df[CRyPTIC_resistance_cols].apply(collapse_CRyPTIC, axis=1)
    ash_df['Finci_resistance'] = ash_df[Finci_resistance_cols].apply(collapse_Finci, axis=1)
    ash_df = ash_df.drop(TGU_resistance_cols + CRyPTIC_resistance_cols + Finci_resistance_cols,
                         axis=1)

    # Collapse many drug resistance Minimum Inhibitory Concentration (MIC) score columns...
    ash_df['CRyPTIC_MIC'] = ash_df[CRyPTIC_MIC_cols].apply(collapse_CRyPTIC_MIC, axis=1)
    ash_df['Finci_MIC'] = ash_df[Finci_MIC_cols].apply(collapse_Finci_MIC, axis=1)
    ash_df = ash_df.drop(CRyPTIC_MIC_cols + Finci_MIC_cols, axis=1)

    # Split the strangely formatted host column into two new columns
    ash_df['host_scientific_name'] = ash_df['host'].apply(extract_scientific_name)
    ash_df['host_common_name'] = ash_df['host'].apply(extract_common_name)
    ash_df = ash_df.drop('host', axis=1)

    # Reorder output colums
    ash_df = ash_df.reindex(columns=output_column_order)

    # Write to stdout
    ash_df.to_csv(sys.stdout, sep='\t', index=False)

main()
