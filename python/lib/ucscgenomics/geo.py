import urllib2, re, datetime

# if the molecule is RNA, we need to map our data into !Sample_molecule, which only takes certain fields
# first we check rnaExtractMapping. If its not there, we use the localization. This is because (at current)
# polyA is the most important trait, otherwise its going to be nonPolyA which GEO doesn't accept. 
rnaExtractMapping = {
    'shortPolyA': 'polyA RNA', 
    'longPolyA': 'polyA RNA', 
    'polyA': 'polyA RNA'
}

localizationMapping = {
    'cytosol': 'cytoplasmic RNA', 
    'polysome': 'cytoplasmic RNA',
    'membraneFraction': 'cytoplasmic RNA',
    'mitochondria': 'cytoplasmic RNA',
    'nucleus': 'nuclear RNA', 
    'nucleolus': 'nuclear RNA', 
    'nucleoplasm': 'nuclear RNA', 
    'nuclearMatrix': 'nuclear RNA', 
    'chromatin': 'nuclear RNA',
    'cell': 'total RNA'
}

# map our instrument names to GEO's names
instrumentModels = {
    'Illumina_GA2x': 'Illumina Genome Analyzer II',
    'Illumina_GA2': 'Illumina Genome Analyzer II',
    'Illumina_HiSeq_2000': 'Illumina HiSeq 2000',
    'Illumina_GA1': 'Illumina Genome Analyzer',
    'Illumina_GA1_or_GA2': 'Illumina Genome Analyzer, Illumina Genome Analyzer II',
    'SOLiD_Unknown': 'SOLiD',
    'Unknown': 'Illumina Genome Analyzer'
}

def getHtml(geoId):
    try:
        response = urllib2.urlopen('http://www.ncbi.nlm.nih.gov/geo/query/acc.cgi?acc=%s' % geoId)
    except:
        return None
    return response.read()
    
def getGeo(geoId):
    return re.findall('(GSM[0-9]+)</a></td>\n<td valign="top">([^<]+)</td>', getHtml(geoId))
    
def getDateSubmitted(geoId):
    datestr = re.search('<td>Submission date</td>\n<td>([^<]+)</td>', getHtml(geoId))
    if datestr == None:
        return None
    return datetime.datetime.strptime(datestr.group(1), '%b %d, %Y')
    
def getDateUpdated(geoId):
    datestr = re.search('<td>Last update date</td>\n<td>([^<]+)</td>', getHtml(geoId))
    if datestr == None:
        return None
    return datetime.datetime.strptime(datestr.group(1), '%b %d, %Y')
    
