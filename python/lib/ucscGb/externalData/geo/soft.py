import sys, re, os
from ucscGb.gbData.ordereddict import OrderedDict
from ucscGb.encode import encodeUtils, track
from ucscGb.externalData.geo import submission

cvDetails = {
    'cell':    [ 'organism', 'description', 'karyotype', 'lineage', 'sex' ],
    'antibody': [ 'antibodyDescription', 'targetDescription', 'vendorName', 'vendorId' ]
}

#if the term appears in the mdb and must overriding the value in the cv
cvOverride = [ 'sex' ]

#talk to Venkat lol
cvPretend = { 'antibody Input': 'control' }

#if its not in cvDetails, which things should we check by default
cvDefaults = [ 'description' ]

mdbWhitelist = [
    'age',
    'bioRep',
    'control',
    'controlId',
    'fragSize',
    'labExpId',
    'labVersion',
    'mapAlgorithm',
    'obtainedBy',
    'phase',
    'readType',
    'region',
    'replicate',
    'restrictionEnzyme',
    'run',
    'softwareVersion',
    'spikeInPool',
    'strain'
]
    
def isRawFile(file):
    return (file.extension == 'fastq' or file.extension == 'csfasta' or file.extension == 'csqual')
    
def isSupplimentaryFile(file):
    return (not isRawFile(file)) and file.extension != 'fasta'
    
def sampleTitle(stanza, expVars, warn=False, rep=False):
    concat = stanza[expVars[0]].replace('-m', '')
    for expVar in expVars[1:len(expVars)]:
        if expVar in stanza and stanza[expVar] != 'None':
            concat += '_' + stanza[expVar]
        elif warn:
            print 'warning: %s is None or not in %s' % (expVar, stanza.name)
    if rep:
        concat += 'Rep' + stanza['replicate']
    return concat
    
def linkName(file, track):
    return '%s_%s' % (track.database, file.name)
    
def createMappings(metadb, all=False, rep=False):
    expIds = dict()
    geoMapping = dict()
    expVars = None
    series = None
    datatype = None
    
    for stanza in metadb.itervalues():
        
        if 'objType' in stanza and stanza['objType'] == 'composite':
            series = stanza
            expVars = stanza['expVars'].split(',')
            continue

        if 'expId' not in stanza:
            print stanza.name + ': no expId'
            continue

        if 'objStatus' in stanza:
            print stanza.name + ': skipping because ' + stanza['objStatus']
            continue
            
        if 'geoSampleAccession' not in stanza or all:
            if stanza['fileName'].endswith('bam') or stanza['fileName'].endswith('bai') and 'Splices' not in stanza['fileName']:
                continue
            # if this hasn't been submitted to GEO yet, we'll add it to the submission list
            if rep:
                if stanza['expId'] + '_' + stanza['replicate'] not in expIds:
                    expIds[stanza['expId'] + '_' + stanza['replicate']] = list()
                expIds[stanza['expId'] + '_' + stanza['replicate']].append(stanza)
            else:
                if stanza['expId'] not in expIds:
                    expIds[stanza['expId']] = list()
                expIds[stanza['expId']].append(stanza)
        
        else:
            # otherwise we keep track of the geo number for partially submitted samples
            if rep:
                varname = stanza['expId'] + '_' + stanza['replicate']
                if varname not in geoMapping:
                    geoMapping[varname] = stanza['geoSampleAccession']
                    print varname + ': ' + stanza['geoSampleAccession']
                elif geoMapping[varname] != 'Inconsistent' and geoMapping[varname] != stanza['geoSampleAccession']:
                    geoMapping[varname] = 'Inconsistent'
                    print stanza.name + ': inconsistent geo mapping'
            else:
                if stanza['expId'] not in geoMapping:
                    geoMapping[stanza['expId']] = stanza['geoSampleAccession']
                elif geoMapping[stanza['expId']] != 'Inconsistent' and geoMapping[stanza['expId']] != stanza['geoSampleAccession']:
                    geoMapping[stanza['expId']] = 'Inconsistent'
                    print stanza.name + ': inconsistent geo mapping'
        
        if datatype == None and 'dataType' in stanza:
            datatype = stanza['dataType']
        elif datatype != None and 'dataType' in stanza and datatype != stanza['dataType']:
            raise KeyError(stanza.name + ': inconsistent data type') 

    try:
        dt = datatype
        datatype = encodeUtils.dataTypes[dt]
        datatype.name = dt
    except KeyError:
        raise KeyError(datatype)
    
    return expIds, expVars, geoMapping, series, datatype
    
    
def createSeries(softfile, compositeTrack, expIds, expVars, geoMapping, series, datatype, replace, audit, argseries, all=False, rep=False):
    
    if 'geoSeriesAccession' in series and not all:
        print 'Existing series ' + series['composite'] + ' using geoSeriesAccession ' + series['geoSeriesAccession']
        return
        
    print 'Writing series ' + series['composite']
    
    seriesStanza = SeriesStanza(softfile)
    seriesStanza['^SERIES'] = series['composite']
    seriesStanza['!Series_title'] = compositeTrack.trackDb[compositeTrack.name]['longLabel'] #STILL INCORRECT
    
    if '!Series_summary' in replace:
        seriesStanza['!Series_summary'] = replace['!Series_summary']
    else:
        print 'warning: no series summary found. Please include in replace file.'
        seriesStanza['!Series_summary'] = '[REPLACE]'
        if audit:
            print seriesStanza.name + ': no summary'
        
    if '!Series_overall_design' in replace:
        seriesStanza['!Series_overall_design'] = replace['!Series_overall_design']
    else:
        print 'no series overall design found. Please include in replace file.'
        seriesStanza['!Series_overall_design'] = '[REPLACE]'
        if audit:
            print seriesStanza.name + ': no overall design'
            
    seriesStanza['!Series_web_link'] = [ compositeTrack.url, 'http://www.ncbi.nlm.nih.gov/geo/info/ENCODE.html' ]
    
    if '!Series_contributor' in replace:
        seriesStanza['!Series_contributor'] = replace['!Series_contributor']
    else:
        seriesStanza['!Series_contributor'] = '[REPLACE]'
        if audit:
            print seriesStanza.name + ': no contributor'
        
    seriesStanza['!Series_gp_id'] = encodeUtils.gpIds[compositeTrack.organism + ' ' + datatype.source]
    
    # could use !Series_variable_* and !Series_repeats_*
    
    if not argseries:
        seriesStanza['!Series_sample_id'] = list()
        
        for idNum in expIds.iterkeys():
            if idNum in geoMapping and geoMapping[idNum] != 'Inconsistent':
                seriesStanza['!Series_sample_id'].append(geoMapping[idNum])
            else:
                seriesStanza['!Series_sample_id'].append(sampleTitle(expIds[idNum][0], expVars, False, rep))
        
    softfile[series['composite']] = seriesStanza
    
def createHighThroughputSoftFile(compositeTrack, cv, expIds, expVars, geoMapping, series, datatype, replace, audit, tarpath, argseries, all=False, rep=False):
    
    print 'Creating HighThroughput soft file'

    softfile = HighThroughputSoftFile()
    fileList = list()
    
    createSeries(softfile, compositeTrack, expIds, expVars, geoMapping, series, datatype, replace, audit, argseries, all)
    
    if argseries:
        return softfile, fileList
    
    for idNum in expIds.iterkeys():
        
        expId = expIds[idNum]
        firstStanza = expId[0]
        if not all: print 'Writing sample ' + firstStanza['metaObject'] + ' (' + idNum + ')'
        sample = HighThroughputSampleStanza(softfile)

        sample['^SAMPLE'] = sampleTitle(firstStanza, expVars, 1, rep)
        sample['!Sample_type'] = 'SRA'
        sample['!Sample_title'] = sample['^SAMPLE']
        
        if 'geoSeriesAccession' in series:
            sample['!Sample_series_id'] = series['geoSeriesAccession']
            
        count = 1
        
        #figure out if the instrument model is consistent across the entire sample
        instrumentModel = None
        for stanza in expId:    
            if 'seqPlatform' in stanza:
                if instrumentModel == None:
                    instrumentModel = submission.instrumentModels[stanza['seqPlatform']]
                else:
                    if instrumentModel != submission.instrumentModels[stanza['seqPlatform']]:
                        instrumentModel = None
                        if audit:
                            print 'expId' + str(expId) + ': inconsistent instrument model'
                        break
        
        for stanza in expId:
        
            for fname in stanza['fileName'].split(','):
              
                file = compositeTrack.files[fname]
                filelist = list()
                
                if file.extension == 'fasta':
                    print 'WARNING: fastas detected!!!'
                
                if isRawFile(file):
                
                    if all:
                        continue
                
                    if file.name.endswith('.tgz') or file.name.endswith('.tar.gz'):
                    
                        if tarpath == None:
                            raise IOError('this track contains tarred fastqs. Please specify a path through the -z option')
                        dirname = tarpath + file.name.split('.')[0] + '/'
                        if os.path.exists(dirname):
                            print dirname + ' already exists, so not unzipping'
                        else:
                            print 'creating ' + dirname + '...'
                            os.mkdir(dirname)
                            os.system('tar -xf %s -C %s' % (file.path + file.name, dirname))
                        
                        for root, dirnames, filenames in os.walk(dirname):
                            for filename in filenames:
                                if 'reject' in filename or 'md5sum' in filename:
                                    continue
                                if filename.endswith('.fastq') or filename.endswith('.txt'):
                                    print 'gzipping ' + filename
                                    os.system('gzip %s' % (root + '/' + filename))
                        
                        for root, dirnames, filenames in os.walk(dirname):
                        
                            rootmd5s = None
                            if os.path.isfile(root + '/md5sum.txt'):
                                rootmd5s = encodeUtils.readMd5sums(root + '/md5sum.txt')
                            
                            for filename in filenames:
                                if 'reject' in filename or 'md5sum' in filename:
                                    continue
                                
                                print root + '/' + filename
                                
                                if rootmd5s != None and filename in rootmd5s:
                                    newmd5 = rootmd5s[filename]
                                else:
                                    newmd5 = encodeUtils.hashFile(root + '/' + filename)
                                    encodeUtils.writeMd5sums(root + '/md5sum.txt', filename, newmd5)
                                newfile = track.TrackFile(root + '/' + filename, newmd5)
                                
                                filelist.append(newfile)

                    else:
                        filelist.append(file)
                        
                    for f in filelist:
                        
                        sample['!Sample_raw_file_' + str(count)] = linkName(f, compositeTrack)
                        if f.extension == 'txt':
                            sample['!Sample_raw_file_type_' + str(count)] = 'fastq'
                        elif f.extension == 'csfasta':
                            sample['!Sample_raw_file_type_' + str(count)] = 'SOLiD_native_csfasta'
                        elif f.extension == 'csqual':
                            sample['!Sample_raw_file_type_' + str(count)] = 'SOLiD_native_qual'
                        else:
                            sample['!Sample_raw_file_type_' + str(count)] = f.extension
                        
                        sample['!Sample_raw_file_checksum_' + str(count)] = f.md5sum

                        if instrumentModel == None and 'seqPlatform' in stanza:
                            sample['!Sample_raw_file_instrument_model_' + str(count)] = submission.instrumentModels[stanza['seqPlatform']]
                            
                        fileList.append(f)    
                        count = count + 1
            
        count = 1

        pooledStanza = dict()
        
        for stanza in expId:
        
            for fname in stanza['fileName'].split(','):
                file = compositeTrack.files[fname]
        
                if isSupplimentaryFile(file):
                    sample['!Sample_supplementary_file_' + str(count)] = linkName(file, compositeTrack)
                    
                    if not all:
                        if file.md5sum != None:
                            sample['!Sample_supplementary_file_checksum_' + str(count)] = file.md5sum
                    
                    sample['!Sample_supplementary_file_build_' + str(count)] = compositeTrack.database
                    
                    if instrumentModel == None and 'seqPlatform' in stanza:
                        sample['!Sample_supplementary_file_instrument_model_' + str(count)] = submission.instrumentModels[stanza['seqPlatform']]
                    
                    fileList.append(file)
                    count = count + 1
                    
            if 'objStatus' in stanza:
                continue
            for k in stanza.iterkeys():
                if k not in pooledStanza:
                    pooledStanza[k] = set()
                pooledStanza[k].add(stanza[k])
        for k in pooledStanza.iterkeys():
            pooledStanza[k] = ','.join(pooledStanza[k])
            
        
        if (idNum in geoMapping and geoMapping[idNum] != 'Inconsistent'):
            sample['!Sample_geo_accession'] = geoMapping[idNum]
        else:
        
            if all and 'geoSampleAccession' in pooledStanza:
                sample['!Sample_geo_accession'] = pooledStanza['geoSampleAccession']
        
            sample['!Sample_source_name'] = pooledStanza['cell']
            sample['!Sample_organism'] = compositeTrack.organism
            
            sample['!Sample_characteristics'] = list()
            allVars = expVars + mdbWhitelist
            
            for var in allVars:
                if var in pooledStanza:
                    foobar = var
                    sample['!Sample_characteristics'].append(var + ': ' + pooledStanza[var])
                    for pretend in cvPretend.iterkeys():
                        if var + ' ' + pooledStanza[var] == pretend:
                            foobar = cvPretend[pretend]
                    if foobar in cvDetails:
                        for cvVar in cvDetails[foobar]:
                            if cvVar in cvOverride and cvVar in pooledStanza:
                                sample['!Sample_characteristics'].append(var + ' ' + cvVar + ': ' + pooledStanza[cvVar])
                            elif cvVar in cv[pooledStanza[var]]:
                                sample['!Sample_characteristics'].append(var + ' ' + cvVar + ': ' + cv[pooledStanza[var]][cvVar])
                    else:
                        for cvVar in cvDefaults:
                            if pooledStanza[var] in cv and cvVar in cv[pooledStanza[var]]:
                                sample['!Sample_characteristics'].append(var + ' ' +  cvVar + ': ' + cv[pooledStanza[var]][cvVar])
                    
            sample['!Sample_biomaterial_provider'] = cv[pooledStanza['cell']]['vendorName']
            
            if 'treatment' in pooledStanza:
                sample['!Sample_treatment_protocol'] = pooledStanza['treatment']
            
            if 'protocol' in cv[pooledStanza['cell']]:
                for protocol in cv[pooledStanza['cell']]['protocol'].split(' '):
                        if protocol == 'missing':
                            continue
                        if ':' not in protocol:
                            raise KeyError(protocol + ' is not valid')
                        key, val = protocol.split(':')
                        if key == 'ENCODE' or key == cv[pooledStanza['lab']]['labPi']:
                            sample['!Sample_growth_protocol'] = val
            
            if datatype.molecule == 'RNA':
                if 'rnaExtract' not in pooledStanza:
                    sample['!Sample_molecule'] = 'total RNA'
                elif pooledStanza['rnaExtract'] in submission.rnaExtractMapping:
                    sample['!Sample_molecule'] = submission.rnaExtractMapping[pooledStanza['rnaExtract']]
                elif pooledStanza['localization'] in submission.localizationMapping:
                    sample['!Sample_molecule'] = submission.localizationMapping[pooledStanza['localization']]
                    
            else:
                sample['!Sample_molecule'] = datatype.molecule
                
            if '!Sample_instrument_model' in replace and replace['!Sample_instrument_model'][0] == 'Unknown':
                sample['!Sample_extract_protocol'] = 'Instrument model unknown. ("%s" specified by default). For more information, see %s' % (submission.instrumentModels[replace['!Sample_instrument_model'][0]], compositeTrack.url)
            else:
                sample['!Sample_extract_protocol'] = compositeTrack.url
            sample['!Sample_library_strategy'] = datatype.strategy
            sample['!Sample_library_source'] = datatype.source
            sample['!Sample_library_selection'] = datatype.selection
            
            # if the instrumentModel is consistent, just use that
            # otherwise take the first seqPlatform value from metadata
            # if that still fails, check the replacement file
            # finally just make it say [REPLACE]
            if instrumentModel != None:
                sample['!Sample_instrument_model'] = instrumentModel
            else:
                for stanza in expId:    
                    if 'seqPlatform' in stanza:
                        sample['!Sample_instrument_model'] = submission.instrumentModels[stanza['seqPlatform']]
                        break
                if '!Sample_instrument_model' not in sample:
                    if '!Sample_instrument_model' in replace:
                        sample['!Sample_instrument_model'] = submission.instrumentModels[replace['!Sample_instrument_model'][0]]
                if '!Sample_instrument_model' not in sample:
                    sample['!Sample_instrument_model'] = '[REPLACE]'
                    if audit:
                        print stanza.name + ': no instrument'
                    
            sample['!Sample_data_processing'] = compositeTrack.url
            
        softfile[sample['^SAMPLE']] = sample
        
    return softfile, fileList
        
        
def createMicroArraySoftFile(compositeTrack, cv, expIds, expVars, geoMapping, series, datatype, replace, audit, tarpath, argseries, all=False):
    
    print 'Creating HighThroughput soft file'

    softfile = HighThroughputSoftFile()
    fileList = list()
    
    createSeries(softfile, compositeTrack, expIds, expVars, geoMapping, series, datatype, replace, audit, argseries, all)
    
    if argseries:
        return softfile, fileList
    
    for idNum in expIds.iterkeys():
        
        # sample['!Sample_table'] = KeyOptional # CEL file
        # sample['!Sample_source_name_ch_1'] = '[REPLACE]' #KeyOnePlusNumbered
        # sample['!Sample_organism_ch_1'] = '[REPLACE]' #KeyOnePlusNumbered
        # sample['!Sample_characteristics_ch_1'] = '[REPLACE]' #KeyOnePlusNumbered
        # sample['!Sample_biomaterial_provider_ch'] = KeyZeroPlusNumbered
        # sample['!Sample_treatment_protocol_ch'] = KeyZeroPlusNumbered
        # sample['!Sample_growth_protocol_ch'] = KeyZeroPlusNumbered
        # sample['!Sample_molecule_ch_1'] = '[REPLACE]' #KeyOnePlusNumbered
        # sample['!Sample_extract_protocol_ch_1'] = '[REPLACE]' #KeyOnePlusNumbered
        # sample['!Sample_label_ch_1'] = '[REPLACE]' #KeyOnePlusNumbered
        # sample['!Sample_label_protocol_ch_1'] = '[REPLACE]' #KeyOnePlusNumbered
        # sample['!Sample_hyb_protocol'] = '[REPLACE]' #KeyOnePlus
        # sample['!Sample_scan_protocol'] = '[REPLACE]' #KeyOnePlus
        # sample['!Sample_data_processing'] = '[REPLACE]' #KeyOnePlus
        # sample['!Sample_description'] = '[REPLACE]' #KeyZeroPlus
        # sample['!Sample_platform_id'] = '[REPLACE]'
        # sample['!Sample_table_begin'] = ''
        # sample['!Sample_table_end'] = ''
        
        expId = expIds[idNum]
        firstStanza = expId[0]
        print 'Writing sample ' + firstStanza['metaObject'] + ' (' + idNum + ')'
        sample = HighThroughputSampleStanza(softfile)

        sample['^SAMPLE'] = sampleTitle(firstStanza, expVars, 1)
        sample['!Sample_title'] = sample['^SAMPLE']
        
        if 'geoSeriesAccession' in series:
            sample['!Sample_series_id'] = series['geoSeriesAccession']
        
        for stanza in expId:
            for fname in stanza['fileName'].split(','):
                file = compositeTrack.files[fname]
                if isRawFile(file):
                    print 'ERROR: RAW FILES IN MICROARRAY SUBMISSION DETECTED'
            
        count = 1
            
        for stanza in expId:
            for fname in stanza['fileName'].split(','):
                file = compositeTrack.files[fname]
        
                if isSupplimentaryFile(file):
                    sample['!Sample_supplementary_file_' + str(count)] = linkName(file, compositeTrack)
                    
                    if file.md5sum != None:
                        sample['!Sample_supplementary_file_checksum_' + str(count)] = file.md5sum
                        
                    fileList.append(file)
                    count = count + 1
                    
        print idNum
        if idNum in geoMapping:
            print geoMapping[idNum]
        
        if idNum in geoMapping and geoMapping[idNum] != 'Inconsistent':
            sample['!Sample_geo_accession'] = geoMapping[idNum]
        else:
        
            sample['!Sample_source_name_ch1'] = firstStanza['cell']
            sample['!Sample_organism_ch1'] = compositeTrack.organism
            
            sample['!Sample_characteristics_ch1'] = list()
            allVars = expVars + mdbWhitelist
            
            for var in allVars:
                if var in firstStanza:
                    foobar = var
                    sample['!Sample_characteristics_ch1'].append(var + ': ' + firstStanza[var])
                    for pretend in cvPretend.iterkeys():
                        if var + ' ' + firstStanza[var] == pretend:
                            foobar = cvPretend[pretend]
                    if foobar in cvDetails:
                        for cvVar in cvDetails[foobar]:
                            if cvVar in cvOverride and cvVar in firstStanza:
                                sample['!Sample_characteristics_ch1'].append(var + ' ' + cvVar + ': ' + firstStanza[cvVar])
                            elif cvVar in cv[firstStanza[var]]:
                                sample['!Sample_characteristics_ch1'].append(var + ' ' + cvVar + ': ' + cv[firstStanza[var]][cvVar])
                    else:
                        for cvVar in cvDefaults:
                            if firstStanza[var] in cv and cvVar in cv[firstStanza[var]]:
                                sample['!Sample_characteristics_ch1'].append(var + ' ' +  cvVar + ': ' + cv[firstStanza[var]][cvVar])
                    
            sample['!Sample_biomaterial_provider_ch1'] = cv[firstStanza['cell']]['vendorName']
            
            if 'treatment' in firstStanza:
                sample['!Sample_treatment_protocol_ch1'] = firstStanza['treatment']
            elif '!Sample_treatment_protocol_ch1' in replace:
                sample['!Sample_treatment_protocol_ch1'] = replace['!Sample_treatment_protocol_ch1']
            else:
                sample['!Sample_treatment_protocol_ch1'] = 'Unknown'
            
            if 'protocol' in cv[firstStanza['cell']]:
                for protocol in cv[firstStanza['cell']]['protocol'].split(' '):
                        if protocol == 'missing':
                            continue
                        if ':' not in protocol:
                            raise KeyError(protocol + ' is not valid')
                        key, val = protocol.split(':')
                        if key == 'ENCODE' or key == cv[firstStanza['lab']]['labPi']:
                            sample['!Sample_growth_protocol_ch1'] = val
            
            if datatype.molecule == 'RNA':
                if 'rnaExtract' not in firstStanza:
                    sample['!Sample_molecule_ch1'] = 'total RNA'
                elif firstStanza['rnaExtract'] in submission.rnaExtractMapping:
                    sample['!Sample_molecule_ch1'] = submission.rnaExtractMapping[firstStanza['rnaExtract']]
                elif firstStanza['localization'] in submission.localizationMapping:
                    sample['!Sample_molecule_ch1'] = submission.localizationMapping[firstStanza['localization']]
                    
            else:
                sample['!Sample_molecule_ch1'] = datatype.molecule

            sample['!Sample_extract_protocol_ch1'] = compositeTrack.url
            
            sample['!Sample_label_ch_1'] = '[REPLACE]'
            if '!Sample_label_ch_1' in replace:
                sample['!Sample_label_ch_1'] = replace['!Sample_label_ch_1'] #KeyOnePlusNumbered
                
            sample['!Sample_label_protocol_ch_1'] = compositeTrack.url #KeyOnePlusNumbered
            
            # are all these just links to the trackDB too?
            sample['!Sample_hyb_protocol'] = compositeTrack.url #KeyOnePlus
            sample['!Sample_scan_protocol'] = compositeTrack.url #KeyOnePlus
            sample['!Sample_data_processing'] = compositeTrack.url #KeyOnePlus
            sample['!Sample_description'] = compositeTrack.url #KeyZeroPlus
            
        softfile[firstStanza['metaObject']] = sample
        
    return softfile, fileList
    
class SoftFile(OrderedDict):

    """
    Stores an Ra file in a set of entries, one for each stanza in the file.
    """

    def __init__(self, filePath=''):
        OrderedDict.__init__(self)
        if filePath != '':
            self.read(filePath) 
    
    def read(self, filePath):
        file = open(filePath, 'r')
        self.readStream(file)
        file.close()
    
    def readStream(self, stream):
        """
        Reads an SoftFile stanza by stanza, and internalizes it.
        """

        stanza = list()

        for line in stream:
 
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

        #stream.close()
        
        name, entry = self.readStanza(stanza)
        #print 'hit: ' + name
        if entry != None:
            if name in self:
                raise KeyError('Duplicate Key ' + name)
            self[name] = entry


    def readStanza(self, stanza):

        if stanza[0].startswith('^SAMPLE'):
            entry = HighThroughputSampleStanza(self) #WILL HAVE TO CHANGE
        elif stanza[0].startswith('^SERIES'):
            entry = SeriesStanza(self)
        elif stanza[0].startswith('^PLATFORM'):
            entry = PlatformStanza(self)
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
        
    def diff(self, other):
        result = dict()
        for key in self.iterkeys():
            if key not in other:
                result[key] = list()
                result[key].append(key)
                result[key].append(None)
            elif self[key] != other[key]:
                result[key] = list()
                result[key].append(self[key].diff(other[key]))
        for key in other.iterkeys():
            if key not in self:
                result[key] = list()
                result[key].append(None)
                result[key].append(key)
        return result
            
class HighThroughputSoftFile(SoftFile):

    def __init__(self, filePath=''):
        SoftFile.__init__(self, filePath)

    def readStanza(self, stanza):
        if stanza[0].startswith('^SAMPLE'):
            entry = HighThroughputSampleStanza(self)
        elif stanza[0].startswith('^SERIES'):
            entry = SeriesStanza(self)
        else:
            raise KeyError(stanza[0])

        val = entry.readStanza(stanza)
        return val, entry
        
        
class MicroArraySoftFile(SoftFile):

    def __init__(self, filePath=''):
        SoftFile.__init__(self, filePath)
        
    def readStanza(self, stanza):
        if stanza[0].startswith('^SAMPLE'):
            entry = MicroArraySampleStanza(self)
        elif stanza[0].startswith('^SERIES'):
            entry = SeriesStanza(self)
        elif stanza[0].startswith('^PLATFORM'):
            entry = PlatformStanza(self)
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

    def __init__(self, keys, parent):
        self._name = ''
        self._keys = keys
        self.parent = parent
        OrderedDict.__init__(self)
        
    @property 
    def name(self):
        return self._name
        
    def setName(self, newname):
        for k in self._keys:
            if k.startswith('^'):
                self[k] = newname
                break
        self.parent[newname] = self.parent[self._name]
        del self.parent[self._name]
        self._name = newname
        
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
        if splitkey in self._keys and (self._keys[splitkey] == KeyZeroPlusNumbered or self._keys[splitkey] == KeyOnePlusNumbered):
            self[key] = val
        
        #this is for channel data in MicroArraySamples
        elif channelkey in self._keys and (self._keys[channelkey] == KeyZeroPlusChannel or self._keys[channelkey] == KeyOnePlusChannel):
            self[key] = val
        
        #if its a single value (ie 0 or 1 allowed entries)
        elif key in self._keys and (self._keys[key] == KeyRequired or self._keys[key] == KeyOptional):
            self[key] = val

        else:
        
            #if key not in self.keys:
            #    print splitkey
            #    raise KeyError(self._name + ': invalid key: ' + key)
            
            #if (self.keys[key] == KeyRequired or self.keys[key] == KeyOptional) and key in self:
            #    raise KeyError(self._name + ': too many of key: ' + key)
                
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
        
    def diff(self, other):
        result = dict()
        for key in self.iterkeys():
            if key not in other:
                result[key] = list()
                result[key].append(self[key])
                result[key].append(None)
            else:
                val1 = self[key]
                val2 = other[key]
                if val1 == None:
                    val1 = 'None'
                if val2 == None:
                    val2 = 'None'
                if isinstance(val1, list) and len(val1) == 1:
                    val1 = val1[0]
                if isinstance(val2, list) and len(val2) == 1:
                    val2 = val2[0]
                if val1 != val2:
                    if isinstance(val1, list) and isinstance(val2, list):
                        toremove = list()
                        for i in val1:
                            if i in val2:
                                toremove.append(i)
                        for r in toremove:
                            val1.remove(r)
                            val2.remove(r)
                    if (val1 != val2):
                        result[key] = list()
                        result[key].append(val1)
                        result[key].append(val2)
        for key in other.iterkeys():
            if key not in self:
                result[key] = list()
                result[key].append(None)
                result[key].append(other[key])
        return result


class MicroArrayPlatformStanza(SoftStanza):

    def __init__(self, parent):
    
        allowedkeys = { 
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
        
        SoftStanza.__init__(self, allowedkeys, parent)
        
        
class MicroArraySampleStanza(SoftStanza):

    def __init__(self, parent):
    
        allowedkeys = { 
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
        
        SoftStanza.__init__(self, allowedkeys, parent)        
        
        
class SeriesStanza(SoftStanza):
    
    def __init__(self, parent):
    
        allowedkeys = { 
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
                
        SoftStanza.__init__(self, allowedkeys, parent)

        
class HighThroughputSampleStanza(SoftStanza):

    def __init__(self, parent):
    
        allowedkeys = {
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
        
        SoftStanza.__init__(self, allowedkeys, parent)
        
