# -*- coding: utf-8 -*-
from __future__ import unicode_literals

import datetime
from django.db import models
from django.utils import timezone
from django.core.validators import MaxValueValidator, MinValueValidator

class Contributor(models.Model):
    name = models.CharField(max_length=250, unique=True)
    email = models.EmailField(max_length=254, blank=True)
    phone = models.CharField(max_length=25, blank=True)
    project_role = models.CharField(max_length=100, default="contributor")
    def __str__(self):
       return self.name

class Lab(models.Model):
    short_name =  models.CharField(max_length=50, unique=True)
    institution = models.CharField(max_length=250, blank="")
    contributors = models.ManyToManyField(Contributor)
    contact = models.ForeignKey(Contributor, null=True, default=None, on_delete=models.SET_NULL, related_name="contact")
    pi = models.ForeignKey(Contributor, null=True, default=None, on_delete=models.SET_NULL, related_name="pi")
    def __str__(self):
       return self.short_name

class Species(models.Model):
     common_name = models.CharField(max_length = 40)
     scientific_name = models.CharField(max_length=150)
     ncbi_taxon = models.IntegerField(unique=True)
     def __str__(self):
       return self.common_name

class ProjectState(models.Model):
    state = models.CharField(max_length=30, unique=True)
    description = models.CharField(max_length=100)
    def __str__(self):
       return self.state

class OrganPart(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    def __str__(self):
        return self.short_name

class Organ(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    def __str__(self):
        return self.short_name
    
class Disease(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    def __str__(self):
        return self.short_name

class Consent(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    def __str__(self):
        return self.short_name

class SampleType(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    def __str__(self):
        return self.short_name

class AssayType(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    def __str__(self):
        return self.short_name

class AssayTech(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=250)
    def __str__(self):
        return self.short_name

class Publication(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    title = models.CharField(max_length=250, blank=True)
    pmid = models.CharField(max_length=16)
    doi = models.CharField(max_length=32)
    def __str__(self):
        return self.short_name

class Project(models.Model):
    short_name = models.CharField(max_length=80)
    state = models.ForeignKey(ProjectState, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    score = models.IntegerField(blank=True,validators=[MinValueValidator(1),MaxValueValidator(5)], default=3)
    wrangler1 = models.ForeignKey(Contributor, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="wrangler1")
    wrangler2 = models.ForeignKey(Contributor, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="wrangler2")
    title = models.CharField(max_length=120)
    description = models.TextField()
    labs = models.ManyToManyField(Lab, blank=True)
    contributors = models.ManyToManyField(Contributor)
    species = models.ManyToManyField(Species, blank=True)
    organ = models.ManyToManyField(Organ, blank=True)
    organ_part = models.ManyToManyField(OrganPart, blank=True)
    disease = models.ManyToManyField(Disease, blank=True)
    sample_type = models.ManyToManyField(SampleType, blank=True)
    consent = models.ForeignKey(Consent, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    assay_type = models.ManyToManyField(AssayType, blank=True)
    assay_tech = models.ManyToManyField(AssayTech, blank=True)
    cells_expected = models.IntegerField(blank=True, default=0)
    publications = models.ManyToManyField(Publication, blank=True)
    def __str__(self):
       return self.short_name

class Funder(models.Model):
    short_name = models.CharField(max_length=50)
    description = models.TextField()
    def __str__(self):
       return self.short_name

class Grant(models.Model):
    grant_title = models.CharField(max_length=200)
    grant_id = models.CharField(max_length=200)
    funder = models.ForeignKey(Funder, null=True, default=None, on_delete=models.SET_NULL)
    funded_projects = models.ManyToManyField(Project, blank=True)
    funded_labs = models.ManyToManyField(Lab, blank=True)
    funded_contributors = models.ManyToManyField(Contributor, blank=True)
    def __str__(self):
       return self.grant_title

