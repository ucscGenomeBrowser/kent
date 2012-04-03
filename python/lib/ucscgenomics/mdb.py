from ucscgenomics import ra, ordereddict

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
    
dataTypes = {
    'Cage': DataType(           'Cage',             'RNA',          'OTHER',                                            'transcriptomic',   'CAGE',                                         'HighThroughput'),
    'ChipSeq': DataType(        'ChipSeq',          'genomic DNA',  'ChIP-Seq',                                         'genomic',          'ChIP',                                         'HighThroughput'),
    'DnaPet': DataType(         'DnaPet',           'genomic DNA',  'OTHER',                                            'genomic',          'size fractionation',                           'HighThroughput'),
    'DnaseDgf': DataType(       'DnaseDgf',         'genomic DNA',  'DNase-Hypersensitivity',                           'genomic',          'DNase',                                        'HighThroughput'),
    'DnaseSeq': DataType(       'DnaseSeq',         'genomic DNA',  'DNase-Hypersensitivity',                           'genomic',          'DNase',                                        'HighThroughput'),
    'FaireSeq': DataType(       'FaireSeq',         'genomic DNA',  'OTHER',                                            'genomic',          'other',                                        'HighThroughput'),
    'MethylSeq': DataType(      'MethylSeq',        'genomic DNA',  'MRE-Seq',                                          'genomic',          'Restriction Digest',                           'HighThroughput'),
    'MethylRrbs': DataType(     'MethylRrbs',       'genomic DNA',  'Bisulfite-Seq',                                    'genomic',          'Reduced Representation',                       'HighThroughput'),
    'Orchid': DataType(         'Orchid',           'genomic DNA',  'OTHER',                                            'genomic',          'other',                                        'HighThroughput'),
    'Proteogenomics': DataType( 'Proteogenomics',   'protein',      'mass spectrometry-based proteogenomic mapping',    'protein',          'chromatographically fractionated peptides',    'HighThroughput'),
    'RnaPet': DataType(         'RnaPet',           'RNA',          'OTHER',                                            'transcriptomic',   'other',                                        'HighThroughput'),
    'RnaSeq': DataType(         'RnaSeq',           'RNA',          'RNA-Seq',                                          'transcriptomic',   'cDNA',                                         'HighThroughput'),
    
    #doublecheck
    'ChiaPet': DataType(        'ChiaPet',          'genomic DNA',  'ChIP-Seq followed by ligation',                    'genomic',          'other',                                      'HighThroughput'),
    'Nucleosome': DataType(     'Nucleosome',       'genomic DNA',  'ChIP-Seq',                                         'genomic',          'ChIP',                                         'HighThroughput'),
    'RipSeq': DataType(         'RipSeq',           'RNA',          'OTHER',                                          'transcriptomic',   'RNA binding protein antibody',                 'HighThroughput'),
    #for ripseq, ask geo about new 'ripseq'
    
    #not geo stuff
    '5C': DataType('5C', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Bip': DataType('Bip', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Gencode': DataType('Gencode', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Mapability': DataType('Mapability', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'NRE': DataType('NRE', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Switchgear': DataType('Switchgear', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'TfbsValid': DataType('TfbsValid', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    'Cluster': DataType('Cluster', 'REPLACE', 'REPLACE', 'REPLACE', 'REPLACE', 'NotGeo'),
    
    #array
    'AffyExonArray': DataType(  'AffyExonArray',    'mRNA',         'RNA-Microarray',                                   'transcriptomic',   'polyA',                                        'MicroArray'),
    'MethylArray': DataType(    'MethylArray',      'genomic DNA',  'REPLACE',                                          'genomic',          'REPLACE',                                      'MicroArray'),
    'RipGeneSt': DataType(      'RipGeneSt',        'RNA',          'REPLACE',                                          'transcriptomic',   'RNA binding protein antibody',                 'MicroArray'), #this isn't correct
    'RipTiling': DataType(      'RipTiling',        'RNA',          'REPLACE',                                          'transcriptomic',   'RNA binding protein antibody',                 'MicroArray'),
    
    #these need to be curated
    'Cnv': DataType(            'Cnv',              'REPLACE',      'REPLACE',                                          'REPLACE',          'REPLACE',                                      None),
    'Combined': DataType(       'Combined',         'REPLACE',      'REPLACE',                                          'REPLACE',          'REPLACE',                                      None),
    'Genotype': DataType(       'Genotype',         'genomic DNA',  'REPLACE',                                          'genomic',          'REPLACE',                                      None),
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

class MdbFile(ra.RaFile):
    '''
    This should be used for all files in the metaDb, since they extend RaFile
    with useful functionality specific to metaDb ra files.
    '''
    
    @property
    def name(self):
        return self.compositeStanza['metaObject']
    
    @property
    def expVars(self):
        '''the experimental variables used in this track'''
        try:
            return self._expVars
        except AttributeError:
            self._expVars = self.compositeStanza['expVars'].split(',')
            return self._expVars
    
    @property
    def dataType(self):
        '''The data type of the experiment. 'None' if inconsistent.'''
        try:
            return self._dataType
        except AttributeError:
            self._dataType = None
            for e in self.experiments.itervalues():
                if self._dataType == None and e.dataType != None:
                    self._dataType = e.dataType
                elif self._dataType != e.dataType or e.dataType == None:
                    self._dataType = None
                    break
            return self._dataType
    
    @property
    def compositeStanza(self):
        '''the stanza (typically first in file) describing the composite'''
        try:
            return self._compositeStanza
        except AttributeError:
            self._compositeStanza = self.filter(lambda s: s['objType'] == 'composite', lambda s: s)
            if len(self._compositeStanza) != 1:
                raise KeyError(self.filename)
            else:
                self._compositeStanza = self._compositeStanza[0]
            return self._compositeStanza
            
    @property    
    def experiments(self):
        '''dictionary of MdbExp objects indexed by the expId'''
        try:
            return self._experiments
        except AttributeError:
            self._experiments = dict()
            exps = self.filter(lambda s: s['objType'] != 'composite', lambda s: (s['expId'], s))
            stanzas = dict()
            for k, v in exps:
                if k not in stanzas:
                    stanzas[k] = list()
                stanzas[k].append(v)
            for id in stanzas.iterkeys():
                self._experiments[id] = MdbExp(id, self, stanzas[id])
            return self._experiments
    
    def __init__(self, filepath):
        ra.RaFile.__init__(self)
        self.read(filepath)
        
    def readStanza(self, stanza, key=None):
        entry = MdbStanza(self)
        if entry.readStanza(stanza, key) == None:
            return None, None, None
        val1, val2 = entry.readStanza(stanza, key)
        return val1, val2, entry

    def expIdRange(self, expIds):
        '''
        Takes in a list of strings, and returns a list containing the 
        valid range of expId keys that the inputted range encompasses.
        '''
        ids = dict()
        tempids = list()
        for id in expIds:
            if '-' in id:
                start, end = id.split('-', 1)
                tempids.extend(range(int(start), int(end) + 1))
            else:
                tempids.append(int(id))
        for id in tempids:
            if str(id) in self.experiments.keys():
                ids.append(str(id))
        return ids
        
class MdbStanza(ra.RaStanza):
    
    @property
    def title(self):
        '''The expVars catted together, making the title used for GEO'''
        try:
            return self._title
        except AttributeError:
            expVars = self._parent.expVars
            if expVars[0] in self:
                self._title = self[expVars[0]].replace('-m', '')
            else:
                self._title = None
            for expVar in expVars[1:len(expVars)]:
                if expVar in self and self[expVar] != 'None':
                    self._title += '_' + self[expVar]
            return self._title
        
    def __init__(self, parent):
        ra.RaStanza.__init__(self)
        self._parent = parent
        
    def __setitem__(self, key, value):
        ordereddict.OrderedDict.__setitem__(self, key, value)
        ordereddict.OrderedDict.sort(self)
        ordereddict.OrderedDict.reorder(self, 0, 'metaObject')
        ordereddict.OrderedDict.reorder(self, 1, 'objType')
        
class MdbExp(list):
    '''
    Describes a single experiment ID, which has a collection of its stanzas as
    well as some additional data that should typically be consistent across all
    the stanzas, as well as verifying that the data is in fact consistent.
    '''
    
    @property
    def name(self):
        return self._id
        
    @property
    def title(self):
        try:
            return self._title
        except AttributeError:
            self._title = None
            for s in self.normalStanzas:
                if self._title == None:
                    self._title = s.title
                elif self._title != s.title:
                    self._title = None
                    break
            return self._title
        
    @property
    def dataType(self):
        '''The data type of the experiment. 'None' if inconsistent.'''
        try:
            return self._dataType
        except AttributeError:
            self._dataType = None
            for s in self.normalStanzas:
                if 'dataType' in s:
                    if self._dataType == None:
                        self._dataType = dataTypes[s['dataType']]
                    elif self._dataType.name != s['dataType']:
                        self._dataType = None
                        break
            return self._dataType
    
    @property
    def normalStanzas(self):
        '''Returns the list of stanzas without revoked items'''
        try:
            return self._normal
        except AttributeError:
            self._normal = list()
            for s in self:
                if 'objStatus' not in s:
                    self._normal.append(s)
            return self._normal
                
    
    def __init__(self, id, parent, stanzas):
        list.__init__(self)
        self.extend(stanzas)
        self._id = id
        self._parent = parent
