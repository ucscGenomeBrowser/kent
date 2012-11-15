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
    'Illumina_GA2x': 'Illumina Genome Analyzer IIx',
    'Illumina_GA2': 'Illumina Genome Analyzer II',
    'Illumina_GA2e': 'Illumina Genome Analyzer II',
    'Illumina_HiSeq_2000': 'Illumina HiSeq 2000',
    'Illumina_GA1': 'Illumina Genome Analyzer',
    'Illumina_GA1_or_GA2': 'Illumina Genome Analyzer, Illumina Genome Analyzer II',
    'SOLiD_Unknown': 'SOLiD',
    'AB_SOLiD_3.5': 'AB SOLiD 3.0 Plus',
    'Unknown': 'Illumina Genome Analyzer'
}

class Submission(object):
    
    @property
    def accessions(self):
        return self._accessions
        
    @property
    def dateSubmitted(self):
        return self._submitted
    
    @property
    def dateUpdated(self):
        return self._updated
    
    def __init__(self, geoId):
        html = getHtml(geoId)
        self._accessions = getGSE(html)
        self._submitted = getDateSubmitted(html)
        self._updated = getDateUpdated(html)
        
    def getSample(self, geoId):
        html = getHtml(geoId)
        return getGSM(html)

def getHtml(geoId):
    try:
        response = urllib2.urlopen('http://www.ncbi.nlm.nih.gov/geo/query/acc.cgi?acc=%s' % geoId)
    except:
        return None
    return response.read()
    
def getGSE(html):
    gsms = re.findall('(GSM[0-9]+)</a></td>\n<td valign="top">([^<]+)</td>', html)
    d = dict()
    for gsm in gsms:
        d[gsm[1]] = gsm[0]
    return d
    
def getGSM(html):
    suppfiles = re.findall('<tr valign="top"><td bgcolor="#[0-9A-F]+">([^<]+)</td>', html)
    d = dict()
    for f in suppfiles:
        if f.startswith('SRX'):
            continue
        fname = f.rsplit('_', 1)[1]
        d[fname] = fname
    return d
        
def getDateSubmitted(html):
    datestr = re.search('<td>Submission date</td>\n<td>([^<]+)</td>', html)
    if datestr == None:
        return None
    return datetime.datetime.strptime(datestr.group(1), '%b %d, %Y')
    
def getDateUpdated(html):
    datestr = re.search('<td>Last update date</td>\n<td>([^<]+)</td>', html)
    if datestr == None:
        return None
    return datetime.datetime.strptime(datestr.group(1), '%b %d, %Y')
    
