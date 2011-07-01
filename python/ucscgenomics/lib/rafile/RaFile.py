import sys
import re
from ordereddict.OrderedDict import *

class RaFile(OrderedDict):
	"""
	Stores an Ra file in a set of entries, one for each stanza in the file.
	"""

	def __init__(self, filePath=''):
		OrderedDict.__init__(self)
		if filePath != '':
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
			raise IOError('File is not newline terminated')

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
			self.__readLine(line)

		return self.__readName(stanza[0])


	def __readName(self, line):
		"""
		Extracts the Stanza's name from the value of the first line of the
		stanza.
		"""

		if len(line.split(' ', 1)) != 2:
			raise ValueError()

		names = map(str.strip, line.split(' ', 1))
		self._name = names[1]
		return names

	def __readLine(self, line):
		"""
		Reads a single line from the stanza, extracting the key-value pair
		""" 

		if line.startswith('#') or line == '':
			OrderedDict.append(self, line)
			#self._OrderedDict__ordering.append(line)
		else:
			raKey = line.split(' ', 1)[0]
			raVal = ''
			if (len(line.split(' ', 1)) == 2):
				raVal = line.split(' ', 1)[1]
			#raKey, raVal = map(str, line.split(' ', 1))
			self[raKey] = raVal


	def iter(self):
		pass


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


	def __str__(self):
		str = ''
		for key in self:
			if key.startswith('#'):
				str += key + '\n'
			else:
				str += key + ' ' + self[key] + '\n'

		return str

