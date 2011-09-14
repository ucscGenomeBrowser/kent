import os
from ucscgenomics.rafile.RaFile import *

def readMd5sums(filename):
	if os.path.isfile(filename):
		md5sums = dict()
		md5file = open(filename, 'r')
		for line in md5file:
			key, val = map(str.strip, line.split(' ', 1))
			md5sums[key] = val
		return md5sums
	else:
		return None

		
class DownloadsFile(object):

	@property 
	def name(self):
		"""The file's name"""
		return self._name
		
	@property 
	def path(self):
		"""The file's full pathname"""
		return self._path
		
	@property 
	def md5sum(self):
		"""The md5sum for this file, stored in the md5sum.txt file in the downloads directory"""
		return self._md5sum
		
	@property 
	def extension(self):
		"""The filetype"""
		return self._extension
		
	@property 
	def size(self):
		"""The size in bytes"""
		return self._size
		
	@property 
	def metaObject(self):
		"""The size in bytes"""
		return self._metaObj
	
	def __init__(self, path, md5, metaObj=None):
		if not os.path.isfile(path):
			raise FileError('invalid file path: %s' % path)
		self._name = path.rsplit('/', 1)[1]
		self._path = path
		self._size = os.stat(path).st_size
		self._md5sum = md5
		self._metaObj = metaObj
		
		self._extension = self._name
		self._extension.replace('.gz', '').replace('.tgz', '')
		if '.' in self._extension:
			self._extension = self._extension.rsplit('.')[1]
		else:
			self._extension = None
	
	
class CompositeTrack(object):

	@property 
	def database(self):
		"""The database for this composite, typically hg19 for humans"""
		return self._database
		
	@property 
	def name(self):
		"""The composite name"""
		return self._name
		
	@property 
	def downloadsDirectory(self):
		"""The location of files in downloads"""
		if not os.path.isdir(self._downloadsDirectory):
			raise KeyError(self._downloadsDirectory + ' does not exist')
		return self._downloadsDirectory
	
	@property 
	def files(self):
		"""A list of all files in the downloads directory of this composite"""
		try:
			return self._files
		except AttributeError:
			md5sums = readMd5sums(self._md5path)
			
			radict = dict()
			for stanza in self.alphaMetaDb:
				if 'fileName' in stanza:
					radict[stanza['fileName']] = stanza
			
			self._files = dict()
			for file in os.listdir(self.downloadsDirectory):
				if os.path.isfile(self.downloadsDirectory + file):
					
					stanza = None
					if file.name in radict:
						stanza = radict[file.name]
						
					if file in md5sums:
						self._files[file] = DownloadsFile(self.downloadsDirectory + file, md5sums[file], stanza)
					else:
						self._files[file] = DownloadsFile(self.downloadsDirectory + file, None, stanza)
		
			return self._files
		
	@property 
	def releases(self):
		"""A list of all files in the release directory of this composite"""
		try:
			return self._releaseFiles
		except AttributeError:
			self._releaseFiles = list()
			count = 1
			
			while os.path.exists(self.downloadsDirectory + 'release' + str(count)):
				releasepath = self.downloadsDirectory + 'release' + str(count) + '/'
				md5s = readMd5sums(releasepath + 'md5sum.txt')
				releasefiles = dict()
				
				for file in os.listdir(releasepath):
					if file != 'md5sum.txt' and md5s != None and file in md5s:
						releasefiles[file] = DownloadsFile(releasepath + file, md5s[file])
					else:
						releasefiles[file] = DownloadsFile(releasepath + file, None)
					
				#releasefiles.sort()
				self._releaseFiles.append(releasefiles)
				count = count + 1
				
			return self._releaseFiles
		
	@property 
	def alphaMetaDb(self):
		"""The Ra file in the metaDb for this composite"""
		try:
			return self._alphaMetaDb
		except AttributeError:
			if not os.path.isfile(self._alphaMdbPath):
				raise KeyError(self._alphaMdbPath + ' does not exist')
			self._alphaMetaDb = RaFile(self._alphaMdbPath)
			return self._alphaMetaDb
		
	@property 
	def betaMetaDb(self):
		"""The Ra file in the metaDb for this composite"""
		try:
			return self._betaMetaDb
		except AttributeError:
			if not os.path.isfile(self._betaMdbPath):
				raise KeyError(self._betaMdbPath + ' does not exist')
			self._betaMetaDb = RaFile(self._betaMdbPath)
			return self._betaMetaDb
		
	@property 
	def publicMetaDb(self):
		"""The Ra file in the metaDb for this composite"""
		try:
			return self._publicMetaDb
		except AttributeError:
			if not os.path.isfile(self._publicMdbPath):
				raise KeyError(self._publicMdbPath + ' does not exist')
			self._publicMetaDb = RaFile(self._publicMdbPath)
			return self._publicMetaDb
		
	@property 
	def trackDb(self):
		"""The Ra file in the trackDb for this composite"""
		try:
			return self._trackDb
		except AttributeError:
			self._trackDb = RaFile(self._trackDbPath)
			return self._trackDb
		
	@property 
	def trackPath(self):
		"""The track path for this composite"""
		return self._trackPath
		
	@property 
	def url(self):
		"""The url on our site for this composite"""
		return self._url
		
	@property 
	def organism(self):
		"""The url on our site for this composite"""
		return self._organism
		
	def __init__(self, database, composite, trackPath=None):
		
		if trackPath == None:
			self._trackPath = os.path.expanduser('~/kent/src/hg/makeDb/trackDb/')
		else:
			self._trackPath = trackPath
			
		organisms = {
			'hg19': 'human',
			'hg18': 'human',
			'mm9': 'mouse'
		}
		
		if database in organisms:
			self._organism = organisms[database]
		else:
			raise KeyError(database + ' is not a valid database')
		
		if not self._trackPath.endswith('/'):
			self._trackPath = self._trackPath + '/'
		
		self._trackDbPath = self._trackPath + self._organism + '/' + database + '/' + composite + '.ra'
		if not os.path.isfile(self._trackDbPath):
			raise KeyError(self._trackDbPath + ' does not exist')	
		
		self._alphaMdbPath = self._trackPath + self._organism + '/' + database + '/metaDb/alpha/' + composite + '.ra'
		self._betaMdbPath = self._trackPath + self._organism + '/' + database + '/metaDb/beta/' + composite + '.ra'	
		self._publicMdbPath = self._trackPath + self._organism + '/' + database + '/metaDb/public/' + composite + '.ra'
		self._downloadsDirectory = '/hive/groups/encode/dcc/analysis/ftp/pipeline/' + database + '/' + composite + '/'
		self._url = 'http://genome.ucsc.edu/cgi-bin/hgTrackUi?db=' + database + '&g=' + composite
		self._database = database
		self._name = composite		
		self._md5path = '/hive/groups/encode/dcc/analysis/ftp/pipeline/' + database + '/' + composite + '/md5sum.txt'
		