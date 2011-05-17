import re
from RaFile import *

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

		# this will become a 30 case if-else block if I do it this way,
		# I'll probably create a dictionary for this.
		if type == 'Antibody':
			entry = AntibodyStanza()
		elif type == 'Cell Line':
			if e['organism'] == 'Human':
				entry = CellLineStanza()
			elif e['organism'] == 'Mouse':
				entry = MouseStanza()
			else:
				self.handler(NonmatchKeyError(e.name, e['organism'], 'organism'))
				return ek, ev, None
		elif type == 'Gene Type':
			entry = GeneTypeStanza()
		elif type == 'age':
			entry = AgeStanza()
		elif type == 'control':
			entry = ControlStanza()
		elif type == 'dataType':
			entry = DataTypeStanza()
		elif type == 'fragSize':
			entry = FragSizeStanza()
		elif type == 'freezeDate':
			entry = FreezeDateStanza()
		elif type == 'grant':
			entry = GrantStanza()
		elif type == 'insertLength':
			entry = InsertLengthStanza()
		elif type == 'lab':
			entry = LabStanza()
		elif type == 'localization':
			entry = LocalizationStanza()
		elif type == 'mapAlgorithm':
			entry = MapAlgorithmStanza()
		elif type == 'organism':
			entry = OrganismStanza()
		elif type == 'phase':
			entry = PhaseStanza()
		elif type == 'promoter':
			entry = PromoterStanza()
		elif type == 'protocol':
			entry = ProtocolStanza()
		elif type == 'readType':
			entry = ReadTypeStanza()
		elif type == 'region':
			entry = RegionStanza()
		elif type == 'restrictionEnzyme':
			entry = RestrictionEnzymeStanza()
		elif type == 'rnaExtract':
			entry = RnaExtractStanza()
		elif type == 'seqPlatform':
			entry = SeqPlatformStanza()
		elif type == 'sex':
			entry = SexStanza()
		elif type == 'species':
			entry = SpeciesStanza()
		elif type == 'strain':
			entry = StrainStanza()
		elif type == 'tier':
			entry = TierStanza()
		elif type == 'tissueSourceType':
			entry = TissueSourceTypeStanza()
		elif type == 'treatment':
			entry = TreatmentStanza()
		elif type == 'typeOfTerm':
			entry = TypeOfTermStanza()
		elif type == 'version':
			entry = VersionStanza()
		elif type == 'view':
			entry = ViewStanza()
		elif type == 'category':
			entry = CategoryStanza()
		else:
			self.handler(NonmatchKeyError(e.name, type, 'type'))
			return ek, ev, None

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

	def validate(self, ra):
		"""default validation for a generic cv stanza."""
		#print 'CvStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description'}
		self.checkMandatory(ra, necessary)
		
	def checkMandatory(self, ra, keys):
		"""ensure that all keys are present and not blank in the stanza"""
		for key in keys:
			if not key in self.keys():
				ra.handler(MissingKeyError(self.name, key))
			elif self[key] == '':
				ra.handler(BlankKeyError(self.name, key))
		
	def checkExtraneous(self, ra, keys):
		"""check for keys that are not in the list of keys"""
		for key in self.iterkeys():
			if key not in keys:
				ra.handler(ExtraKeyError(self.name, key))
	
	def checkRelational(self, ra, key, other):
		"""check that the value at key matches the value at other"""
		p = 0
		
		#if (self['term'] == "CH12"):
		#	print "checkrelational CH12";
		
		if key not in self:
			return
		
		for entry in ra.itervalues():
			if 'type' in entry and other in entry:
				
				#if (self['term'] == "CH12"):
				#	print self['term'] + ': entrytype: ' + entry['type'] + ' ?= key: ' + key + ' and selfkey: ' + self[key] + ' ?= entryother: ' + entry[other]
				
				if entry['type'] == key and self[key] == entry[other]:
					
					#if (self['term'] == "CH12"):
					#	print 'passed'
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
		return repr(self.stanza + ': missing key (' + self.key + ')')
	
	
class BlankKeyError(CvError):
	"""raised if a mandatory key is blank"""
	
	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return repr(self.stanza + ': key (' + self.key + ') is blank')
	
	
class ExtraKeyError(CvError):
	"""raised if an extra key not in the list of keys is found"""

	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return repr(self.stanza + ': extra key (' + self.key + ')')	

		
class NonmatchKeyError(CvError):
	"""raised if a relational key does not match any other value"""
	
	def __init__(self, stanza, key, val):
		self.stanza = stanza
		self.key = key
		self.val = val
	
	def __str__(self):
		return repr(self.stanza + ': key (' + self.key + ') does not match any (' + self.val + ')')
		
		
class GrantStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class RestrictionEnzymeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class VersionStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class TreatmentStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class SexStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class FragSizeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class LocalizationStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class OrganismStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class GeneTypeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class LabStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'LabStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description', 'organism', 'labPi'}
		optional = {'label', 'labInst', 'labPiFull', 'grantPi'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary | optional)
		self.checkRelational(ra, 'organism', 'term')


class PhaseStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class AgeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'AgeStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description', 'stage'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary)


class DataTypeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'ViewStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description', 'label'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary)


class RegionStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class CellLineStanza(CvStanza):

	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'CellLineStanza.validate(' + self.name + ')'

		#if (self['term'] == 'GM12878'):
		#	print 'GM12878 Validate'
		
		necessary = {'term', 'tag', 'type', 'description', 'organism', 'vendorName', 'orderUrl', 'sex', 'tier'}
		optional = {'tissue', 'vendorId', 'karyotype', 'lineage', 'termId', 'termUrl', 'color', 'protocol', 'category'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary | optional)
		self.checkRelational(ra, 'organism', 'term')
		self.checkRelational(ra, 'sex', 'term')
		self.checkRelational(ra, 'category', 'term')
		self.checkRelational(ra, 'tier', 'term')


class ReadTypeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class MapAlgorithmStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class PromoterStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class TierStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class RnaExtractStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class TissueSourceTypeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class SeqPlatformStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'SeqPlatformStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description'}
		optional = {'geo'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary | optional)


class AntibodyStanza(CvStanza):

	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'AntibodyStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'target', 'antibodyDescription', 'targetDescription', 'vendorName', 'vendorId', 'orderUrl', 'lab', 'targetId'}
		optional = {'validation', 'targetUrl', 'lots'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary | optional)
		self.checkRelational(ra, 'lab', 'labPi')


class ViewStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'ViewStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description', 'label'}

		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary)


class ControlStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class TypeOfTermStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class ProtocolStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class FreezeDateStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class StrainStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class InsertLengthStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'


class MouseStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'MouseStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description', 'organism', 'vendorName', 'orderUrl', 'age', 'strain', 'sex'}
		optional = {'tissue', 'termId', 'termUrl', 'color', 'protocol', 'category'}
		
		self.checkMandatory(ra, necessary)
		self.checkExtraneous(ra, necessary | optional)
		self.checkRelational(ra, 'organism', 'term')
		self.checkRelational(ra, 'sex', 'term')
		self.checkRelational(ra, 'category', 'term')
		self.checkRelational(ra, 'age', 'term')
		self.checkRelational(ra, 'strain', 'term')



class SpeciesStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'

class CategoryStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		CvStanza.validate(self, ra)
		#print 'validate(' + self.name + ')'
