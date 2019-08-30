# -*- coding: utf-8 -*-
from __future__ import unicode_literals

import datetime
from django.db import models
from django.utils import timezone
from django.core.validators import MaxValueValidator, MinValueValidator

class VocabCommentType(models.Model):
    short_name = models.CharField(unique=True, max_length=10)
    description = models.TextField()
    def __str__(self):
       return self.short_name

class VocabComment(models.Model):
    type = models.ForeignKey(VocabCommentType, on_delete=models.PROTECT)
    text = models.CharField(unique=True, max_length=255)
    def __str__(self):
       #return self.type.short_name + ": " + self.text[:80]
       # The above breaks the lookup, possible to do but not worth it yet.
       return self.text
    
class Contributor(models.Model):
    name = models.CharField(max_length=250, unique=True)
    email = models.EmailField(max_length=254, blank=True)
    phone = models.CharField(max_length=25, blank=True)
    project_role = models.CharField(max_length=100, default="contributor")
    projects = models.ManyToManyField("Project", blank=True, through="project_contributors")
    labs = models.ManyToManyField("Lab", blank=True, through="lab_contributors")
    grants = models.ManyToManyField("Grant", blank=True, through="grant_funded_contributors")
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
       return self.name

class Lab(models.Model):
    short_name =  models.CharField(max_length=50, unique=True)
    institution = models.CharField(max_length=250, blank="")
    contact = models.ForeignKey(Contributor, null=True, default=None, on_delete=models.SET_NULL, related_name="contact")
    pi = models.ForeignKey(Contributor, null=True, default=None, on_delete=models.SET_NULL, related_name="pi")
    contributors = models.ManyToManyField(Contributor)
    projects = models.ManyToManyField("Project", blank=True, through="project_labs")
    grants = models.ManyToManyField("Grant", blank=True, through="grant_funded_labs")
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
       return self.short_name

class Species(models.Model):
     common_name = models.CharField(max_length = 40)
     scientific_name = models.CharField(max_length=150)
     ncbi_taxon = models.IntegerField(unique=True)
     def __str__(self):
       return self.common_name
     class Meta:
       verbose_name_plural = "species"

class VocabProjectState(models.Model):
    state = models.CharField(max_length=30, unique=True)
    description = models.CharField(max_length=100)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
       return self.state

class VocabOrganPart(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
        return self.short_name

class VocabOrgan(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
        return self.short_name
    
class VocabDisease(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
        return self.short_name

class VocabConsent(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
        return self.short_name

class VocabSampleType(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
        return self.short_name

class VocabAssayTech(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
        return self.short_name

class VocabAssayType(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
        return self.short_name

class VocabPublication(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    title = models.CharField(max_length=250, blank=True)
    pmid = models.CharField(max_length=16)
    doi = models.CharField(max_length=32)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
        return self.short_name

class VocabProjectOrigin(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
        return self.short_name

class Project(models.Model):
    short_name = models.CharField(max_length=80)
    state = models.ForeignKey(VocabProjectState, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    stars = models.IntegerField(blank=True,validators=[MinValueValidator(1),MaxValueValidator(5)], default=3)
    wrangler1 = models.ForeignKey(Contributor, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="wrangler1")
    wrangler2 = models.ForeignKey(Contributor, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="wrangler2")
    title = models.CharField(max_length=120)
    description = models.TextField()
    labs = models.ManyToManyField(Lab, blank=True)
    organ = models.ManyToManyField(VocabOrgan, blank=True)
    organ_part = models.ManyToManyField(VocabOrganPart, blank=True)
    disease = models.ManyToManyField(VocabDisease, blank=True)
    sample_type = models.ManyToManyField(VocabSampleType, blank=True)
    consent = models.ForeignKey(VocabConsent, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    origin = models.ForeignKey(VocabProjectOrigin, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    assay_type = models.ManyToManyField(VocabAssayType, blank=True)
    assay_tech = models.ManyToManyField(VocabAssayTech, blank=True)
    cells_expected = models.IntegerField(blank=True, default=0)
    publications = models.ManyToManyField(VocabPublication, blank=True)
    contributors = models.ManyToManyField(Contributor)
    species = models.ManyToManyField(Species, blank=True)
    grants = models.ManyToManyField("Grant", blank=True, through="grant_funded_projects")
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
       return self.short_name

class VocabFunder(models.Model):
    short_name = models.CharField(max_length=50)
    description = models.TextField()
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
       return self.short_name


class Grant(models.Model):
    grant_title = models.CharField(max_length=200)
    grant_id = models.CharField(max_length=200)
    funder = models.ForeignKey(VocabFunder, null=True, default=None, on_delete=models.SET_NULL)
    funded_projects = models.ManyToManyField(Project, blank=True)
    funded_labs = models.ManyToManyField(Lab, blank=True)
    funded_contributors = models.ManyToManyField(Contributor, blank=True)
    comments = models.ManyToManyField(VocabComment, blank=True);
    def __str__(self):
       return self.grant_title

