import re
from RaFile import *

class CvFile(RaFile):

	def __init__(self, filePath=''):
		RaFile.__init__(self)
		self.errors = list()
		if filePath != '':
			self.read(filePath)


	def readStanza(self, stanza):
		e = RaStanza()
		e.readStanza(stanza)
		type = e['type']

		# this will become a 30 case if-else block if I do it this way,
		# I'll probably create a dictionary for this.
		if type == 'Antibody':
			entry = AntibodyStanza()
		elif type == 'Cell Line':
			entry = CellLineStanza()
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
		elif type == 'mouse':
			entry = MouseStanza()
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
		else:
			raise NonmatchKeyError(entry.name, type, 'type')

		key, val = entry.readStanza(stanza)
		return key, val, entry


	def validate(self):
		for stanza in self.itervalues():
			try:
				stanza.validate(self)
			except CvError as e:
				self.errors.append(e)
		
		for err in self.errors:
			print err

class CvStanza(RaStanza):

	def __init__(self):
		RaStanza.__init__(self)

	def validate(self, ra):
		#print 'CvStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description'}
		self.checkMandatory(necessary)
		
	def checkMandatory(self, keys):
		for key in keys:
			if not key in self.keys():
				raise MissingKeyError(self.name, key)
			if self[key] == '':
				raise BlankKeyError(self.name, key)
		
	def checkExtraneous(self, keys):
		for key in self.iterkeys():
			if key not in keys:
				raise ExtraKeyError(self.name, key)
	
	#check that the value at 'key' matches the value at some 'other'
	def checkRelational(self, ra, key, other):
		p = 0
		if key not in self:
			return
			
		for entry in ra.itervalues():
			if 'type' in entry and other in entry:
				if entry['type'] == key and self[key] == entry[other]:
					p = 1
		if p == 0:
			raise NonmatchKeyError(self.name, key, other)


class CvError(Exception):
	pass
		
		
class MissingKeyError(CvError):
	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return repr(self.stanza + ': missing key (' + self.key + ')')
	
	
class BlankKeyError(CvError):
	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return repr(self.stanza + ': key (' + self.key + ') is blank')
	
	
class ExtraKeyError(CvError):
	def __init__(self, stanza, key):
		self.stanza = stanza
		self.key = key
	
	def __str__(self):
		return repr(self.stanza + ': extra key (' + self.key + ')')	

		
class NonmatchKeyError(CvError):
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

		self.checkMandatory(necessary)
		self.checkExtraneous(necessary | optional)
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

		self.checkMandatory(necessary)
		self.checkExtraneous(necessary)


class DataTypeStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'ViewStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description', 'label'}

		self.checkMandatory(necessary)
		self.checkExtraneous(necessary)


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

		necessary = {'term', 'tag', 'type', 'description', 'organism', 'vendorName', 'orderUrl', 'sex', 'tier'}
		optional = {'tissue', 'vendorId', 'karyotype', 'lineage', 'termId', 'termUrl', 'color', 'protocol', 'category'}

		self.checkMandatory(necessary)
		self.checkExtraneous(necessary | optional)
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

		self.checkMandatory(necessary)
		self.checkExtraneous(necessary | optional)


class AntibodyStanza(CvStanza):

	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'AntibodyStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description', 'target', 'antibodyDescription', 'targetDescription', 'vendorName', 'vendorId', 'orderUrl', 'lab', 'targetId'}
		optional = {'validation', 'targetUrl'}

		self.checkMandatory(necessary)
		self.checkExtraneous(necessary | optional)
		self.checkRelational(ra, 'lab', 'labPi')


class ViewStanza(CvStanza):
	
	def __init__(self):
		CvStanza.__init__(self)

	def validate(self, ra):
		#print 'ViewStanza.validate(' + self.name + ')'

		necessary = {'term', 'tag', 'type', 'description', 'label'}

		self.checkMandatory(necessary)
		self.checkExtraneous(necessary)


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
		
		self.checkMandatory(necessary)
		self.checkExtraneous(necessary | optional)
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


