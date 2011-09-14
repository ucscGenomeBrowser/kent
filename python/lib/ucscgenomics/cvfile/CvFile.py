import re
import os
from ucscgenomics.rafile.RaFile import *

class CvFile(RaFile):
	"""cv.ra representation. Mainly adds CV-specific validation to the RaFile"""

	def __init__(self, filePath=None, handler=None, protocolPath=None):
		"""sets up exception handling method, and optionally reads from a file"""
		RaFile.__init__(self)
		
		self.handler = handler
		if handler == None:
			self.handler = self.raiseException
			
		if filePath == None:
			filePath = os.path.expanduser('~/kent/src/hg/makeDb/trackDb/') + 'cv/alpha/cv.ra'
		
		self.protocolPath = protocolPath
		if protocolPath == None:
			self.protocolPath == os.path.expanduser('~/htdocsExtras/ENCODE/')
		
		self.read(filePath)

	def raiseException(self, exception):
		"""wrapper function for raising exception"""
		raise exception

	def readStanza(self, stanza):
		"""overriden method from RaFile which makes specialized stanzas based on type"""
		e = RaStanza()
		ek, ev = e.readStanza(stanza)
		type = e['type']

		if type == 'Antibody':
			entry = AntibodyStanza()
		elif type == 'Cell Line':
			if e['organism'] == 'human':
				entry = CellLineStanza()
			elif e['organism'] == 'mouse':
				entry = MouseStanza()
			else:
				self.handler(NonmatchKeyError(e.name, e['organism'], 'organism'))
				return ek, ev, None
		elif type == 'age':
			entry = AgeStanza()
		elif type == 'dataType':
			entry = DataTypeStanza()
		elif type == 'lab':
			entry = LabStanza()
		elif type == 'seqPlatform':
			entry = SeqPlatformStanza()
		elif type == 'typeOfTerm':
			entry = TypeOfTermStanza()
		elif type == 'view':
			entry = ViewStanza()
		elif type == 'localization':
			entry = LocalizationStanza()
		elif type == 'rnaExtract':
			entry = RnaExtractStanza()
		elif type == 'treatment':
			entry = TreatmentStanza()
		elif type == 'grant':
			entry = GrantStanza()
		else:
			entry = CvStanza()

		key, val = entry.readStanza(stanza)
		return key, val, entry


	def validate(self):
		"""base validation method which calls all stanzas' validate"""
		for stanza in self.itervalues():
				stanza.validate(self)

				
class CvStanza(RaStanza):
	"""base class for a single stanza in the cv, which adds validation"""
	
	def __init__(self):
		RaStanza.__init__(self)

	def readStanza(self, stanza):
		"""
		Populates this entry from a single stanza
		"""
		
		for line in stanza:
			self.readLine(line)

		return self.readName(stanza[0])
		
	def readName(self, line):
		"""
		Extracts the Stanza's name from the value of the first line of the
		stanza.
		"""

		if len(line.split(' ', 1)) != 2:
			raise ValueError()

		names = map(str.strip, line.split(' ', 1))
		self._name = names[1]
		return names
		
	def readLine(self, line):
		"""
		Reads a single line from the stanza, extracting the key-value pair
		""" 

		if line.startswith('#') or line == '':
			OrderedDict.append(self, line)
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
		
	def validate(self, ra, necessary=None, optional=None):
		"""default validation for a generic cv stanza. Should be called with all arguments if overidden"""
		
		if necessary == None:
			necessary = set()
			
		if optional == None:
			optional = set()
		
		baseNecessary = {'term', 'tag', 'type'}
		
		if self['type'] != 'Antibody':
			baseNecessary.add('description')
		
		baseOptional = {'deprecated'}
		self.checkMandatory(ra, necessary | baseNecessary)
		self.checkExtraneous(ra, necessary | baseNecessary | optional | baseOptional)
		
		if self['type'] != 'Cell Line': # cv, you disgust me with your inconsistencies
			if len(ra.filter(lambda s: s['term'] == self['type'] and s['type'] == 'typeOfTerm', lambda s: s)) == 0:
				ra.handler(InvalidTypeError(self, self['type']))

		self.checkDuplicates(ra)
		
		
	def checkDuplicates(self, ra):
		"""ensure that all keys are present and not blank in the stanza"""
		for key in self.iterkeys():
			if '__$$' in key:
				newkey = key.split('__$$', 1)[0]
				ra.handler(DuplicateKeyError(self, newkey))
		
	def checkMandatory(self, ra, keys):
		"""ensure that all keys are present and not blank in the stanza"""
		for key in keys:
			if not key in self.keys():
				ra.handler(MissingKeyError(self, key))
			elif self[key] == '':
				ra.handler(BlankKeyError(self, key))
				
	# def checkOptional(self, ra, keys):
		# """ensure that all keys are present and not blank in the stanza"""
		# for key in keys:
			# if key in self and self[key] == '':
				# ra.handler(BlankKeyError(self, key))
		
	def checkExtraneous(self, ra, keys):
		"""check for keys that are not in the list of keys"""
		for key in self.iterkeys():
			if key not in keys and '__$$' not in key:
				ra.handler(ExtraKeyError(self, key))
	
	def checkFullRelational(self, ra, key, other, type):
		"""check that the value at key matches the value of another
		stanza's value at other, where the stanza type is specified by type"""
		
		p = 0
		if key not in self:
			return
		
		for entry in ra.itervalues():
			if 'type' in entry and other in entry:
				if entry['type'] == type and self[key] == entry[other]:
					p = 1
					break
		if p == 0:
			ra.handler(NonmatchKeyError(self, key, other))
	
	def checkRelational(self, ra, key, other):
		"""check that the value at key matches the value at other"""
		p = 0
		
		if key not in self:
			return
		
		for entry in ra.itervalues():
			if 'type' in entry and other in entry:
				if entry['type'] == key and self[key] == entry[other]:
					p = 1
					break
		if p == 0:
			ra.handler(NonmatchKeyError(self, key, other))
			
	def checkListRelational(self, ra, key, other):
		"""check that the value at key matches the value at other"""
		
		if key not in self:
			return
		
		for val in self[key].split(','):
			val = val.strip()
			p = 0
		
			for entry in ra.itervalues():
				if 'type' in entry and other in entry:

					if entry['type'] == key and val == entry[other]:
						p = 1
						break
			if p == 0:
				ra.handler(NonmatchKeyError(self, key, other))

	def checkProtocols(self, ra, path):
		if 'protocol' in self:
			protocols = self['protocol'].split()
			for protocol in protocols:
				p = protocol.split(':', 1)[1]
				if not os.path.isfile(ra.protocolPath + path + p):
					ra.handler(InvalidProtocolError(self, protocol))
				
class CvError(Exception):
	"""base error class for the cv."""
	def __init__(self, stanza):
		self.stanza = stanza
		self.msg = ''
		
	def __str__(self):
		return str('%s[%s] %s: %s' % (self.stanza.name, self.stanza['type'], self.__class__.__name__, self.msg))
		
class MissingKeyError(CvError):
	"""raised if a mandatory key is missing"""
	
	def __init__(self, stanza, key):
		CvError.__init__(self, stanza)
		self.msg =  key
	
	# def __str__(self):
		# return str('%s(%s[%s])' % self.__class__.__name__ self.stanza + ': missing key (' + self.key + ')')
	
	
class DuplicateKeyError(CvError):
	"""raised if a key is duplicated"""
	
	def __init__(self, stanza, key):
		CvError.__init__(self, stanza)
		self.msg = key
	
	# def __str__(self):
		# return str(self.stanza + ': duplicate key (' + self.key + ')')
	
	
class BlankKeyError(CvError):
	"""raised if a mandatory key is blank"""
	
	def __init__(self, stanza, key):
		CvError.__init__(self, stanza)
		self.msg = key
	
	# def __str__(self):
		# return str(self.stanza + ': key (' + self.key + ') is blank')
	
	
class ExtraKeyError(CvError):
	"""raised if an extra key not in the list of keys is found"""

	def __init__(self, stanza, key):
		CvError.__init__(self, stanza)
		self.msg = key
	
	# def __str__(self):
		# return str(self.stanza + ': extra key (' + self.key + ')')	

		
class NonmatchKeyError(CvError):
	"""raised if a relational key does not match any other value"""
	
	def __init__(self, stanza, key, val):
		CvError.__init__(self, stanza)
		self.msg = '%s does not match %s' % (key, val)
	
	# def __str__(self):
		# return str(self.stanza + ': key (' + self.key + ') does not match any (' + self.val + ')')
		
		
class DuplicateVendorIdError(CvError):
	"""When there exists more than one connected component of stanzas (through derivedFrom) with the same vendorId"""
	
	def __init__(self, stanza):
		CvError.__init__(self, stanza)
		self.msg = '%s' % self.stanza['vendorId']
		
	# def __str__(self):
		# return str('warning: ' + self.stanza.name + ': vendorId (' + self.stanza['vendorId'] + ') has multiple parent cell lines')
		
		
class InvalidProtocolError(CvError):
	"""raised if a protocol doesnt match anything in the directory"""
	
	def __init__(self, stanza, key):
		CvError.__init__(self, stanza)
		self.msg = key
	
	# def __str__(self):
		# return str(self.stanza.name + ': missing protocol document (' + self.key + ')')	
	
		
class InvalidTypeError(CvError):
	"""raised if a relational key does not match any other value"""
	
	def __init__(self, stanza, key):
		CvError.__init__(self, stanza)
		self.msg = key
	
	# def __str__(self):
		# return str(self.stanza + ': ' + self.key + ' does not match any types')
		

class LabStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'organism', 'labPi'}
		optional = {'label', 'labInst', 'labPiFull', 'grantPi'}
		CvStanza.validate(self, ra, necessary, optional)

		self.checkRelational(ra, 'organism', 'term')
		

class AgeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'stage'}
		CvStanza.validate(self, ra, necessary)


class DataTypeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'label'}
		CvStanza.validate(self, ra, necessary)


class CellLineStanza(CvStanza):

	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'organism', 'vendorName', 'orderUrl', 'sex', 'tier'}
		optional = {'tissue', 'vendorId', 'karyotype', 'lineage', 'termId', 'termUrl', 'color', 'protocol', 'category', 'lots', 'derivedFrom', 'lab'}
		CvStanza.validate(self, ra, necessary, optional)

		self.checkRelational(ra, 'organism', 'term')
		self.checkRelational(ra, 'sex', 'term')
		self.checkRelational(ra, 'category', 'term')
		self.checkRelational(ra, 'tier', 'term')
		self.checkListRelational(ra, 'lab', 'labPi')
		
		# ensure the derivedFrom matches a valid cell line
		if 'derivedFrom' in self and len(ra.filter(lambda s: s['term'] == self['derivedFrom'] and s['type'] == 'Cell Line', lambda s: s)) == 0:
			ra.handler(NonmatchKeyError(self, self['derivedFrom'], 'Cell Line'))
			
		# ensure that there are no other non-related stanzas that have the same vendorId
		if 'derivedFrom' not in self or ra[self['derivedFrom']]['vendorId'] != self['vendorId']:
			otherstanzas = ra.filter(lambda s: s['type'] == 'Cell Line' and s != self and s['vendorId'] == self['vendorId'] and ('derivedFrom' not in s or ra[s['derivedFrom']]['vendorId'] != s['vendorId']), lambda s: s)
			if len(otherstanzas) > 0:
				ra.handler(DuplicateVendorIdError(self))
			
		self.checkProtocols(ra, 'protocols/cell/human/')
		

class SeqPlatformStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		optional = {'geo'}
		CvStanza.validate(self, ra, None, optional)


class AntibodyStanza(CvStanza):

	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'target', 'antibodyDescription', 'targetDescription', 'vendorName', 'vendorId', 'orderUrl', 'targetId', 'lab'}
		optional = {'validation', 'targetUrl', 'lots', 'displayName'}
		CvStanza.validate(self, ra, necessary, optional)
		self.checkListRelational(ra, 'lab', 'labPi')
		self.checkProtocols(ra, 'validation/antibodies/')


class ViewStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'label'}
		CvStanza.validate(self, ra, necessary)
		

class TypeOfTermStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'searchable', 'cvDefined', 'validate', 'priority'}
		optional = {'deprecated', 'label', 'hidden'}
		CvStanza.validate(self, ra, necessary, optional)
		
		if len(ra.filter(lambda s: s['term'] == self['type'] and s['type'] == 'typeOfTerm', lambda s: s)) == 0:
			ra.handler(InvalidTypeError(self, self['type']))
				

class MouseStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'organism', 'vendorName', 'orderUrl', 'age', 'strain', 'sex'}
		optional = {'tissue', 'termId', 'termUrl', 'color', 'protocol', 'category', 'vendorId', 'lots'}
		CvStanza.validate(self, ra, necessary, optional)
		
		self.checkRelational(ra, 'organism', 'term')
		self.checkRelational(ra, 'sex', 'term')
		self.checkRelational(ra, 'category', 'term')
		self.checkRelational(ra, 'age', 'term')
		self.checkRelational(ra, 'strain', 'term')
		self.checkProtocols(ra, 'protocols/cell/mouse/')

		
class LocalizationStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'termId', 'termUrl'}
		optional = {'label'}
		CvStanza.validate(self, ra, necessary, optional)
		
		
class RnaExtractStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		optional = {'label'}
		CvStanza.validate(self, ra, None, optional)
		
		
class TreatmentStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		optional = {'label'}
		CvStanza.validate(self, ra, None, optional)
		
		
class GrantStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'grantInst', 'projectName'}
		optional = {'label'}
		CvStanza.validate(self, ra, necessary, optional)
		