#!/usr/bin/env python

import csv
import re
import logging, argparse, sys, math
from warnings import warn
from collections import defaultdict
import pandas as pd
import dateutil.parser


def cleanDate(messyDate):
    if isinstance(messyDate, float):
        if math.isnan(messyDate):
            return ''
        else:
            return int(messyDate)
    elif messyDate.endswith('.0'):
        return messyDate.removesuffix('.0')
    elif messyDate.startswith('between '):
        return messyDate.removeprefix('between ')
    else:
        return dateutil.parser.parse(messyDate).strftime('%Y-%m-%d')


def checkEqualOrEmpty(valA, valB):
    if valA == 'nan' or (isinstance(valA, float) and math.isnan(valA)):
        valA = ''
    if valB == 'nan' or (isinstance(valB, float) and math.isnan(valB)):
        valB = ''
    if valA == '':
        return valB
    elif valB == '':
        return valA
    else:
        if valA != valB:
            warn(f"Mismatching non-empty values '{valA}' != '{valB}'")
        return valA

def useLonger(valA, valB):
    if valA == 'nan' or (isinstance(valA, float) and math.isnan(valA)):
        valA = ''
    if valB == 'nan' or (isinstance(valB, float) and math.isnan(valB)):
        valB = ''
    if valA == '':
        return valB
    elif valB == '':
        return valA
    elif valA.startswith(valB):
        return valA
    elif valB.startswith(valA):
        return valB
    else:
        if valA != valB:
            warn(f"Mismatching non-empty values '{valA}' != '{valB}'")
        return valA


def useAltIfEmpty(val, alt):
    if val == 'nan' or (isinstance(val, float) and math.isnan(val)):
        val = ''
    if alt == 'nan' or (isinstance(alt, float) and math.isnan(alt)):
        alt = ''
    if val == '':
        return alt
    else:
        return val


def main():
    parser = argparse.ArgumentParser(description="""
Read in INSDC metadata and metadata from Ash O'Farrell, join into one cohesive
output TSV on stdout.
"""
    )
    parser.add_argument('insdc_metadata', help='TSV from INSDC samples with user-friendly tree name, may include BioSample ID')
    parser.add_argument('ash_metadata', help="TSV from Ash O'Farrell keyed to BioSample ID")
    args = parser.parse_args()

    # Read inputs
    insdc_df = pd.read_csv(args.insdc_metadata, sep='\t', dtype='unicode')
    ash_df = pd.read_csv(args.ash_metadata, sep='\t', dtype='unicode')

    # Convert arbitrary date strings to ISO dates
    ash_df['date_isolation'] = ash_df['date_isolation'].apply(cleanDate)

    both_df = (insdc_df.rename(columns = {'biosample_accession': 'BioSample'})
               .merge(ash_df, on = ['BioSample'], how = 'outer')
              )

    # Make sure that redundant columns agree (if not empty)
    both_df['country_x'] = both_df['country_x'].combine(both_df['country_y'], checkEqualOrEmpty)
    both_df['date'] = both_df['date'].combine(both_df['date_isolation'], useLonger)
    both_df['location'] = both_df['location'].combine(both_df['region'], checkEqualOrEmpty)
    both_df['bioproject_accession'] = both_df['bioproject_accession'].combine(both_df['BioProject'],
                                                          checkEqualOrEmpty)
    both_df['sra_sample_accession'] = both_df['sra_sample_accession'].combine(both_df['ERS'],
                                                                              checkEqualOrEmpty)

    # If 'strain' column is empty (i.e. BioSample is only in Ash's metadata), set it to BioSample
    both_df['strain'] = both_df['strain'].combine(both_df['BioSample'], useAltIfEmpty)

    # Drop unwanted columns and tweak some column names
    both_df = (both_df.drop(['year_isolation', 'tba3_no_lineage', 'on_20240221_final',
                             'date_isolation', 'country_y', 'region', 'BioProject', 'ERS'], axis=1)
               .rename(columns = {'country_x': 'country',
                                  'location': 'region',
                                  'bioproject_accession': 'BioProject',
                                  'sra_sample_accession': 'SraSample'}))

    # Write to stdout
    both_df.to_csv(sys.stdout, sep='\t', index=False)

main()
