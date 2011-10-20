class OrderedDict(dict):
	"""
	A Dictionary ADT that preserves ordering of its keys through a parallel
	list.

	Inherits from the dict built-in python class, extending functionality 
	relevant to ordering.
	"""

	def __init__(self):
		self.__ordering = list()
		dict.__init__(self)


	def __setitem__(self, key, value):
		if key not in self:
			self.__ordering.append(key)
		dict.__setitem__(self, key, value)
		

	def __delitem__(self, key):
		dict.__delitem__(self, key)
		self.__ordering.remove(key)


	def append(self, item):
		self.__ordering.append(item)


	def remove(self, item):
		self.__ordering.remove(item)


	def __iter__(self):
		for item in self.__ordering:
			yield item


	def iterkeys(self):
		self.__iter__()


	def itervalues(self):
		for item in self.__ordering:
			yield self[item]


	def iteritems(self):
		for item in self.__ordering:
			yield item, self[item]


	def __str__(self):
		str = ''
		for item in self.iteritems():
			str += item.__str__() + '\n'
		return str

