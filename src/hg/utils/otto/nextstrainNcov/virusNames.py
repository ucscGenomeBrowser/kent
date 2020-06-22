# Every group uses slightly different names for their sequences/trees,
# and then we went ahead and made our own gloms for Nextstrain VCF.
# But there are some common components of SARS-CoV-2 sequence names that
# can help cross-reference them:
# * EPI_ISL_\d+
# * Country/local-ID/20(19|20)

import logging
import re
from collections import defaultdict

# Regular expressions for picking out name components
# GISAID ID:
epiRe = re.compile(r'.*?(EPI_ISL_\d+)')
# Slash-separated country/localId/year name often shared by GISAID, NCBI, CNCB, COG-UK:
slashRe = re.compile(r'.*?(\w+(\/([\w.-]+)\/20\d\d?\b))')
# Slash-separated but with slashes replaced by underscores:
undRe = re.compile(r'.*?([A-Za-z]+)_([\w.-]+?)_(20\d\d?\b)')
# Slash-sep with underscores in country name
slashUndRe = re.compile(r'.*?([A-Za-z]+_[A-Za-z]+\w+)(\/[\w-]+\/20\d\d?\b)')
# AZ-TGEN-TG or just AZ-TG?
azTgenRe = re.compile(r'.*?USA\/AZ-TG\d+\/20\d\d?')

def makeIdLookup(seqNames):
    """Return a dict mapping sequence names, and components of those names like
    'EPI_ISL_402121' and 'Wuhan/IVDC-HB-05/2019' from 'EPI_ISL_402121|Wuhan/IVDC-HB-05/2019|Dec30',
    to those sequence names, so that we can attempt to map a different set of names to seqNames
    even if the other names have only a component in common."""
    idLookup = defaultdict(list)
    for seqName in seqNames:
        # Map seqName to itself in case names happen to be identical
        idLookup[seqName].append(seqName)
        # Look for EPI_ISL_ component:
        epiMatch = epiRe.match(seqName)
        if (epiMatch):
            epiId = epiMatch.groups()[0]
            idLookup[epiId].append(seqName)
        # Look for Country/localId/year component:
        slashMatch = slashRe.match(seqName)
        if (slashMatch):
            slashName, localIdYear, localId = slashMatch.groups()
            idLookup[slashName].append(seqName)
            # Sometimes a different country name is used, but the local IDs tend to be
            # pretty distinctive (except in cases where they're just a number), so
            # add just /localId/year too.
            if (not localId.isdigit()):
                idLookup[localIdYear].append(seqName)
        else:
            if ('|Wuhan-Hu-1/2019' in seqName):
                # Nextstrain uses countryless "Wuhan-Hu-1/2019", COG-UK uses "China/Wuhan-Hu-1/2019"
                idLookup['China/Wuhan-Hu-1/2019'].append(seqName)
                idLookup['Wuhan-Hu-1'].append(seqName)
            else:
                logging.debug('No slashMatch for "' + seqName + '"')
    return idLookup

def checkEpiIds(resultList, origEpiMatch, label):
    """Watch out for some instances of the same slash-separated names having
    different EPI IDs (same sample, different sequences?)"""
    if (origEpiMatch):
        okResults = []
        for result in resultList:
            resultEpiMatch = epiRe.match(result)
            if (resultEpiMatch and origEpiMatch.groups()[0] != resultEpiMatch.groups()[0]):
                logging.warn("Tree label '" + label + "' and VCF result '" + result +
                             "' were joined by a component but have different EPI IDs; "
                             "ignoring.");
            else:
                okResults.append(result)
        if (len(okResults)):
            resultList = okResults
        else:
            resultList = None
    return resultList

def lookupSeqName(seqName, idLookup):
    """If seqName, or a component of seqName, is in idLookup, then return list of
    associated name(s)."""
    resultList = idLookup[seqName]
    if (not resultList):
        # EPI_ISL_ ID is most unambiguous
        epiMatch = epiRe.match(seqName)
        if (epiMatch):
            epiId = epiMatch.groups()[0]
            resultList = idLookup[epiId]
        # Failing that, try country/localId/year
        if (not resultList):
            slashMatch = slashRe.match(seqName)
            if (not slashMatch):
                # If slashes were replaced with underscores, swap them back and try again
                if (seqName == 'Wuhan-Hu-1_2019'):
                    resultList = idLookup['Wuhan-Hu-1']
                elif (seqName == 'canine_HongKong_20-02756_2020'):
                    resultList = idLookup['HongKong/20-02756/2020']
                else:
                    undMatch = undRe.match(seqName)
                    if (undMatch):
                        country, localId, year = undMatch.groups()
                        seqNameSlash = '/'.join([country, localId, year])
                        slashMatch = slashRe.match(seqNameSlash)
            if (slashMatch):
                slashName, localIdYear, localId = slashMatch.groups()
                resultList = idLookup[slashName]
                if (not resultList):
                    resultList = idLookup[localIdYear]
                resultList = checkEpiIds(resultList, epiMatch, seqName)
        if (not resultList):
            # Try removing underscores in the country name
            slashUndMatch = slashUndRe.match(seqName)
            if (slashUndMatch):
                countryUnd, rest = slashUndMatch.groups()
                country = countryUnd.replace('_', '')
                seqNameNoUnd = country + rest
                resultList = idLookup[seqNameNoUnd]
                resultList = checkEpiIds(resultList, epiMatch, seqNameNoUnd)
        if (not resultList):
            azTgMatch = azTgenRe.match(seqName)
            if (azTgMatch):
                seqNameTgen = seqName.replace('AZ-TG', 'AZ-TGEN-TG')
                resultList = idLookup[seqNameTgen]
                resultList = checkEpiIds(resultList, epiMatch, seqNameTgen)
    return resultList

def maybeLookupSeqName(seqName, idLookup):
    """If seqName, or a component of seqName, is in idLookup, then return the first
    associated name; otherwise return the original name."""
    resultList = lookupSeqName(seqName, idLookup)
    if (len(resultList)):
        return resultList[0]
    else:
        return seqName

