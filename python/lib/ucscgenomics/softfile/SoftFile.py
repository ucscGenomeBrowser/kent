import sys
import re
from ucscgenomics.ordereddict.OrderedDict import *
	
class SoftFile(OrderedDict):

	"""
	Stores an Ra file in a set of entries, one for each stanza in the file.
	"""

	def __init__(self, filePath=''):
		OrderedDict.__init__(self)
		if filePath != '':
			self.read(filePath) 

	def read(self, filePath):
		"""
		Reads an SoftFile stanza by stanza, and internalizes it.
		"""

		file = open(filePath, 'r')

		stanza = list()

		for line in file:
 
			line = line.strip()

			if line.startswith('^') and stanza != []:
				name, entry = self.readStanza(stanza)
				#print 'hit: ' + name
				if entry != None:
					if name in self:
						raise KeyError('Duplicate Key ' + name)
					self[name] = entry
				
				stanza = list()

			#print 'appending: ' + line
			stanza.append(line)

		file.close()
		
		name, entry = self.readStanza(stanza)
		#print 'hit: ' + name
		if entry != None:
			if name in self:
				raise KeyError('Duplicate Key ' + name)
			self[name] = entry


	def readStanza(self, stanza):

		if stanza[0].startswith('^SAMPLE'):
			entry = HighThroughputSampleStanza() #WILL HAVE TO CHANGE
		elif stanza[0].startswith('^SERIES'):
			entry = SeriesStanza()
		elif stanza[0].startswith('^PLATFORM'):
			entry = PlatformStanza()
		else:
			raise KeyError(stanza[0])

		val = entry.readStanza(stanza)
		return val, entry


	def iter(self):
		for item in self._OrderedDict__ordering:
			yield item


	def iterkeys(self):
		for item in self._OrderedDict__ordering:
			yield item


	def itervalues(self):
		for item in self._OrderedDict__ordering:
			yield self[item]


	def iteritems(self):
		for item in self._OrderedDict__ordering:
			yield [item]


	def __str__(self):
		str = ''
		for item in self.iterkeys():
			str += self[item].__str__()
			
		return str
		
			
class HighThroughputSoftFile(SoftFile):

	def __init__(self, filePath=''):
		SoftFile.__init__(self, filePath)

	def readStanza(self, stanza):
		if stanza[0].startswith('^SAMPLE'):
			entry = HighThroughputSampleStanza()
		elif stanza[0].startswith('^SERIES'):
			entry = SeriesStanza()
		else:
			raise KeyError(stanza[0])

		val = entry.readStanza(stanza)
		return val, entry
		
		
class MicroArraySoftFile(SoftFile):

	def __init__(self, filePath=''):
		SoftFile.__init__(self, filePath)
		
	def readStanza(self, stanza):
		if stanza[0].startswith('^SAMPLE'):
			entry = MicroArraySampleStanza()
		elif stanza[0].startswith('^SERIES'):
			entry = SeriesStanza()
		elif stanza[0].startswith('^PLATFORM'):
			entry = PlatformStanza()
		else:
			raise KeyError(stanza[0])

		val = entry.readStanza(stanza)
		return val, entry
		
			
class KeyRequired(object):
	pass
	
class KeyOptional(object):
	pass
	
class KeyZeroPlus(object):
	pass
	
class KeyOnePlus(object):
	pass
	
class KeyZeroPlusNumbered(object):
	pass
	
class KeyOnePlusNumbered(object):
	pass
	
class KeyZeroPlusChannel(object):
	pass
	
class KeyOnePlusChannel(object):
	pass
	

class SoftStanza(OrderedDict):
	"""
	Holds an individual entry in the RaFile.
	"""

	def __init__(self, keys):
		self._name = ''
		self.keys = keys
		OrderedDict.__init__(self)
		
	@property 
	def name(self):
		return self._name
		
	def readStanza(self, stanza):
		"""
		Populates this entry from a single stanza
		"""

		for line in stanza:
			self.__readLine(line)

		return self.__readName(stanza[0])


	def __readName(self, line):
		"""
		Extracts the Stanza's name from the value of the first line of the
		stanza.
		"""

		if len(line.split('=', 1)) != 2:
			raise ValueError()

		self._name = line.split('=', 1)[1].strip()
		return self._name

	def __readLine(self, line):
		"""
		Reads a single line from the stanza, extracting the key-value pair
		""" 
		key = line.split('=', 1)[0].strip()
		val = ''
		if (len(line.split('=', 1)) == 2):
			val = line.split('=', 1)[1].strip()
		
		#split on the last underscore to determine if we're using a numbered key or not
		splitkey = key.rsplit('_', 1)[0]
		channelkey = splitkey + '_ch'
		#if the key is a numbered key
		if splitkey in self.keys and (self.keys[splitkey] == KeyZeroPlusNumbered or self.keys[splitkey] == KeyOnePlusNumbered):
			self[key] = val
		
		#this is for channel data in MicroArraySamples
		elif channelkey in self.keys and (self.keys[channelkey] == KeyZeroPlusChannel or self.keys[channelkey] == KeyOnePlusChannel):
			self[key] = val
		
		#if its a single value (ie 0 or 1 allowed entries)
		elif key in self.keys and (self.keys[key] == KeyRequired or self.keys[key] == KeyOptional):
			self[key] = val

		else:
		
			if key not in self.keys:
				print splitkey
				raise KeyError(self._name + ': invalid key: ' + key)
			
			if (self.keys[key] == KeyRequired or self.keys[key] == KeyOptional) and key in self:
				raise KeyError(self._name + ': too many of key: ' + key)
				
			if key not in self:
				self[key] = list()
			self[key].append(val)


	def iter(self):
		yield iterkeys(self)


	def iterkeys(self):
		for item in self._OrderedDict__ordering:
			yield item


	def itervalues(self):
		for item in self._OrderedDict__ordering:
			yield self[item]


	def iteritems(self):
		for item in self._OrderedDict__ordering:
			yield item, self[item]


	def __str__(self):
		str = ''
		for key in self:
			if isinstance(self[key], basestring):
				str += key + ' = ' + self[key] + '\n'
			else:
				for val in self[key]:
					str += key + ' = ' + val + '\n'

		return str
		
	def write(self, filename):
		#check for absence of required vars
		file = open(filename, 'r')
		file.write(self.__str__())
		file.close()


class MicroArrayPlatformStanza(SoftStanza):

	def __init__(self):
	
		keys = { 
			'^PLATFORM': KeyRequired,
			'!Platform_title': KeyRequired,
			'!Platform_distribution': KeyRequired,
			'!Platform_technology': KeyRequired,
			'!Platform_organism': KeyOnePlus,
			'!Platform_manufacturer': KeyRequired,
			'!Platform_manufacture_protocol': KeyOnePlus,
			'!Platform_catalog_number': KeyZeroPlus,
			'!Platform_web_link': KeyZeroPlus,
			'!Platform_support': KeyOptional,
			'!Platform_coating': KeyOptional,
			'!Platform_description': KeyZeroPlus,
			'!Platform_contributor': KeyZeroPlus,
			'!Platform_pubmed_id': KeyZeroPlus,
			'!Platform_geo_accession': KeyOptional,
			'!Platform_table_begin': KeyRequired,
			'!Platform_table_end': KeyRequired
		}
		
		SoftStanza.__init__(self, keys)
		
		
class MicroArraySampleStanza(SoftStanza):

	def __init__(self):
	
		keys = { 
			'^SAMPLE': KeyRequired,
			'!Sample_title': KeyRequired,
			'!Sample_supplementary_file': KeyOnePlus,
			'!Sample_table': KeyOptional,
			'!Sample_source_name_ch': KeyOnePlusNumbered,
			'!Sample_organism_ch': KeyOnePlusNumbered,
			'!Sample_characteristics_ch': KeyOnePlusNumbered,
			'!Sample_biomaterial_provider_ch': KeyZeroPlusNumbered,
			'!Sample_treatment_protocol_ch': KeyZeroPlusNumbered,
			'!Sample_growth_protocol_ch': KeyZeroPlusNumbered,
			'!Sample_molecule_ch': KeyOnePlusNumbered,
			'!Sample_extract_protocol_ch': KeyOnePlusNumbered,
			'!Sample_label_ch': KeyOnePlusNumbered,
			'!Sample_label_protocol_ch': KeyOnePlusNumbered,
			'!Sample_hyb_protocol': KeyOnePlus,
			'!Sample_scan_protocol': KeyOnePlus,
			'!Sample_data_processing': KeyOnePlus,
			'!Sample_description': KeyZeroPlus,
			'!Sample_platform_id': KeyRequired,
			'!Sample_geo_accession': KeyOptional,
			'!Sample_anchor': KeyRequired,
			'!Sample_type': KeyRequired,
			'!Sample_tag_count': KeyRequired,
			'!Sample_tag_length': KeyRequired,
			'!Sample_table_begin': KeyRequired,
			'!Sample_table_end': KeyRequired
		}
		
		SoftStanza.__init__(self, keys)		
		
		
class SeriesStanza(SoftStanza):
	
	def __init__(self):
	
		keys = { 
			'^SERIES': KeyRequired,
			'!Series_title': KeyRequired,
			'!Series_summary': KeyOnePlus,
			'!Series_overall_design': KeyRequired,
			'!Series_pubmed_id': KeyZeroPlus,
			'!Series_web_link': KeyZeroPlus,
			'!Series_contributor': KeyZeroPlus,
			'!Series_variable': KeyZeroPlusNumbered,
			'!Series_variable_description': KeyZeroPlusNumbered,
			'!Series_variable_sample_list': KeyZeroPlusNumbered,
			'!Series_repeats': KeyZeroPlusNumbered,
			'!Series_repeats_sample_list': KeyZeroPlusNumbered,
			'!Series_sample_id': KeyOnePlus,
			'!Series_geo_accession': KeyOptional,
			'!Series_gp_id': KeyOptional
		}
				
		SoftStanza.__init__(self, keys)

		
class HighThroughputSampleStanza(SoftStanza):

	def __init__(self):
	
		keys = {
			'^SAMPLE': KeyRequired,
			'!Sample_type': KeyRequired,
			'!Sample_title': KeyRequired,
			'!Sample_supplementary_file': KeyOnePlusNumbered,
			'!Sample_supplementary_file_checksum': KeyZeroPlusNumbered,
			'!Sample_supplementary_file_build': KeyZeroPlusNumbered,
			'!Sample_raw_file': KeyOnePlusNumbered,
			'!Sample_raw_file_type': KeyOnePlusNumbered,
			'!Sample_raw_file_checksum': KeyZeroPlusNumbered,
			'!Sample_source_name': KeyRequired,
			'!Sample_organism': KeyOnePlus,
			'!Sample_characteristics': KeyOnePlus,
			'!Sample_biomaterial_provider': KeyZeroPlus,
			'!Sample_treatment_protocol': KeyZeroPlus,
			'!Sample_growth_protocol': KeyZeroPlus,
			'!Sample_molecule': KeyRequired,
			'!Sample_extract_protocol': KeyOnePlus,
			'!Sample_library_strategy': KeyOnePlus,
			'!Sample_library_source': KeyOnePlus,
			'!Sample_library_selection': KeyOnePlus,
			'!Sample_instrument_model': KeyOnePlus,
			'!Sample_data_processing': KeyRequired,
			'!Sample_barcode': KeyOptional,
			'!Sample_description': KeyZeroPlus,
			'!Sample_geo_accession': KeyOptional,
			'!Sample_table_begin': KeyOptional,
			'!Sample_table': KeyOptional,
			'!Sample_table_end': KeyOptional
		}
		
		SoftStanza.__init__(self, keys)
		