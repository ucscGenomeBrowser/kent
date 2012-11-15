import os, hashlib, subprocess

class DataType(object):

    def __init__(self, name, molecule, strategy, source, selection, type):
        self.name = name
        self.molecule = molecule
        self.strategy = strategy
        self.source = source
        self.selection = selection
        self.type = type
        
    @property    
    def valid(self):
        return self.molecule != 'REPLACE' and self.strategy != 'REPLACE' and self.source != 'REPLACE' and self.selection != 'REPLACE' and self.type != None
    
    @property
    def shouldSubmit(self):
        return self.type != 'NotGeo'
       
    def __str__(self):
        return self.name

dataTypes = {
    'Cage': DataType(           'Cage',             'RNA',          'OTHER',                                            'transcriptomic',   'CAGE',                                         'HighThroughput'),
    'ChipSeq': DataType(        'ChipSeq',          'genomic DNA',  'ChIP-Seq',                                         'genomic',          'ChIP',                                         'HighThroughput'),
    'Combined': DataType(       'Combined',         'genomic DNA',  'ChIP-Seq',                                         'genomic',          'ChIP',                                         'HighThroughput'), #copy of chipseq
    'DnaPet': DataType(         'DnaPet',           'genomic DNA',  'OTHER',                                            'genomic',          'size fractionation',                           'HighThroughput'),
    'DnaseDgf': DataType(       'DnaseDgf',         'genomic DNA',  'DNase-Hypersensitivity',                           'genomic',          'DNase',                                        'HighThroughput'),
    'DnaseSeq': DataType(       'DnaseSeq',         'genomic DNA',  'DNase-Hypersensitivity',                           'genomic',          'DNase',                                        'HighThroughput'),
    'FaireSeq': DataType(       'FaireSeq',         'genomic DNA',  'OTHER',                                            'genomic',          'other',                                        'HighThroughput'),
    'MethylSeq': DataType(      'MethylSeq',        'genomic DNA',  'MRE-Seq',                                          'genomic',          'Restriction Digest',                           'HighThroughput'),
    'MethylRrbs': DataType(     'MethylRrbs',       'genomic DNA',  'Bisulfite-Seq',                                    'genomic',          'Reduced Representation',                       'HighThroughput'),
    'RnaPet': DataType(         'RnaPet',           'RNA',          'OTHER',                                            'transcriptomic',   'other',                                        'HighThroughput'),
    'RnaSeq': DataType(         'RnaSeq',           'RNA',          'RNA-Seq',                                          'transcriptomic',   'cDNA',                                         'HighThroughput'),
    '5C': DataType(             '5C',               'genomic DNA',  'OTHER',                                            'genomic',          'other',                                        'HighThroughput'),
    #need this
    'RepliSeq': DataType(       'RepliSeq', 'genomic DNA', 'OTHER', 'genomic', 'ChIP', 'HighThroughput'),
    'RepliChip': DataType(      'RepliChip', 'genomic DNA', 'ChIP-Seq', 'genomic', 'ChIP', 'HighThroughput'),
    
    #doublecheck
    'ChiaPet': DataType(        'ChiaPet',          'genomic DNA',  'ChIP-Seq followed by ligation',                    'genomic',          'other',                                      'HighThroughput'),
    'Nucleosome': DataType(     'Nucleosome',       'genomic DNA',  'MNase-Seq',                                         'genomic',          'MNase',                                         'HighThroughput'),
    'RipSeq': DataType(         'RipSeq',           'RNA',          'OTHER',                                          'transcriptomic',   'RNA binding protein antibody',                 'HighThroughput'),
    #for ripseq, ask geo about new 'ripseq'
    
    'MethylWgbs': DataType(       'MethylWgbs', 'genomic DNA', 'Bisulfite-Seq', 'genomic', 'other', 'HighThroughput'),
    
    #not geo stuff
    'Bip': DataType('Bip', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Gencode': DataType('Gencode', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Mapability': DataType('Mapability', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'NRE': DataType('NRE', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Switchgear': DataType('Switchgear', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'TfbsValid': DataType('TfbsValid', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Cluster': DataType('Cluster', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Orchid': DataType('Orchid', 'REPLACE',  'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Proteogenomics': DataType( 'Proteogenomics',   'REPLACE',      'REPLACE',    'REPLACE',          'REPLACE',    'NotGeo'),
    
    #array
    'AffyExonArray': DataType(  'AffyExonArray',    'mRNA',         'RNA-Microarray',                                   'transcriptomic',   'polyA',                                        'MicroArray'),
    'MethylArray': DataType(    'MethylArray',      'genomic DNA',  'REPLACE',                                          'genomic',          'REPLACE',                                      'MicroArray'),
    'RipGeneSt': DataType(      'RipGeneSt',        'RNA',          'REPLACE',                                          'transcriptomic',   'RNA binding protein antibody',                 'MicroArray'), #this isn't correct
    'RipTiling': DataType(      'RipTiling',        'RNA',          'REPLACE',                                          'transcriptomic',   'RNA binding protein antibody',                 'MicroArray'),
    'Genotype': DataType(       'Genotype',         'genomic DNA',  'REPLACE',                                          'genomic',          'REPLACE',                                      'MicroArray'),
    
    #these need to be curated
    'Cnv': DataType(            'Cnv',              'REPLACE',      'REPLACE',                                          'REPLACE',          'REPLACE',                                      None),
    'RnaChip': DataType(        'RnaChip',          'RNA',          'REPLACE',                                          'transcriptomic',   'RNA binding protein antibody',                 None),
    'RipChip': DataType(        'RipChip',          'RNA',          'REPLACE',                                          'transcriptomic',   'RNA binding protein antibody',                 None)
}

#compare this to the source in datatype, give GP ids depending on the type
gpIds = {
    'human genomic': '63443',
    'human transcriptomic': '30709',
    'human protein': '63447',
    
    'mouse genomic': '63471',
    'mouse transcriptomic': '66167',
    'mouse protein': '63475'
}

organisms = {
    'hg19': 'human',
    'hg18': 'human',
    'mm9': 'mouse',
    'encodeTest': 'human'
}

def defaultTrackPath():
    return os.path.expanduser('~/kent/src/hg/makeDb/trackDb/')

def defaultCvPath():
    return defaultTrackPath() + 'cv/alpha/cv.ra'
    
def downloadsPath(database, composite):
    return '/hive/groups/encode/dcc/analysis/ftp/pipeline/' + database + '/' + composite + '/'

def readFilesList(filename):
    if os.path.isfile(filename):
        files = dict()
        f = open(filename, 'r')
        for line in f:
            key, val = map(str.strip, line.split('\t', 1))
            files[key] = val.split('; ')
        return files
    else:
        return None
        
def writeFilesList(filename, filesList):
    f = open(filename, 'w')
    for k in filesList: 
        f.write(k + '\t' + '; '.join(filesList[k]) + '\n')
    f.close()
    
def readMd5sums(filename):
    '''Reads an md5sum.txt file and returns a dictionary of filename: md5'''
    if os.path.isfile(filename):
        md5sums = dict()
        md5file = open(filename, 'r')
        for line in md5file:
            key, val = map(str.strip, line.split(' ', 1))
            md5sums[key] = val
        return md5sums
    else:
        return None
        
def writeMd5sums(filename, file, md5sum):
    md5file = readMd5sums(filename)
    if md5file != None and file in md5file:
        md5file[file] = md5sum
        f = open(filename, 'w')
        for key in md5file.iterkeys():
            f.write(key + ' ' + md5file[key] + '\n')
        f.close()
    else:
        f = open(filename, 'a')
        f.write(file + ' ' + md5sum + '\n')
        f.close()

def hashFile(filename, hasher=hashlib.md5(), blocksize=65536):
    '''MD5's the file, and returns the number'''
    out = subprocess.check_output(['md5sum', filename])
    md5 = out.split('  ')[0]
    return md5
    # afile = open(filename, 'rb')
    # buf = afile.read(blocksize)
    # while len(buf) > 0:
        # hasher.update(buf)
        # buf = afile.read(blocksize)
    # return hasher.hexdigest()
