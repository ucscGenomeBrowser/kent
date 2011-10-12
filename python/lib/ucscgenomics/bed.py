
class BedLine(object):
	
	@property 
	def chromosome(self):
		"""The chromosome"""
		return self._chromosome
		
	@property 
	def start(self):
		"""The start"""
		return self._start
		
	@property 
	def end(self):
		"""The end"""
		return self._end
		
	@property 
	def name(self):
		"""The file's name"""
		return self._name
	
	def __init__(self, chromosome, start, end, name=None):
		self._chromosome = chromosome
		self._start = start
		self._end = end
		self._name = name

class BedFile(object):

	@property 
	def lines(self):
		"""asdf"""
		return self._nodes

	def __init__(self, filename):

		self._nodes = list()
	
		for line in open(filename):
			
			line = line.strip()
			if line == '':
				continue
			
			vals = line.split('\t')
			
			self._nodes.append(BedLine(vals[0], int(vals[1]), int(vals[2]), vals[3]))
