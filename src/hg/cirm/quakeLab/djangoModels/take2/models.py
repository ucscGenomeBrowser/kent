class PI(models.Model):
    """ Store PI name.  Names must be entered manually in Django admin interface
    """
    sunetid =  models.CharField('PI sunetid', max_length=32, primary_key=True, db_index=True )
    first =  models.CharField('first name', max_length=32)
    last =  models.CharField('surname', max_length=32, null=True, blank=True)
    fullname =  models.CharField('Full name', max_length=65, null=True, blank=True)
    aliases =  models.CharField('aliases', max_length=128, null=True, blank=True)

    def save(self):
        self.fullname = self.first + " " + self.last
        self.aliases = self.first + " " + self.last
        super(PI, self).save()

    class Meta:
        verbose_name = "PI"
        ordering = ['last', 'first']

    def __unicode__(self):
        if len(self.fullname):
            return self.fullname
        else:
            return self.sunetid

class User(models.Model):
    """Store user name and sunetid.  When first encountered, only the sunetid is used.  First name and surname must be entered manually
    """
    sunetid =  models.CharField('PI sunetid', max_length=32, primary_key=True, db_index=True )
    first =  models.CharField('first name', max_length=32)
    last =  models.CharField('surname', max_length=32)
    defaultPI = models.ForeignKey(PI,  null=True, blank=True)

    class Meta:
        ordering = ['last', 'first']

    def __unicode__(self):
        if len(self.first + " " + self.last) > 1:
            return self.last + ", " + self.first
        else:
            return self.sunetid

class SampleSpecies(models.Model):
    """ Species of sample.  New species are added upon first encounter by input programs. Practically speaking, alternatives are contrained by the options offered by the microscope imaging program Alias field contains mispelling found in legacy data (discovered using a contains query 
    """
    name =  models.CharField('species name', max_length=32, primary_key=True)
    aliases =  models.CharField('species aliases', max_length=128, default='', null=True, blank=True )

    class Meta:
         verbose_name = "Sample specie"

    def __unicode__(self):
        return self.name


class C1Instrument(models.Model):
    """ Available instruments, including other labs and systems at Fluidigm HQ
    """
    name = models.CharField('C1 id name', max_length=30, primary_key=True )
    location = models.CharField('location', max_length=16)
    serialnumber =  models.CharField('serial number', max_length=16, null = True, blank = True) aliases = models.CharField('name used in Leica program', max_length=30, null = True, blank = True )

    def __unicode__(self):
        return self.name

class ChipType(models.Model):
    """ Fluidigm C1 chip protocol type (e.g. mRNAseq)
    """
    type = models.CharField('chip protocol', max_length=64, primary_key=True)

    def __unicode__(self):
        return self.type

class SampleType(models.Model):
    """ Broad category of sample, such as cell culture, primary specimen, etc.
    """
    type = models.CharField('chip protocol', max_length=64, primary_key=True)

    def __unicode__(self):
        return self.type

class ChipSize(models.Model):
    """ Target cell size range captured by Fludigm C1 chip, in microns
    """
    size = models.CharField('cell capture size', max_length=64, primary_key=True )

    def __unicode__(self):
        return self.size

class SortMethod(models.Model):
    """ Method used to select cells out of original population prior to placing on C1
    """
    name = models.CharField('sort method', max_length=64, primary_key=True )

    def __unicode__(self):
        return self.name

class SerialMapping(models.Model):
    """ First four digits of the 10-digit Fluidigm C1 barcode.  These digits correspond to a unique chip protocol and and cell size range.  Used to automatically determine chip type as well as recognize legal 10-digit barcodes in text fields
    """
    serial_prefix =  models.CharField('serial number digits that encode chip type to be mapped', max_length=4, primary_key=True )
    type = models.ForeignKey(ChipType)
    size = models.ForeignKey(ChipSize)

    def __unicode__(self):
        return self.serial_prefix

class ImageType(models.Model):
    """ Describes the Fluo cube used for the image and the special case of "overlay" the composite of all images
    """
    name =  models.CharField('Imaging method', max_length=10, primary_key=True )

    def __unicode__(self):
        return self.name

class Tissue(models.Model):
    """ Tissue/organ from which cells were derived.  Input programs will add new tissues as they are encountered but microscope programs constrains user inputs to discourage tissue type proliferation
    """
    name =  models.CharField('source tissue', max_length = 64, primary_key=True )

    class Meta:
        ordering = ['name']

    def __unicode__(self):
        return self.name

class TissueAge(models.Model):
    """ age or passage number of sample, if applicable.  No ontology forced on this field as there is currently no agreement on one method (e.g.  gestation week, after birth in weeks or years
    """
    name =  models.CharField('tissue age', max_length = 64, primary_key=True )

    class Meta:
        ordering = ['name']

    def __unicode__(self):
        return self.name

class TargetLineage(models.Model):
    """ In contrast to tissue, this focuses on the cell type, e.g.  endothelial, mesenchyme, etc.  Does not follow a set ontology driven by requirements of individual projects
    """
    name =  models.CharField('target cell lineage', max_length = 64, primary_key=True )

    class Meta:
        ordering = ['name']

    def __unicode__(self):
        return self.name

class SampleStrain(models.Model):
    """ Modifier to Species.  Most appropriate for Mouse and microbial samples
    """
    name =  models.CharField('sample strain', max_length = 127, primary_key=True )

    class Meta:
        ordering = ['name']

    def __unicode__(self):
        return self.name

class StorageLocation(models.Model):
    """ Physical freezer location of capture plate from cell harvest
    """
    name = models.CharField('Storage Location', max_length=16, primary_key=True ) 
    class Meta:
        ordering = ['name']

    def __unicode__(self):
        return self.name


class CaptureArray(models.Model):
    """ The specific Fluidigm C1 capture array used for an individual experiment.  Currently plates have 96 locations to capture cells.  Contents of individual wells of their own record which references this array record.  
    """
    chipid = models.CharField('unique, 10-digit barcode', max_length=10, primary_key=True )
    type = models.ForeignKey(ChipType, null=True, blank=True)
    size = models.ForeignKey(ChipSize, null=True, blank=True)
    pi = models.ForeignKey(PI, null=True, blank=True)
    user = models.ForeignKey(User, null=True, blank=True)
    species = models.ForeignKey(SampleSpecies, null=True, blank=True)
    strain = models.ForeignKey(SampleStrain, null=True, blank=True)
    instrument = models.ForeignKey(C1Instrument, null=True, blank=True)
    sampletype = models.ForeignKey(SampleType, null=True, blank=True)
    tissue = models.ForeignKey(Tissue, null=True, blank=True)
    tissueage =  models.ForeignKey(TissueAge, null=True, blank=True)
    targetlineage =  models.ForeignKey(TargetLineage, null=True, blank=True)
    sortmethod = models.ForeignKey(SortMethod, null=True, blank=True)
    description = models.TextField('Sample description', max_length=1024, null=True, blank=True) 
    capturetime = models.DateTimeField('time and date of image capture', null=True, blank=True)
    revisiontime =  models.DateTimeField('time and date of last data update', null=True, blank=True)
    location = models.ForeignKey(StorageLocation, null=True, blank=True)
    mosaicimage = models.ImageField('mosaic image', max_length=255, null = True, blank = True)
    mosaicthumb = models.ImageField('mosaic thumbnail', max_length=255, null = True, blank = True)
    cirmproject = models.BooleanField('Associated with CIRM Cell Atlas Project', default = False)
    cirmxfer = models.BooleanField('Transferred to CIRM Cell Atlas Repository', default = False)
    datesearch =  models.CharField('date and time string for admin search',max_length=128,  blank=True, null=True)



    class Meta:
        ordering = ['chipid']

    def save(self):

        try:
            prefix = self.chipid[0:4]
            obj = SerialMapping.objects.filter(serial_prefix=prefix)[0]
            self.type = obj.type
            self.size = obj.size
        except:
            pass

        try:
            self.datesearch = self.capturetime.strftime('%Y%m%d%H%M%S')
        except:
            pass

        super(CaptureArray, self).save()

    def __unicode__(self):
        return self.chipid

class CellPresent(models.Model):
    """ Investigators subjective assessment of well contents, e.g. one cell, multi cell, no cell
    """
    evaluation = models.CharField('user evaluation', null = True, blank = True, default = "No response", max_length = 16)
    value = models.IntegerField('encoded value')

    class Meta:
         verbose_name = "user evaluation"

    def __unicode__(self):
        return self.evaluation

class HarvestWell(models.Model):
    """ harvest plate well in 96-well plate coordinates (e.g. A12, H02)
    """
    name = models.CharField('cell capture site name', max_length = 3, primary_key=True )

    class Meta:
        ordering = ['name']

    def __unicode__(self):
        return self.name


class ChipSite(models.Model):
    """ Chip sites available to capture cells, currently 96 in the format of C01 to C96 There were two legacy naming schemes used: ch## by the microscope program and C# for the first 9 capture sites.  This record helps to reconcole the legacy naming to current format
    """
    name = models.CharField('cell capture site name', max_length = 4, primary_key=True )
    harvestwell = models.ForeignKey(HarvestWell, null=True, blank=True)
    alias = models.CharField('cell capture site alias', max_length = 4, null=True, blank=True) 
    def save(self):
        try:
            self.alias = "C" + self.name[2:]
            super(ChipSite, self).save()
        except:
            pass

    class Meta:
        ordering = ['name']

    def __unicode__(self):
        if self.alias:
            return self.alias
        else:
            return self.name



class CellImage(models.Model):
    """ Location and metadata for each cell image and its thumbnail as reported by the microscope capture program.  The reference field is a hack to create a unique key based on 3 fields since the uniquer_ together capability in Django fails during data migration when N>2
    """
    chipid = models.ForeignKey(CaptureArray, null=True, blank=True)
    cellsite = models.ForeignKey(ChipSite, null=True, blank=True)
    harvestwell = models.ForeignKey(HarvestWell, null=True, blank=True)
    imagetype =  models.ForeignKey(ImageType, null=True, blank=True)
    gain = models.FloatField('gain', null=True, blank=True)
    magnification = models.IntegerField('magnification', default=20, null=True, blank=True)
    exposure = models.FloatField('exposure', null=True, blank=True)
    cellsize = models.FloatField('cell size', null=True, blank=True)
    cellpresent = models.ForeignKey(CellPresent, null=True, blank=True)
    image = models.ImageField('cell image', max_length=255, null = True, blank = True, upload_to='cellcapture/cellimages')
    thumbnail = models.ImageField('thumbnail image', max_length=255, null = True, blank = True, upload_to='cellcapture/cellimages')
    reference = models.CharField('unique reference', max_length=255, unique=True, blank=True, null=True )

    class Meta:
        ordering = ('chipid','cellsite','imagetype')

    def save(self):   # work-around failed three-way unique_together
        try:
            self.reference = u'%s:%s:%s'%(self.chipid, self.cellsite, self.imagetype)
            super(CellImage, self).save()
        except:
            pass

    def __unicode__(self):
        return u'%s:%s:%s'%(self.chipid, self.cellsite, self.imagetype)

class SortMarker(models.Model):
    """ name of a marker used to sort cells in method described in SortMethod
    """
    name = models.CharField('unique, sort marker', max_length=32, unique=True, blank=True, null=True )
    #name = models.CharField('unique, sort marker', max_length=32, default='None', blank=True, null=True )
    aliases =  models.CharField('Marker aliases', max_length=128, default='', blank=True, null=True)

    class Meta:
        ordering = ['name']

    def __unicode__(self):
        return self.name


class SortMarkerEntry(models.Model):
    """ Records a unique marker and capture array pair.  Used to determine which capture arrays are associated with a sort marker
    """
    sortmarker = models.ForeignKey(SortMarker, null=True, blank=True)
    chipid = models.ForeignKey(CaptureArray, null=True, blank=True)

    class Meta:
        ordering = ['sortmarker']
     #   unique_together = ['sortmarker', 'chipid']

    def __unicode__(self):
        return   u'%s:%s'%(self.chipid, self.sortmarker)
