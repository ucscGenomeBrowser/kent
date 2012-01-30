import os, re, hashlib
from ucscgenomics import ra, mdb

organisms = {
    'hg19': 'human',
    'hg18': 'human',
    'mm9': 'mouse',
    'encodeTest': 'human'
}

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

        
def hashfile(filename, hasher=hashlib.md5(), blocksize=65536):
    afile = open(filename, 'rb')
    buf = afile.read(blocksize)
    while len(buf) > 0:
        hasher.update(buf)
        buf = afile.read(blocksize)
    return hasher.hexdigest()
        
class TrackFile(object):
    '''
    A file in the trackDb, which has useful information about iself.
    
    CompositeTrack (below) has multiple dictionaries of TrackFiles, one for
    the root downloads directory, and one for each release. The root directory
    will link itself to the CompositeTrack's alpha metadata.
    '''

    @property 
    def name(self):
        '''The file's name'''
        return self._name
        
    @property 
    def fullname(self):
        '''The file's full name including path'''
        return self._path + self._name
        
    @property 
    def path(self):
        '''The file's path'''
        return self._path
        
    @property 
    def md5sum(self):
        '''The md5sum for this file, stored in the md5sum.txt file in the downloads directory'''
        if self._md5sum == None:
            self._md5sum = hashfile(self.fullname)
        return self._md5sum
        
    @property 
    def extension(self):
        '''The filetype'''
        return self._extension
        
    @property 
    def size(self):
        '''The size in bytes'''
        return self._size
        
    @property 
    def metaObject(self):
        '''The size in bytes'''
        return self._metaObj
    
    def __init__(self, fullname, md5=None, metaObj=None):
        fullname = os.path.abspath(fullname)
        if not os.path.isfile(fullname):
            raise KeyError('invalid file: %s' % fullname)
        self._path, self._name = fullname.rsplit('/', 1)
        self._path = self._path + '/'
        self._fullname = fullname
        self._size = os.stat(fullname).st_size
        self._md5sum = md5
        self._metaObj = metaObj
        
        self._extension = self._name
        self._extension.replace('.gz', '').replace('.tgz', '')
        if '.' in self._extension:
            self._extension = self._extension.rsplit('.')[1]
        else:
            self._extension = None
    
    
class CompositeTrack(object):
    '''
    Stores an entire track, consisting mainly of its metadata and files.
    
    To make a CompositeTrack, you must specify database and name of the track:
        sometrack = CompositeTrack('hg19', 'wgEncodeCshlLongRnaSeq')
        
    You can also specify a trackDb path in the event that yours is different
    from the default, '~/kent/src/hg/makeDb/trackDb/':
        sometrack = CompositeTrack('hg19', 'wgEncode...', '/weird/path')
        
    It's important to know that the CompositeTrack does NOT load all of its
    information up front. Therefore, there's no performance hit for using a
    CompositeTrack instead of just specifying a RaFile. In fact, it's
    beneficial, since it adds another layer of abstraction to your code. You
    can access a composite's ra files:
        somemetadata = sometrack.alphaMetaDb
        
    For more information on what you can do with ra files, check the ra.py
    documentation.
    
    You can also access a track's files. This is one of the more useful parts
    of the composite track:
        for file in sometrack.files:
            print '%s %s' % (file.name, file.size)
            
    Each file is an instance of a TrackFile object, which is detailed in its
    own documentation above. There are also lists of these files for each
    release associated with the track:
        for file in sometrack.releases[0]:
            print file.name in sometrack.releases[1]
            
    Note that the files are indexed by their filename. This means that you can
    easily compare multiple releases as in the above example.
    '''

    @property 
    def database(self):
        '''The database for this composite, typically hg19 for humans'''
        return self._database
        
    @property 
    def name(self):
        '''The composite name'''
        return self._name
        
    @property 
    def downloadsDirectory(self):
        '''The location of files in downloads'''
        if not os.path.isdir(self._downloadsDirectory):
            raise KeyError(self._downloadsDirectory + ' does not exist')
        return self._downloadsDirectory
   
    @property 
    def httpDownloadsPath(self):
        '''The location of the downloadable files path in apache form'''
        if not os.path.isdir(self._httpDownloadsPath):
            raise KeyError(self._httpDownloadsPath + ' does not exist')
        return self._httpDownloadsPath
    
    @property 
    def files(self):
        '''A list of all files in the downloads directory of this composite'''
        try:
            return self._files
        except AttributeError:
            md5sums = readMd5sums(self._md5path)
            
            radict = dict()
            for stanza in self.alphaMetaDb.itervalues():
                if 'fileName' in stanza:
                    for file in stanza['fileName'].split(','):
                        radict[file] = stanza
            
            self._files = dict()
            for file in os.listdir(self.downloadsDirectory):
                if os.path.isfile(self.downloadsDirectory + file):
                
                    stanza = None
                    if file in radict:
                        stanza = radict[file]
                        
                    if file in md5sums:
                        self._files[file] = TrackFile(self.downloadsDirectory + file, md5sums[file], stanza)
                    else:
                        self._files[file] = TrackFile(self.downloadsDirectory + file, None, stanza)
        
            return self._files
            
    @property 
    def qaInitDir(self):
        qaDir = '/hive/groups/encode/encodeQa/' + self._database + '/' + self._name + '/'
        if os.path.exists(qaDir) and os.path.isdir(qaDir):
            pass
        else:
            os.makedirs(qaDir)
        self._qaDir = qaDir
        return qaDir
    @property 
    def qaInitDirTest(self):
        qaDir = '/hive/groups/encode/encodeQa/test/' + self._database + '/' + self._name + '/'
        if os.path.exists(qaDir) and os.path.isdir(qaDir):
            pass
        else:
            os.makedirs(qaDir)
        self._qaDir = qaDir
        return qaDir

    @property 
    def releases(self):
        '''A list of all files in the release directory of this composite'''
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
                    if file != 'md5sum.txt' and md5s != None and file in md5s and not os.path.isdir(releasepath + file):
                        releasefiles[file] = TrackFile(releasepath + file, md5s[file])
                    elif not os.path.isdir(releasepath + file):
                        releasefiles[file] = TrackFile(releasepath + file, None)
                    elif os.path.isdir(releasepath + file):
                        if not re.match('.*supplemental.*', releasepath + file):
                            continue
                        for innerfile in os.listdir(releasepath + file):
                            pathfile = file + "/" + innerfile 
                            releasefiles[pathfile] = TrackFile(releasepath + pathfile, None)
        #releasefiles.sort()
                self._releaseFiles.append(releasefiles)
                count = count + 1
                
            return self._releaseFiles
        
    @property 
    def alphaMetaDb(self):
        '''The Ra file in the metaDb for this composite'''
        try:
            return self._alphaMetaDb
        except AttributeError:
            if not os.path.isfile(self._alphaMdbPath):
                raise KeyError(self._alphaMdbPath + ' does not exist')
            self._alphaMetaDb = mdb.MdbFile(self._alphaMdbPath)
            return self._alphaMetaDb
        
    @property 
    def betaMetaDb(self):
        '''The Ra file in the metaDb for this composite'''
        try:
            return self._betaMetaDb
        except AttributeError:
            if not os.path.isfile(self._betaMdbPath):
                raise KeyError(self._betaMdbPath + ' does not exist')
            self._betaMetaDb = mdb.MdbFile(self._betaMdbPath)
            return self._betaMetaDb
        
    @property 
    def publicMetaDb(self):
        '''The Ra file in the metaDb for this composite'''
        try:
            return self._publicMetaDb
        except AttributeError:
            if not os.path.isfile(self._publicMdbPath):
                raise KeyError(self._publicMdbPath + ' does not exist')
            self._publicMetaDb = mdb.MdbFile(self._publicMdbPath)
            return self._publicMetaDb
        
    @property 
    def trackDb(self):
        '''The Ra file in the trackDb for this composite'''
        try:
            return self._trackDb
        except AttributeError:
            self._trackDb = ra.RaFile(self._trackDbPath)
            return self._trackDb
        
    @property 
    def trackPath(self):
        '''The track path for this composite'''
        return self._trackPath
        
    @property 
    def url(self):
        '''The url on our site for this composite'''
        return self._url
        
    @property 
    def organism(self):
        '''The url on our site for this composite'''
        return self._organism

    @property 
    def currentTrackDb(self):
        trackDb = self._trackDbDir + "trackDb.wgEncode.ra"
        f = open(trackDb, "r")
        lines = f.readlines()
        p = re.compile(".*(%s\S+) ?(\S+)" % self._name)
        for i in lines:
            if re.match("^\s*#.*", i):
                continue
            m = p.match(i)
            if m and re.search('alpha', m.group(2)):
                tdbpath = "%s%s" % (self._trackDbDir, m.group(1))
                return tdbpath
        return None


    def __init__(self, database, compositeName, trackPath=None, mdbCompositeName=None):
        
        if mdbCompositeName == None:
            mdbCompositeName = compositeName
        
        if trackPath == None:
            self._trackPath = os.path.expanduser('~/kent/src/hg/makeDb/trackDb/')
        else:
            self._trackPath = trackPath
            if not self._trackPath.endswith('/'):
                self._trackPath = self._trackPath + '/'
            
        if database in organisms:
            self._organism = organisms[database]
        else:
            raise KeyError(database + ' is not a valid database')
        
        #self._trackDbPath = self._trackPath + self._organism + '/' + database + '/' + compositeName + '.ra'
        self._trackDbDir = self._trackPath + self._organism + '/' + database + '/'
  
        self._alphaMdbPath = self._trackPath + self._organism + '/' + database + '/metaDb/alpha/' + mdbCompositeName + '.ra'
        self._betaMdbPath = self._trackPath + self._organism + '/' + database + '/metaDb/beta/' + mdbCompositeName + '.ra'    
        self._publicMdbPath = self._trackPath + self._organism + '/' + database + '/metaDb/public/' + mdbCompositeName + '.ra'
        self._alphaMdbDir = self._trackPath + self._organism + '/' + database + '/metaDb/alpha/'
        self._betaMdbDir = self._trackPath + self._organism + '/' + database + '/metaDb/beta/'
        self._publicMdbDir = self._trackPath + self._organism + '/' + database + '/metaDb/public/'
        self._downloadsDirectory = '/hive/groups/encode/dcc/analysis/ftp/pipeline/' + database + '/' + compositeName + '/'
        self._httpDownloadsPath = '/usr/local/apache/htdocs-hgdownload/goldenPath/' + database + '/encodeDCC/' + compositeName + '/'
        self._rrHttpDir = '/usr/local/apache/htdocs/goldenPath/' + database + '/encodeDCC/' + compositeName + '/'
        self._notesDirectory = os.path.expanduser("~/kent/src/hg/makeDb/doc/encodeDcc%s" % database.capitalize()) + '/'
        self._url = 'http://genome.ucsc.edu/cgi-bin/hgTrackUi?db=' + database + '&g=' + compositeName
        self._database = database
        self._name = compositeName        
        self._md5path = '/hive/groups/encode/dcc/analysis/ftp/pipeline/' + database + '/' + compositeName + '/md5sum.txt'
        self._trackDbPath = self.currentTrackDb
        if self._trackDbPath == None:
            self._trackDbPath = self._trackPath + self._organism + '/' + database + '/' + compositeName + '.ra' 
        if not os.path.isfile(self._trackDbPath):
            raise KeyError(self._trackDbPath + ' does not exist')
        

class TrackCollection(dict):
    '''
    A collection that stores all the tracks for a given database, indexed by
    its metaDb name.
    '''
    
    @property 
    def database(self):
        return self._database
    
    @property 
    def organism(self):
        return self._organism  
        
    def __init__(self, database, trackPath=None):
        dict.__init__(self)
    
        self._database = database
        
        if database in organisms:
            self._organism = organisms[database]
        else:
            raise KeyError(database + ' is not a valid database')
    
        if trackPath == None:
            self._trackPath = os.path.expanduser('~/kent/src/hg/makeDb/trackDb/')
        else:
            self._trackPath = trackPath
            if not self._trackPath.endswith('/'):
                self._trackPath = self._trackPath + '/'
    
        metaDb = self._trackPath + self._organism + '/' + self._database + '/metaDb/alpha/'
        
        for file in os.listdir(metaDb):
            if os.path.isfile(metaDb + file) and file.endswith('.ra'):
                trackname = file.replace('.ra', '') 
                if os.path.isfile(self._trackPath + self._organism + '/' + self._database + '/' + file):
                    self[trackname] = CompositeTrack(self._database, trackname, self._trackPath)
                
                