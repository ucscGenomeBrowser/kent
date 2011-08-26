import sys
import re
from ucscgenomics.ordereddict.OrderedDict import *

class RaFile(OrderedDict):
	"""
	Stores an Ra file in a set of entries, one for each stanza in the file.
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

