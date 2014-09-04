from django.db import models

from cellcapture.models import *

from django.core.files import File
from django.core.files.storage import FileSystemStorage
from datetime import datetime
import urllib
import os

#fragmentfs = FileSystemStorage(location='/b7_1/exports/fragment')
class FragmentPlate(models.Model):
    plate = models.DateTimeField('time and date of AATI run')
    datesearch =  models.CharField('date and time string for admin search',max_length=128,  blank=True, null=True) 

    def save(self):
        try:
            self.datesearch = self.plate.strftime('%Y%m%d%H%M%S')
        except:
            pass
        super(FragmentPlate, self).save()

    def __unicode__(self):
        return self.plate.strftime('%Y%m%d%H%M%S') 


class FragmentWell(models.Model):
    plate = models.ForeignKey(FragmentPlate) 
    well = models.ForeignKey(HarvestWell)  # capture plate well names used, though these are different plates. Used to enforce name correctness only
    capturearray = models.ForeignKey(CaptureArray,  blank=True, null=True)
    chipsite = models.ForeignKey(ChipSite,  blank=True, null=True) 
    sampleid = models.CharField('User provided sample name',max_length=128,  blank=True, null=True)
    range = models.CharField('Fragment size analysis range',max_length=128,  blank=True, null=True)
    ngul = models.FloatField('Fragment concentration ng/ul', blank=True, null=True) 
    percenttotal = models.FloatField('% of Total', blank=True, null=True) 
    nmolel =  models.FloatField('Fragment concentration nmole/l', blank=True, null=True)
    size = models.FloatField('Average size', blank=True, null=True)
    percentcv =  models.FloatField('%CV', blank=True, null=True)

    class Meta:
        ordering = ['plate', 'well']
        unique_together = ['plate', 'well']

    def __unicode__(self):
        return u'%s:%s'%(self.plate, self.well)
