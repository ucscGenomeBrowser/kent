from ucscGb.encode import encodeUtils
from ucscGb.gbData import ordereddict
from ucscGb.gbData.ra.raFile import RaFile
from ucscGb.gbData.ra.raStanza import RaStanza

class MdbFile(RaFile):
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
            #print '%s mdb exps: %d' % (self.name, len(self.experiments.values()))
            for e in self.experiments.itervalues():
                if self._dataType == None and e.dataType != None:
                    #print '%s mdb set: %s' % (self.name, e.dataType)
                    self._dataType = e.dataType
                elif (self._dataType != e.dataType or e.dataType == None) and len(e.normalStanzas) > 0:
                    #print '%s mdb warning: %s != %s' % (self.name, self._dataType, e.dataType)
                    self._dataType = None
                    break
                #else:
                #    print '%s mdb same: %s' % (self.name, e.dataType)
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
        RaFile.__init__(self)
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
        
class MdbStanza(RaStanza):
    
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
                self._title = 'None'
            for expVar in expVars[1:len(expVars)]:
                if expVar in self and self[expVar] != 'None':
                    self._title += '_' + self[expVar]
            return self._title
        
    def __init__(self, parent):
        RaStanza.__init__(self)
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
            #print '%s exp normalstanzas: %d' % (self.name, len(self.normalStanzas))
            for s in self.normalStanzas:
                if 'dataType' in s:
                    if self._dataType == None:
                        #print '%s exp set: %s' % (self.name, s['dataType'])
                        self._dataType = encodeUtils.dataTypes[s['dataType']]
                    elif self._dataType.name != s['dataType']:
                        #print '%s exp warning: %s != %s' % (self.name, self._dataType.name, s['dataType'])
                        self._dataType = None
                        break
                    #else:
                    #    print '%s exp same: %s' % (self.name, s['dataType'])
                #else:
                #    print '%s exp warning: no dataType in %s' % (self.name, s.name)
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
