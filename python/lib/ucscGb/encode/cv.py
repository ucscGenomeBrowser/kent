import re
import os
from ucscGb.gbData import ordereddict
from ucscGb.encode import encodeUtils
from ucscGb.gbData.ra.raFile import RaFile
from ucscGb.gbData.ra.raStanza import RaStanza

def extractValue(val, prefix='', removeComments=1):
    val2 = val.replace(prefix, '')
    if removeComments and '#' in val2:
        val2 = val2.split('#', 1)[0]
    return val2.strip()

def extractList(val, prefix='', removeComments=1):
    val2 = val.replace(prefix, '')
    if removeComments and '#' in val2:
        val2 = val2.split('#', 1)[0]
    return map(str.strip, val2.split(','))

class CvFile(RaFile):
    '''
    cv.ra representation. Mainly adds CV-specific validation to the RaFile
    
    To create a CvFile, the simplest way is just to call it with no params,
    but you can specify a file path if you want to open up something other
    than the alpha cv in your tree, specify this. The handler can almost
    always be left blank, since that simply provides a function to handle
    validation errors that would otherwise throw an exception. You also should
    specify a protocolPath if you want to validate, since it will check the
    protocol documents when you validate, to ensure that the cv matches them.
    
    Validation recurses over all stanzas, calling the overridden validation
    function for the more developed stanzas. To start validation, you can 
    simply call validate() on the cv object.
    
    For more information about other things not specific to the cv, but for
    all ra files, look at the RaFile documentation.
    '''

    def __init__(self, filePath=None, handler=None, protocolPath=None):
        '''sets up exception handling method, and optionally reads from a file'''
        RaFile.__init__(self)
        
        self.handler = handler
        if handler == None:
            self.handler = self.raiseException
            
        if filePath == None:
            filePath = encodeUtils.defaultCvPath()
        
        self.protocolPath = protocolPath
        if protocolPath == None:
            self.protocolPath == os.path.expanduser('~/htdocsExtras/ENCODE/')
        
        self.missingTypes = set()
        
        self.read(filePath)

    def raiseException(self, exception):
        '''wrapper function for raising exception'''
        raise exception

    def readStanza(self, stanza, key=None):
        '''overriden method from RaFile which makes specialized stanzas based on type'''
        entry = CvStanza()

        key, val = entry.readStanza(stanza)
        return key, val, entry


    def validate(self):
        '''base validation method which calls all stanzas' validate'''
        for stanza in self.itervalues():
            stanza.validate(self)

    def getTypeOfTermStanza(self, type):
    
        if type == 'mouseCellType':
            mousestanza = CvStanza()
            mousestanza['term'] = 'mouseCellType'
            mousestanza['tag'] = 'MOUSECELLTYPE'
            mousestanza['type'] = 'typeOfTerm'
            mousestanza['label'] = 'Cell, tissue or DNA sample specific to mouse'
            mousestanza['description'] = 'NOT FOR USE! ONLY FOR VALIDATION. Cell line or tissue used as the source of experimental material specific to mouse.'
            mousestanza['searchable'] = 'multiSelect'
            mousestanza['cvDefined'] = 'yes'
            mousestanza['validate'] = 'cv or None'
            mousestanza['requiredVars'] = 'term,tag,type,description,organism,vendorName,orderUrl,age,strain,sex #Provisional'
            mousestanza['optionalVars'] = 'label,tissue,termId,termUrl,color,protocol,category,vendorId,lots,deprecated #Provisional'
            return mousestanza
    
        types = self.filter(lambda s: s['term'] == type and s['type'] == 'typeOfTerm', lambda s: s)
        if len(types) != 1:
            return None
        return types[0]
                
class CvStanza(RaStanza):
    '''base class for a single stanza in the cv, which adds validation'''
    
    def __init__(self):
        RaStanza.__init__(self)

    def __setitem__(self, key, value):
        ordereddict.OrderedDict.__setitem__(self, key, value)
        ordereddict.OrderedDict.sort(self)
        ordereddict.OrderedDict.reorder(self, 0, 'term')
        ordereddict.OrderedDict.reorder(self, 1, 'tag')
        ordereddict.OrderedDict.reorder(self, 2, 'type')
        if 'label' in self.keys():
            ordereddict.OrderedDict.reorder(self, 3, 'label')
        
    def readStanza(self, stanza):
        '''
        Populates this entry from a single stanza
        '''
        
        for line in stanza:
            self.readLine(line)

        return self.readName(stanza[0])
        
    def readName(self, line):
        '''
        Extracts the Stanza's name from the value of the first line of the
        stanza.
        '''

        if len(line.split(' ', 1)) != 2:
            raise ValueError()

        names = map(str.strip, line.split(' ', 1))
        self._name = names[1]
        return names
        
    def readLine(self, line):
        '''
        Reads a single line from the stanza, extracting the key-value pair
        ''' 

        if line.startswith('#') or line == '':
            self.append(line)
        else:
            raKey = line.split(' ', 1)[0]
            raVal = ''
            if (len(line.split(' ', 1)) == 2):
                raVal = line.split(' ', 1)[1]
                
            if raKey in self:
                count = 0
                while raKey + '__$$' + str(count) in self:
                    count = count + 1
                    
                self[raKey + '__$$' + str(count)] = raVal
                
            else:
                self[raKey] = raVal
    
    
    
#     validate [cv/date/exists/float/integer/list:/none/regex:] outlines the expected values.  ENFORCED by mdbPrint -validate
#           cv: must be defined term in cv (e.g. cell=GM12878).  "cv or None" indicates that "None is also acceptable.
#               "cv or control" indicates that cv-defined terms of type "control" are also acceptable.
#         date: must be date in YYYY-MM-DD format
#       exists: not enforced.  (e.g. fileName could be validated to exist in download directory)
#        float: must be floating point number
#      integer: must be integer
#      "list:": must be one of several terms in comma delimeited list (e.g. "list: yes,no,maybe" )  # ("list:" includes colon)
#         none: not validated in any way
#     "regex:": must match regular expression (e.g. "regex: ^GS[M,E][0-9]$" )  # ("regex:" includes colon)
#    # NOTE: that validate rules may end comment delimited by a '#'
        
    def validate(self, cvfile):
        type = self['type']
        if self['type'] == 'Cell Line' or self['type'] == 'cell' or self['type'] == 'cellType': # :(
            if 'organism' in self and self['organism'] == 'human':
                type = 'cellType'
            elif 'organism' in self and self['organism'] == 'mouse':
                type = 'mouseCellType'
            else:
                cvfile.handler(OrganismError(self))
                
        if self['type'] == 'Antibody':
            type == 'antibody'
        
        typeStanza = cvfile.getTypeOfTermStanza(type)
        if typeStanza == None:
            cvfile.handler(InvalidTypeError(self, self['type'] + '(%s)' % type))
            return
        required = list()
        if 'requiredVars' in typeStanza:
            required = extractList(typeStanza['requiredVars'])
        optional = list()
        if 'optionalVars' in typeStanza:
            optional = extractList(typeStanza['optionalVars'])
        
        self.checkMandatory(cvfile, required)
        required.extend(optional)
        self.checkExtraneous(cvfile, required)
        self.checkDuplicates(cvfile)
        
        for key in self.iterkeys():
            
            itemType = cvfile.getTypeOfTermStanza(key)
            if itemType == None:
                cvfile.missingTypes.add(key)
                continue
            validation = itemType['validate']
            val = self[key]
            
            if validation.startswith('cv'):
                if validation == 'cv or None' and val == 'None':
                    pass
                else:
                    self.checkRelational(cvfile, val, key)
            elif validation == 'date':
                try:
                    d = datetime.datetime.strptime(val, '%Y-%m-%d')
                except:
                    cvfile.handler(InvalidDateError(self, val))
            elif validation == 'exists':
                if not os.path.exists(val):
                    cvfile.handler(MissingFileError(self, val))
            elif validation == 'float':
                try:
                    f = float(val)
                except:
                    cvfile.handler(InvalidFloatError(self, val))
            elif validation == 'integer':
                try:
                    i = int(val)
                except:
                    cvfile.handler(InvalidIntError(self, val))
            elif validation.startswith('list:'):
                validVals = extractList(validation, 'list:')
                if val not in validVals:
                    cvfile.handler(InvalidListError(self, val, validVals))
            elif validation == 'none':
                pass
            elif validation.startswith('regex:'):
                regex = extractValue(validation, 'regex:')
                if not re.match(val, regex):
                    cvfile.handler(UnmatchedRegexError(self, val, regex))
        
    def checkDuplicates(self, cvfile):
        '''ensure that all keys are present and not blank in the stanza'''
        for key in self.iterkeys():
            if '__$$' in key:
                newkey = key.split('__$$', 1)[0]
                cvfile.handler(DuplicateKeyError(self, newkey))
        
    def checkMandatory(self, cvfile, keys):
        '''ensure that all keys are present and not blank in the stanza'''
        for key in keys:
            if not key in self.keys():
                cvfile.handler(MissingKeyError(self, key))
            elif self[key] == '':
                cvfile.handler(BlankKeyError(self, key))
               
    def checkExtraneous(self, cvfile, keys):
        '''check for keys that are not in the list of keys'''
        for key in self.iterkeys():
            if key not in keys and '__$$' not in key:
                cvfile.handler(ExtraKeyError(self, key))
    
    def checkFullRelational(self, cvfile, key, other, type):
        '''check that the value at key matches the value of another
        stanza's value at other, where the stanza type is specified by type'''
        
        p = 0
        if key not in self:
            return
        
        for entry in cvfile.itervalues():
            if 'type' in entry and other in entry:
                if entry['type'] == type and self[key] == entry[other]:
                    p = 1
                    break
        if p == 0:
            cvfile.handler(NonmatchKeyError(self, key, other))
    
    def checkRelational(self, cvfile, key, other):
        '''check that the value at key matches the value at other'''
        p = 0
        
        if key not in self:
            return
        
        for entry in cvfile.itervalues():
            if 'type' in entry and other in entry:
                if entry['type'] == key and self[key] == entry[other]:
                    p = 1
                    break
        if p == 0:
            cvfile.handler(NonmatchKeyError(self, key, other))
            
    def checkListRelational(self, cvfile, key, other):
        '''check that the value at key matches the value at other'''
        
        if key not in self:
            return
        
        for val in self[key].split(','):
            val = val.strip()
            p = 0
        
            for entry in cvfile.itervalues():
                if 'type' in entry and other in entry:

                    if entry['type'] == key and val == entry[other]:
                        p = 1
                        break
            if p == 0:
                cvfile.handler(NonmatchKeyError(self, key, other))

    def checkProtocols(self, cvfile, path):
        if 'protocol' in self:
            protocols = self['protocol'].split()
            for protocol in protocols:
                if ':' not in protocol:
                    cvfile.handler(InvalidProtocolError(self, protocol))
                else:
                    p = protocol.split(':', 1)[1]
                    if cvfile.protocolPath != None and not os.path.isfile(cvfile.protocolPath + path + p):
                        cvfile.handler(InvalidProtocolError(self, protocol))
                
class CvError(Exception):
    '''base error class for the cv.'''
    def __init__(self, stanza):
        Exception.__init__(self)
        self.stanza = stanza
        self.msg = ''
        self.strict = 0
        
    def __str__(self):
        return str('%s[%s] %s: %s' % (self.stanza.name, self.stanza['type'], self.__class__.__name__, self.msg))
        
class MissingKeyError(CvError):
    '''raised if a mandatory key is missing'''
    
    def __init__(self, stanza, key):
        CvError.__init__(self, stanza)
        self.msg =  key
        self.strict = 1
    
class DuplicateKeyError(CvError):
    '''raised if a key is duplicated'''
    
    def __init__(self, stanza, key):
        CvError.__init__(self, stanza)
        self.msg = key
        self.strict = 1
    
class BlankKeyError(CvError):
    '''raised if a mandatory key is blank'''
    
    def __init__(self, stanza, key):
        CvError.__init__(self, stanza)
        self.msg = key
        self.strict = 0
    
class ExtraKeyError(CvError):
    '''raised if an extra key not in the list of keys is found'''

    def __init__(self, stanza, key):
        CvError.__init__(self, stanza)
        self.msg = key
        self.strict = 0
        
class NonmatchKeyError(CvError):
    '''raised if a relational key does not match any other value'''
    
    def __init__(self, stanza, key, val):
        CvError.__init__(self, stanza)
        self.msg = '%s does not match %s' % (key, val)
        self.strict = 1
        
class DuplicateVendorIdError(CvError):
    '''When there exists more than one connected component of stanzas (through derivedFrom) with the same vendorId'''
    
    def __init__(self, stanza):
        CvError.__init__(self, stanza)
        self.msg = '%s' % self.stanza['vendorId']
        self.strict = 0
        
class InvalidProtocolError(CvError):
    '''raised if a protocol doesnt match anything in the directory'''
    
    def __init__(self, stanza, key):
        CvError.__init__(self, stanza)
        self.msg = key
        self.strict = 0
    
class InvalidTypeError(CvError):
    '''raised if a relational key does not match any other value'''
    
    def __init__(self, stanza, key):
        CvError.__init__(self, stanza)
        self.msg = key
        self.strict = 1
    
class TypeValidationError(CvError):
    '''raised if the terms type of term has an invalid validation value'''
    
    def __init__(self, stanza):
        CvError.__init__(self, stanza)
        self.msg = 'validation ' + stanza['validation']
        self.strict = 1
       
class InvalidDateError(CvError):
    '''raised if the value is an invalid date'''
    
    def __init__(self, stanza, val):
        CvError.__init__(self, stanza)
        self.msg = val + ' does not match a YYYY-MM-DD date'
        self.strict = 1      

class MissingFileError(CvError):
    '''raised if the value is a filename that does not exist'''
    
    def __init__(self, stanza, val):
        CvError.__init__(self, stanza)
        self.msg = val + ' does not exist'
        self.strict = 1        

class InvalidFloatError(CvError):
    '''raised if the value not a float'''
    
    def __init__(self, stanza, val):
        CvError.__init__(self, stanza)
        self.msg = val + ' is not a float'
        self.strict = 1           
        
class InvalidIntError(CvError):
    '''raised if the value is not an int'''
    
    def __init__(self, stanza, val):
        CvError.__init__(self, stanza)
        self.msg = val + ' is not an int'
        self.strict = 1   
        
class InvalidListError(CvError):
    '''raised if the value is not among the given list of values'''
    
    def __init__(self, stanza, val, list):
        CvError.__init__(self, stanza)
        self.msg = val + ' is not in ' + list.join(',')
        self.strict = 1   
        
class UnmatchedRegexError(CvError):
    '''raised if the value is a filename that does not exist'''
    
    def __init__(self, stanza, val, regex):
        CvError.__init__(self, stanza)
        self.msg = val + ' does not match the regex ' + regex
        self.strict = 1       

class OrganismError(CvError):
    '''raised if the value is a filename that does not exist'''
    
    def __init__(self, stanza):
        CvError.__init__(self, stanza)
        if 'organism' in stanza:
            self.msg = 'organism ' + stanza['organism'] + ' does not match human or mouse'
        else:
            self.msg = 'organism does not exist in stanza'
        self.strict = 1           
