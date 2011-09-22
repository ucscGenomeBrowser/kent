import sys
import re
from ucscgenomics.ordereddict import OrderedDict

class RaFile(OrderedDict):
	"""
	Stores a Ra file in a set of entries, one for each stanza in the file.
	
	To make a RaFile, it is usually easiest to just pass it's path:
		rafile = ra.RaFile('kent/src/hg/.../wgEncodeSomeRaFile.ra')
		
	The data is read in and organized as a collection of stanzas. Ra files
	store the stanza by it's name, so to access a specific stanza, say:
		somestanza = rafile['wgEncodeSomeStanzaName']
		
	Once you have a stanza, you may want specific data about that stanza. 
	Stanzas are, as ra files, organized as a collection of terms. Therefore
	to get the description of the stanza, we can say:
		somedescription = somestanza['description']
		
	You can also access a stanza's name from the stanza itself, since by the
	nature of ra files, the first key is it's name. Therefore for most ra 
	files the following holds true:
		somestanza.name = somestanza['metaObject'] = 'wgEncodeSomeStanzaName'
		
	Although the above is useful if you want one thing, it's usually more
	helpful to be able to loop and query on the stanza. To add a term named
	'foobar' to every stanza in a ra file:
		for stanza in rafile.values():
			stanza['foobar'] = 'some value'
	
	Note that I iterated over values. It can also be useful to iterate over
	a stanza's keys:
		for key in rafile.keys():
			print key
	
	Note that ra files are order preserving. Added entries are appended to the
	end of the file. This allows you to print out a ra file easily:
		print rafile
	
	Most of the time you don't want to do something with all stanzas though,
	instead you want to filter them. The included filter method allows you to
	specify two functions (or lambda expressions). The first is the 'where'
	predicate, which must take in one stanza, and return true/false depending
	on whether you want to take that stanza. The second is the 'select'
	predicate, which takes in the stanza, and returns some subset or superset
	of the stanza as a list. Using filter is preferable to for loops where
	there are no side effects, or to filter data before iterating over it as
	opposed to using if statements in the loop. To get all stanzas with one 
	experiment ID for instance, we would do something like this:
		stanzas = rafile.filter(lambda s: s['expId'] == '123', lambda s: s)
		
	Note that you don't have to ensure 'expId' is in the stanza, it will
	silently fail. Let's look at another example, say you want to find all
	stanza's with an geoSampleAccession that are also fastq's
		submittedfastqs = rafile.filter(
			lambda s: 'geoSampleAccession' in s and s['fileName'].endswith('.fastq'),
			lambda s: s)
			
	We don't always have to just return the stanza in the second parameter
	however. If we wanted to, for each stanza, return the file associated
	with that stanza, we could easily do that as well. This would return a
	simple list of the string filenames in a ra file:
		files = rafile.filter(lambda s: 1, lambda s: s['fileName'])
		
	Note that once again, we don't have to ensure 'fileName' exists. Also note
	that lambda s: 1 means always return true. Lambda expressions are always
	preferable to functions unless the expression would need to be reused
	multiple times. It is also best to reduce the set of stanzas as much as
	possible before operating over them.
	
	Filtering allows you to eliminate a lot of code. 
	"""

	def __init__(self, filePath=None):
		OrderedDict.__init__(self)
		if filePath != None:
			self.read(filePath) 

	def read(self, filePath):
		"""
		Reads an rafile stanza by stanza, and internalizes it.
		"""

		file = open(filePath, 'r')

		#entry = None
		stanza = list()
		keyValue = ''

		for line in file:
 
			line = line.strip()

			if len(stanza) == 0 and (line.startswith('#') or line == ''):
				OrderedDict.append(self, line)
				continue

			if line != '':
				stanza.append(line)
			elif len(stanza) > 0:
				if keyValue == '':
					keyValue, name, entry = self.readStanza(stanza)
				else:
					testKey, name, entry = self.readStanza(stanza)
					if entry != None and keyValue != testKey:
						raise KeyError('Inconsistent Key ' + testKey)
						
				if entry != None:
					if name in self:
						raise KeyError('Duplicate Key ' + name)
					self[name] = entry
				
				stanza = list()

		if len(stanza) > 0:
			if keyValue == '':
				keyValue, name, entry = self.readStanza(stanza)
			else:
				testKey, name, entry = self.readStanza(stanza)
				if entry != None and keyValue != testKey:
					raise KeyError('Inconsistent Key ' + testKey)
			
			if entry != None:
				if name in self:
					raise KeyError('Duplicate Key ' + name)
				self[name] = entry

		file.close()


	def readStanza(self, stanza):
		entry = RaStanza()
		val1, val2 = entry.readStanza(stanza)
		return val1, val2, entry


	def iter(self):
		pass


	def iterkeys(self):
		for item in self._OrderedDict__ordering:
			if not(item.startswith('#') or item == ''):
				yield item


	def itervalues(self):
		for item in self._OrderedDict__ordering:
			if not (item.startswith('#') or item == ''):
				yield self[item]


	def iteritems(self):
		for item in self._OrderedDict__ordering:
			if not (item.startswith('#') or item == ''):
				yield item, self[item]
			else:
				yield [item]

				
	def append(self, item):
		OrderedDict.append(self, item)
				
				
	def filter(self, where, select):
		"""
		select useful data from matching criteria
		
		where: the conditional function that must be met. Where takes one argument, the stanza and should return true or false
		select: the data to return. Takes in stanza, should return whatever to be added to the list for that stanza.
		
		For each stanza, if where(stanza) holds, it will add select(stanza) to the list of returned entities.
		Also forces silent failure of key errors, so you don't have to check that a value is or is not in the stanza.
		"""
		
		ret = list()
		for stanza in self.itervalues():
			try:
				if where(stanza):
					ret.append(select(stanza))
			except KeyError:
				continue
		return ret
				
				
	def __str__(self):
		str = ''
		for item in self.iteritems():
			if len(item) == 1:
				str += item[0].__str__() + '\n'
			else:
				str += item[1].__str__() + '\n'
		return str


class RaStanza(OrderedDict):
	"""
	Holds an individual entry in the RaFile.
	"""

	def __init__(self):
		self._name = ''
		OrderedDict.__init__(self)

	@property 
	def name(self):
		return self._name


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
			#if raKey in self:
				#raise KeyError(raKey + ' already exists')
			self[raKey] = raVal


	def iterkeys(self):
		for item in self._OrderedDict__ordering:
			if not (item.startswith('#') or item == ''):
				yield item


	def itervalues(self):
		for item in self._OrderedDict__ordering:
			if not (item.startswith('#') or item == ''):
				yield self[item]


	def iteritems(self):
		for item in self._OrderedDict__ordering:
			if not (item.startswith('#') or item == ''):
				yield item, self[item]


	def iter(self):
		iterkeys(self)
				
				
	def __str__(self):
		str = ''
		for key in self:
			if key.startswith('#'):
				str += key + '\n'
			else:
				str += key + ' ' + self[key] + '\n'

		return str

