#!/usr/bin/env python3

import json
import re
import logging, argparse, sys
from warnings import warn
from collections import defaultdict

def nameValToDDict(attributes):
    """Dunno why, but instead of a plain old JSON object, they used an array of
    { "name": ..., "value": ... } objects for attributes.
    Unpack into plain old defaultdict(str)."""
    attrs = defaultdict(str)
    for nameVal in attributes:
        attrs[nameVal['name']] = nameVal['value'].strip()
    return attrs

def isReal(val):
    """Return true if value is something real, not a placeholder"""
    if not val:
        return False
    lcVal = val.lower()
    if lcVal in ['missing', 'unknown', 'not applicable', 'not collected', 'not provided',
                 'restricted access']:
        return False
    return True

def tryAttrs(attrs, choices):
    """See if attrs has anything in choices; if so, return the first one encountered,
    otherwise return empty string."""
    for choice in choices:
        if isReal(attrs[choice]):
            return attrs[choice]
    return ""

nameFluff = ['SARS-CoV-2/', 'SARS-Cov-2/',
             'hCoV-19/', 'hCov-19/',
             'Human/', 'human/',
             'Severe acute respiratory syndrome coronavirus 2/',
             'Severe_acute_respiratory_syndrome_coronavirus2/',
             '/North America/',
             'BetaCov/'
             ]

def removeNameFluff(name):
    for fluff in nameFluff:
        name = name.replace(fluff, '');
    return name

def scoreName(name):
    """Make up some scores that will help us compare names given in different places in the record
    (because record fields and attributes are used so inconsistently by different labs).
    We're hoping for a country/isolate/year name but have to settle for the most ID-ish thing.
    removeNameFluff before calling this."""
    if not name:
        return 100
    elif name.isdigit():
        return 70
    elif '/' in name:
        return 10
    elif len(name) > 25:
        return 80
    elif name.isalpha():
        return 75
    elif name == "SARS-CoV-2":
        return 78
    else:
        return 50

def bestName(nameList):
    """Return the highest-scoring name, which hopefully will be the most familiar and useful for
    matching with GISAID"""
    defluffedList = [ removeNameFluff(name) for name in nameList ]
    defluffedList.sort(key=scoreName)
    return defluffedList[0]

def nameFromRecordAttrs(record, attrs):
    """Try various attributes and other record fields that often contain something like the
    country/isolate/year format that we want.  Strip fluff that is sometimes prepended."""
    possibleNames = []
    for attrName in ['sample name', 'Submitter Id', 'strain', 'isolate', 'title',
                     'virus identifier']:
        if isReal(attrs[attrName]):
            possibleNames.append(attrs[attrName])
    if record.get('description'):
        title = record['description'].get('title')
        if title:
            possibleNames.append(title)
    if record.get('sampleIds'):
        for obj in record['sampleIds']:
            label = obj.get('label')
            if label and label == 'Sample name':
                possibleNames.append(obj['value'])
    name = bestName(possibleNames)
    return name

def dateFromAttrs(attrs):
    """Try to extract most complete sample collection date from attrs"""
    collectionDate = tryAttrs(attrs, ['collection date', 'collection_date'])
    receiptDate = tryAttrs(attrs, ['receipt date', 'receipt_date'])
    date = ""
    if receiptDate and collectionDate:
        if len(receiptDate) > len(collectionDate):
            date = receiptDate
        else:
            date = collectionDate
    elif collectionDate:
        date = collectionDate
    else:
        date = receiptDate
    return date

def labFromRecordAttrs(record, attrs):
    lab = tryAttrs(attrs, ['collecting institution', 'collected by', 'INSDC center name',
                           'collected_by'])
    # HT Jover Lee https://github.com/nextstrain/ncov-ingest/pull/208
    if not lab and record.get('owner') and record['owner'].get('name') and \
        record['owner']['name'] == "European Bioinformatics Institute":
        if record.get('sampleIds'):
            for sidBlob in record['sampleIds']:
                if sidBlob.get('label') == "Sample name":
                    lab = sidBlob['db']
    return lab

def authorFromAttrs(attrs):
    return tryAttrs(attrs, ['collector name'])

def countryFromAttrs(attrs):
    return tryAttrs(attrs, ['geographic location', 'geo_loc_name'])

def localeFromAttrs(attrs, country):
    locale = tryAttrs(attrs, ['geographic location (region and locality)'])
    if country and not locale and ':' in country:
        country, locale = country.split(':', 2)
        locale = locale.strip()
    return country, locale

def hostIdFromAttrs(attrs):
    return tryAttrs(attrs, ['host subject id', 'host_subject_id'])

epiIslRe = re.compile('.*(EPI_ISL_[0-9]+).*')

def epiIdFromRecordAttrs(record, attrs):
    epiId = tryAttrs(attrs, ['gisaid_accession', 'gisaid_accession_id', 'gisaid', 'gisaid id',
                             'gisaid accession id', 'subgroup', 'GISAID Accession ID'])
    if not epiId and record.get('description'):
        comment =  record['description'].get('comment')
        if comment:
            m = epiIslRe.match(comment)
            if m:
                epiId = m.groups()[0]
    if epiId == "0":
        epiId = ""
    return epiId

def sraIdFromRecord(record):
    sraId = ""
    if record.get('sampleIds'):
        for dbVal in record['sampleIds']:
            db = dbVal.get('db')
            if db and db == 'SRA':
                sraId = dbVal['value']
                break
    return sraId

def main():
    parser = argparse.ArgumentParser(description="""
Read in NCBI Datasets / Virus biosample.jsonl file (each line has JSON for a BioSample record),
extract relevant attributes, and output TSV.
"""
    )
    parser.add_argument('jsonFile', help='File with one line of JSON per BioSample entry')
    args = parser.parse_args()

    with open(args.jsonFile) as f:
        for line in f:
            record = json.loads(line)
            attrs = nameValToDDict(record['attributes'])
            acc = record['accession']
            gi = ""
            name = nameFromRecordAttrs(record, attrs)
            date = dateFromAttrs(attrs)
            lab = labFromRecordAttrs(record, attrs)
            author = authorFromAttrs(attrs)
            country = countryFromAttrs(attrs)
            country, locale = localeFromAttrs(attrs, country)
            hostId = hostIdFromAttrs(attrs)
            sraId = sraIdFromRecord(record)
            epiId = epiIdFromRecordAttrs(record, attrs)
            print('\t'.join([gi, acc, name, date, lab, author, country, locale,
                             hostId, sraId, epiId]))

main()
