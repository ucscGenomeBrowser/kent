from django.db import models


from cellcapture.models import User, PI, CaptureArray, ChipSite

class SequencerModel(models.Model):
    name = models.CharField('sequencer model name', max_length=30, primary_key=True )
    
    class Meta:
        ordering = ['name']

    def __unicode__(self): 
        return self.name

class SequencerInstrument(models.Model):
    name = models.CharField('sequencer id name', max_length=30, default='', null=True, blank=True )
    model = models.ForeignKey(SequencerModel, null=True, blank=True)
    serialnumber =  models.CharField('serial number reported in run name', max_length=32, primary_key = True)
    location = models.CharField('location', max_length=16, default='', null=True, blank=True )
    
    class Meta:
        ordering = ['name']

    def __unicode__(self): 
	try:
            return self.name + " " + self.location + " " + self.serialnumber
	except:
	    return self.serialnumber

class RunType(models.Model):

    name = models.CharField('run type', null=True, blank=True, max_length=60, unique=True)
    read1cycles = models.IntegerField("read one cycle count", default=0) 
    read2cycles = models.IntegerField("read two cycle count", default=0) 
    index1cycles = models.IntegerField("index one cycle count", default=0) 
    index2cycles = models.IntegerField("index two cycle count", default=0) 
 
    class Meta:
        ordering = ['name']

    def save(self):
        tempname = 'Not specified' 
        if self.read1cycles:
	    tempname = '%i'%self.read1cycles
	    if self.index1cycles:
		tempname += " (%i)"%self.index1cycles
	        if self.index2cycles:
		   tempname += " (%i)"%self.index2cycles
            if self.read2cycles:
	        tempname += " %i"%self.read2cycles
	    self.name = tempname
            super(RunType, self).save()

    def __unicode__(self): 
        return self.name
    
class Run(models.Model):
    name = models.CharField('Run ID', primary_key=True, max_length=60)
    flowcellid=models.CharField('flow cell ID', max_length=20, null=True, blank=True)
    date = models.DateTimeField('Run start date', null=True)
    instrument = models.ForeignKey(SequencerInstrument, null=True)
    runtype = models.ForeignKey(RunType, null=True)
    sequenced = models.BooleanField('sequenced?',default=False)
    cirmproject = models.BooleanField('Associated with CIRM Cell Atlas Project', default = False)
    cirmxfer = models.BooleanField('Transferred to CIRM Cell Atlas Repository', default = False)
    path =  models.CharField('path to run folder', max_length=255, null=True, blank=True)

    class Meta:
        ordering = ['name']
    
    def __unicode__(self): 
        return self.name

class BarcodeSet(models.Model):
    name = models.CharField('name for barcode set', max_length=30, primary_key=True ) 
    
    class Meta:
        ordering = ['name']

    def __unicode__(self):
        return self.name

class Barcode(models.Model):
    name = models.CharField('core unique name for barcode', max_length=30, editable=False, primary_key=True )
    barcodeset = models.ForeignKey(BarcodeSet)
    index1 = models.CharField('first index sequence', max_length=16)
    index2 = models.CharField('second index sequence', max_length=16)
    index2rc = models.CharField('reverse complement of index2, currently required by NextSeq', max_length=16)
    dualindex =  models.CharField('legacy combined index separated by \"-\"', max_length=33)
    adapter1 = models.CharField('first complete adapter sequence containing index', max_length=100)
    adapter2 = models.CharField('second complete adapter sequence containing index', max_length=100)
    
    class Meta:
        ordering = ['name']

    def __unicode__(self): 
        return self.name


class CoreLibraryID(models.Model):
    name = models.CharField('core IL or IM name', max_length=30, editable=False, primary_key=True )
    description = models.TextField('Long Library description', max_length=1024, null=True, blank=True)
    
    class Meta:
        ordering = ['name']

    def __unicode__(self): 
        return self.name

class LibrarySample(models.Model):
    name =  models.CharField('user-provided descriptive sample name', max_length=255, primary_key=True)
    libraryid = models.ForeignKey(CoreLibraryID, null=True, blank=True)
    index1 = models.CharField('first index sequence', max_length=16,  null=True, blank=True)
    index2 = models.CharField('second index sequence', max_length=16,  null=True, blank=True)
    barcode = models.ForeignKey(Barcode,blank=True, null=True)
    description = models.TextField('Long sample description', max_length=1024, null=True, blank=True)
    chipid = models.ForeignKey('cellcapture.CaptureArray', null=True, blank=True)
    chipsite = models.ForeignKey('cellcapture.ChipSite', null=True, blank=True)
    user = models.ForeignKey('cellcapture.User', null=True)

    class Meta:
        ordering = ['libraryid', 'name',]
        unique_together = ['libraryid','name',]
    
    def save(self):
	if self.barcode:
	    self.index1 = self.barcode.index1
	    self.index2 = self.barcode.index2
	super(LibrarySample, self).save()

    def __unicode__(self): 
	return self.name

class RunEntry(models.Model):
    run = models.ForeignKey(Run)
    lane = models.IntegerField('lane number', default = 1)
    sample = models.ForeignKey(LibrarySample, null=True)
    user = models.ForeignKey('cellcapture.User', null=True)
    pi = models.ForeignKey('cellcapture.PI', null=True)
    toanalyze = models.BooleanField('Perform automated analysis', default = False)

    class Meta:
        ordering = ['run', 'lane', 'sample']
        unique_together = ['run', 'lane', 'sample']

    def __unicode__(self): 
        return  u'%s'%self.sample
    
