import re
from ucscgenomics.rafile.RaFile import *

class CvFile(RaFile):
	"""cv.ra representation. Mainly adds CV-specific validation to the RaFile"""

	def __init__(self, filePath='', handler=None):
		"""sets up exception handling method, and optionally reads from a file"""
		RaFile.__init__(self)
		if handler != None:
			self.handler = handler
		else:
			self.handler = self.raiseException
		if filePath != '':
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
		else:
			entry = CvStanza()
			#self.handler(NonmatchKeyError(e.name, type, 'type'))
			#return ek, ev, None

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
		
	def validate(self, ra):
		"""default validation for a generic cv stanza."""

		necessary = {'term', 'tag', 'type', 'description'}
		self.checkMandatory(ra, necessary)
		
		if len(ra.filter(lambda s: s['term'] == self['type'] and s['type'] == 'typeOfTerm', lambda s: s)) == 0:
			ra.handler(InvalidTypeError(self.name, self['type']))
		
		self.checkDuplicates(ra)
		
	def checkDuplicates(self, ra):
		"""ensure that all keys are present and not blank in the stanza"""
		for key in self.iterkeys():
			if '__$$' in key:
				newkey = key.split('__$$', 1)[0]
				ra.handler(DuplicateKeyError(self.name, newkey))
		
	def checkMandatory(self, ra, keys):
		"""ensure that all keys are present and not blank in the stanza"""
		for key in keys:
			if not key in self.keys():
				ra.handler(MissingKeyError(self.name, key))
			elif self[key] == '':
				ra.handler(BlankKeyError(self.name, key))
				
	def checkOptional(self, ra, keys):
		"""ensure that all keys are present and not blank in the stanza"""
		for key in keys:
			if key in self and self[key] == '':
				ra.handler(BlankKeyError(self.name, key))
		
	def checkExtraneous(self, ra, keys):
		"""check for keys that are not in the list of keys"""
		for key in self.iterkeys():
			if key not in keys and '__$$' not in key:
				ra.handler(ExtraKeyError(self.name, key))
	
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
			ra.handler(NonmatchKeyError(self.name, key, other))
	
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
			ra.handler(NonmatchKeyError(self.name, key, other))
			
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
				ra.handler(NonmatchKeyError(self.name, key, other))

class CvError(Exception):
	"""base error class for the cv."""
	pass
		
		
class MissingKeyError(CvError):
	"""raised if a mandatory key is missing"""
	
	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return str(self.stanza + ': missing key (' + self.key + ')')
	
	
class DuplicateKeyError(CvError):
	"""raised if a key is duplicated"""
	
	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return str(self.stanza + ': duplicate key (' + self.key + ')')
	
	
class BlankKeyError(CvError):
	"""raised if a mandatory key is blank"""
	
	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return str(self.stanza + ': key (' + self.key + ') is blank')
	
	
class ExtraKeyError(CvError):
	"""raised if an extra key not in the list of keys is found"""

	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return str(self.stanza + ': extra key (' + self.key + ')')	

		
class NonmatchKeyError(CvError):
	"""raised if a relational key does not match any other value"""
	
	def __init__(self, stanza, key, val):
		self.stanza = stanza
		self.key = key
		self.val = val
	
	def __str__(self):
		return str(self.stanza + ': key (' + self.key + ') does not match any (' + self.val + ')')
		
		
class InvalidTypeError(CvError):
	"""raised if a relational key does not match any other value"""
	
	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return str(self.stanza + ': ' + self.key + ' does not match any types')
		
		
# class GrantStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class RestrictionEnzymeStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class VersionStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class TreatmentStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class SexStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class FragSizeStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class LocalizationStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class OrganismStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class GeneTypeStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

class LabStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'term', 'tag', 'type', 'description', 'organism', 'labPi'}
		optional = {'label', 'labInst', 'labPiFull', 'grantPi'}

		self.checkMandatory(ra, necessary)
		self.checkOptional(ra, optional)
		self.checkExtraneous(ra, necessary | optional)
		self.checkRelational(ra, 'organism', 'term')
		self.checkDuplicates(ra)


# class PhaseStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

class AgeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'term', 'tag', 'type', 'description', 'stage'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary)
		self.checkDuplicates(ra)


class DataTypeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'term', 'tag', 'type', 'description', 'label'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary)
		self.checkDuplicates(ra)


# class RegionStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

class CellLineStanza(CvStanza):

	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'term', 'tag', 'type', 'description', 'organism', 'vendorName', 'orderUrl', 'sex', 'tier'}
		optional = {'tissue', 'vendorId', 'karyotype', 'lineage', 'termId', 'termUrl', 'color', 'protocol', 'category', 'lots', 'derivedFrom'}

		self.checkMandatory(ra, necessary)
		self.checkOptional(ra, optional)
		self.checkExtraneous(ra, necessary | optional)
		self.checkRelational(ra, 'organism', 'term')
		self.checkRelational(ra, 'sex', 'term')
		self.checkRelational(ra, 'category', 'term')
		self.checkRelational(ra, 'tier', 'term')
		
		if 'derivedFrom' in self and len(ra.filter(lambda s: s['term'] == self['derivedFrom'] and s['type'] == 'Cell Line', lambda s: s)) == 0:
			ra.handler(NonmatchKeyError(self.name, self['derivedFrom'], 'Cell Line'))
		
		self.checkDuplicates(ra)


# class ReadTypeStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

# class MapAlgorithmStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

# class PromoterStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

# class TierStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class RnaExtractStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

# class TissueSourceTypeStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

class SeqPlatformStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'term', 'tag', 'type', 'description'}
		optional = {'geo'}

		self.checkMandatory(ra, necessary)
		self.checkOptional(ra, optional)
		self.checkExtraneous(ra, necessary | optional)
		self.checkDuplicates(ra)


class AntibodyStanza(CvStanza):

	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'term', 'tag', 'type', 'target', 'antibodyDescription', 'targetDescription', 'vendorName', 'vendorId', 'orderUrl', 'lab', 'targetId'}
		optional = {'validation', 'targetUrl', 'lots', 'displayName'}

		self.checkMandatory(ra, necessary)
		self.checkOptional(ra, optional)
		self.checkExtraneous(ra, necessary | optional)
		self.checkListRelational(ra, 'lab', 'labPi')
		self.checkDuplicates(ra)


class ViewStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'term', 'tag', 'type', 'description', 'label'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary)
		self.checkDuplicates(ra)


# class ControlStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

class TypeOfTermStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		

# class ProtocolStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)


# class FreezeDateStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

# class StrainStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

# class InsertLengthStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		

class MouseStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		necessary = {'term', 'tag', 'type', 'description', 'organism', 'vendorName', 'orderUrl', 'age', 'strain', 'sex'}
		optional = {'tissue', 'termId', 'termUrl', 'color', 'protocol', 'category', 'vendorId', 'lots'}
		
		self.checkMandatory(ra, necessary)
		self.checkOptional(ra, optional)
		self.checkExtraneous(ra, necessary | optional)
		self.checkRelational(ra, 'organism', 'term')
		self.checkRelational(ra, 'sex', 'term')
		self.checkRelational(ra, 'category', 'term')
		self.checkRelational(ra, 'age', 'term')
		self.checkRelational(ra, 'strain', 'term')
		self.checkDuplicates(ra)


# class SpeciesStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)

		
# class CategoryStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		
		
# class AtticStanza(CvStanza):
	
	# def __init__(self):
		# CvStanza.__init__(self)

	# def validate(self, ra):
		# CvStanza.validate(self, ra)
		
