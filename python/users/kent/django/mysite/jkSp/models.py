# This is an auto-generated Django model module.
# You'll have to do the following manually to clean this up:
#     * Rearrange models' order
#     * Make sure each model has one field with primary_key=True
# Feel free to rename the models, but don't rename db_table values or field names.
#
# Also note: You'll have to insert the output of 'django-admin.py sqlcustom [appname]'
# into your database.

from django.db import models

class DisplayId(models.Model):
    acc = models.CharField(max_length=36, primary_key=True)
    val = models.CharField(unique=True, max_length=36)
    features = models.ManyToManyField('FeatureClass', through='Feature')
    class Meta:
        db_table = u'jkSp_displayId'

    def __unicode__(self):
        return self.acc + ' ' + self.val

class Keyword(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.CharField(max_length=765)
    class Meta:
        db_table = u'jkSp_keyword'

    def __unicode__(self):
        return `self.id` + ' ' + self.val

#class AccToKeyword(models.Model):
#    acc = models.ForeignKey(DisplayId, db_column='acc', editable=False)
#    keyword = models.ForeignKey(Keyword, db_column='keyword')
#    class Meta:
#        db_table = u'jkSp_accToKeyword'
#
#    def __unicode__(self):
#        return self.acc + ' ' + self.keyword

class Taxon(models.Model):
    id = models.IntegerField(primary_key=True)
    binomial = models.CharField(max_length=765)
    toGenus = models.TextField(db_column='toGenus') # Field name made lowercase.
    class Meta:
        db_table = u'jkSp_taxon'

    def __unicode__(self):
        return self.binomial

#class AccToTaxon(models.Model):
    #acc = models.ForeignKey(DisplayId, db_column='acc', editable=False)
    #taxon = models.ForeignKey(Taxon, db_column='taxon')
    #class Meta:
        #db_table = u'jkSp_accToTaxon'

class Author(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.CharField(max_length=765)
    class Meta:
        db_table = u'jkSp_author'

    def __unicode__(self):
        return `self.id` + ' ' + self.val

class Reference(models.Model):
    id = models.IntegerField(primary_key=True)
    title = models.TextField()
    cite = models.TextField()
    pubMed = models.CharField(max_length=30, db_column='pubMed') # Field name made lowercase.
    medline = models.CharField(max_length=36)
    doi = models.CharField(max_length=765)
    class Meta:
        db_table = u'jkSp_reference'

    def __unicode__(self):
        return self.title

#class ReferenceAuthors(models.Model):
    #reference = models.ForeignKey(Reference, db_column='reference')
    #author = models.ForeignKey(Author, db_column='author')
    #class Meta:
        #db_table = u'jkSp_referenceAuthors'

class CitationRp(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.TextField()
    class Meta:
        db_table = u'jkSp_citationRp'

    def __unicode__(self):
        return `self.id` + ' ' + self.val
        

class Citation(models.Model):
    id = models.IntegerField(primary_key=True)
    acc = models.ForeignKey(DisplayId, db_column='acc', editable=False)
    reference = models.ForeignKey(Reference, db_column='reference')
    rp = models.ForeignKey(CitationRp, db_column = 'rp')
    class Meta:
        db_table = u'jkSp_citation'

    def __unicode__(self):
        return `self.id` + ' ' + self.acc

class RcType(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.CharField(max_length=765)
    class Meta:
        db_table = u'jkSp_rcType'

    def __unicode__(self):
        return `self.id` + ' ' + self.val
        
class RcVal(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.TextField()
    class Meta:
        db_table = u'jkSp_rcVal'

    def __unicode__(self):
        return `self.id` + ' ' + self.val
        
#class CitationRc(models.Model):
    #citation = models.ForeignKey(Citation, db_column='citation')
    #rcType = models.ForeignKey(RcType, db_column='rcType') # Field name made lowercase.
    #rcVal = models.ForeignKey(RcVal, db_column='rcVal') # Field name made lowercase.
    #class Meta:
        #db_table = u'jkSp_citationRc'

class CommentType(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.CharField(max_length=765)
    class Meta:
        db_table = u'jkSp_commentType'

    def __unicode__(self):
        return `self.id` + ' ' + self.val
        
class CommentVal(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.TextField()
    class Meta:
        db_table = u'jkSp_commentVal'

    def __unicode__(self):
        return `self.id` + ' ' + self.val

#class Comment(models.Model):
#    acc = models.ForeignKey(DisplayId, db_column='acc')
#    commentType = models.ForeignKey(CommentType, db_column='commentType') # Field name made lowercase.
#    commentVal = models.ForeignKey(CommentVal, db_column='commentVal') # Field name made lowercase.
#    class Meta:
#        db_table = u'jkSp_comment'

#class CommonName(models.Model):
#    taxon = models.ForeignKey(Taxon, db_column='taxon')
#    val = models.CharField(max_length=765)
#    class Meta:
#        db_table = u'jkSp_commonName'

class Description(models.Model):
    acc = models.ForeignKey(DisplayId, db_column='acc', primary_key=True, editable=False)
    val = models.TextField()
    class Meta:
        db_table = u'jkSp_description'

    def __unicode__(self):
        return `self.acc` + ' ' + self.val

class ExtDb(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.CharField(max_length=765)
    class Meta:
        db_table = u'jkSp_extDb'

    def __unicode__(self):
        return `self.id` + ' ' + self.val

#class ExtDbRef(models.Model):
#    acc = models.ForeignKey(DisplayId, db_column='acc')
#    extDb = models.ForeignKey(ExtDb, db_column='extDb') # Field name made lowercase.
#    extAcc1 = models.CharField(max_length=765, db_column='extAcc1') # Field name made lowercase.
#    extAcc2 = models.CharField(max_length=765, db_column='extAcc2') # Field name made lowercase.
#    extAcc3 = models.CharField(max_length=765, db_column='extAcc3') # Field name made lowercase.
#    class Meta:
#        db_table = u'jkSp_extDbRef'

class FeatureClass(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.CharField(max_length=765)
    class Meta:
        db_table = u'jkSp_featureClass'

    def __unicode__(self):
        return `self.id` + ' ' + self.val

class FeatureId(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.CharField(max_length=120)
    class Meta:
        db_table = u'jkSp_featureId'

    def __unicode__(self):
        return `self.id` + ' ' + self.val

class FeatureType(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.TextField()
    class Meta:
        db_table = u'jkSp_featureType'

    def __unicode__(self):
        return `self.id` + ' ' + self.val

class Feature(models.Model):
    id = models.IntegerField(primary_key=True)
    acc = models.ForeignKey(DisplayId, db_column='acc')
    start = models.IntegerField()
    end = models.IntegerField()
    featureClass = models.ForeignKey(FeatureClass, db_column='featureClass') 
    featureType = models.ForeignKey(FeatureType, db_column='featureType') 
    softEndBits = models.IntegerField(db_column='softEndBits') 
    featureId = models.ForeignKey(FeatureId, db_column='featureId') 
    class Meta:
        db_table = u'jkSp_feature'

    def __unicode__(self):
        return `self.acc` + ' ' + `self.featureClass` + ' ' + `self.featureType`

#class Gene(models.Model):
    #acc = models.ForeignKey(DisplayId, db_column='acc')
    #val = models.CharField(max_length=765)
    #class Meta:
        #db_table = u'jkSp_gene'

    #def __unicode__(self):
        #return `self.acc` + ' ' + self.val

#class GeneLogic(models.Model):
    #acc = models.ForeignKey(DisplayId, db_column='acc')
    #val = models.TextField()
    #class Meta:
        #db_table = u'jkSp_geneLogic'

    #def __unicode__(self):
        #return `self.acc` + ' ' + self.val

class Organelle(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.TextField()
    class Meta:
        db_table = u'jkSp_organelle'

    def __unicode__(self):
        return `self.id` + ' ' + self.val

class Info(models.Model):
    acc = models.ForeignKey(DisplayId, db_column='acc', primary_key=True, editable=False)
    isCurated = models.IntegerField(db_column='isCurated') # Field name made lowercase.
    aaSize = models.IntegerField(db_column='aaSize') # Field name made lowercase.
    molWeight = models.IntegerField(db_column='molWeight') # Field name made lowercase.
    createDate = models.DateField(db_column='createDate') # Field name made lowercase.
    seqDate = models.DateField(db_column='seqDate') # Field name made lowercase.
    annDate = models.DateField(db_column='annDate') # Field name made lowercase.
    organelle = models.ForeignKey(Organelle, db_column='organelle')
    class Meta:
        db_table = u'jkSp_info'

    def __unicode__(self):
        return `self.acc` + ' ' + `self.createDate`


#class OtherAcc(models.Model):
#    acc = models.ForeignKey(DisplayId, db_column='acc', editable=False)
#    val = models.CharField(max_length=36)
#    class Meta:
#        db_table = u'jkSp_otherAcc'

#class PathogenHost(models.Model):
#    pathogen = models.ForeignKey(Taxon, db_column='pathogen')
#    host = models.ForeignKey(Taxon, db_column='host')
#    class Meta:
#        db_table = u'jkSp_pathogenHost'

class Protein(models.Model):
    acc = models.ForeignKey(DisplayId, db_column='acc', primary_key=True, editable=False)
    val = models.TextField()
    class Meta:
        db_table = u'jkSp_protein'

    def __unicode__(self):
        return `self.acc` + self.val[:5] + '...'

class ProteinEvidenceType(models.Model):
    id = models.IntegerField(primary_key=True)
    val = models.TextField()
    class Meta:
        db_table = u'jkSp_proteinEvidenceType'

    def __unicode__(self):
        return `self.id` + ' ' + self.val

#class ProteinEvidence(models.Model):
    #acc = models.ForeignKey(DisplayId, db_column='acc', editable=False)
    #proteinEvidenceType = models.ForeignKey(ProteinEvidenceType, db_column='proteinEvidenceType') # Field name made lowercase.
    #class Meta:
        #db_table = u'jkSp_proteinEvidence'

class VarAcc(models.Model):
    varAcc = models.CharField(max_length=36, primary_key=True, db_column='varAcc') # Field name made lowercase.
    parAcc = models.ForeignKey(DisplayId, db_column='parAcc') # Field name made lowercase.
    variant = models.CharField(max_length=12)
    class Meta:
        db_table = u'jkSp_varAcc'

    def __unicode__(self):
        return self.varAcc + ' var of ' + `self.parAcc`

class VarProtein(models.Model):
    acc = models.ForeignKey(DisplayId, db_column='acc', primary_key=True, editable=False)
    val = models.TextField()
    class Meta:
        db_table = u'jkSp_varProtein'

    def __unicode__(self):
        return `self.acc` + ' ' + self.val[:20]
