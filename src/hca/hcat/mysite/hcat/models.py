# -*- coding: utf-8 -*-
from __future__ import unicode_literals

import datetime
from django.db import models
from django.utils import timezone
from django.core.validators import MaxValueValidator, MinValueValidator

class ContributorType(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler contributor type'

class Contributor(models.Model):
    name = models.CharField(max_length=255, unique=True)
    email = models.EmailField(max_length=128, blank=True)
    phone = models.CharField(max_length=32, blank=True)
    address = models.CharField(max_length=255, blank=True)
    department = models.CharField(max_length=255, blank=True)
    institute = models.CharField(max_length=255, blank=True)
    city = models.CharField(max_length=100, blank=True)
    zip_postal_code = models.CharField(max_length=20, blank=True)
    country = models.CharField(max_length = 100, blank=True)
    projects = models.ManyToManyField("Project", blank=True, through="project_contributors")
    labs = models.ManyToManyField("Lab", blank=True, through="lab_contributors")
    grants = models.ManyToManyField("Grant", blank=True, through="grant_funded_contributors")
    type = models.ForeignKey(ContributorType, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.name

class Lab(models.Model):
    short_name =  models.CharField(max_length=50, unique=True)
    institution = models.CharField(max_length=255, null=True, blank=True)
    #contacts = models.ManyToManyField(Contributor, related_name="contacts")
    pi = models.ForeignKey(Contributor, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="pi")
    contributors = models.ManyToManyField(Contributor, blank=True, related_name="contributors")
    projects = models.ManyToManyField("Project", blank=True, through="project_labs")
    grants = models.ManyToManyField("Grant", blank=True, through="grant_funded_labs")
    comments = models.CharField(max_length=255, blank=True)
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

class Url(models.Model):
    short_name = models.CharField(unique=True, max_length=50)
    path = models.URLField()
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.short_name
    class Meta:
       verbose_name = 'Wrangler url'

class File(models.Model):
    short_name = models.CharField(unique=True, max_length=80)
    file = models.FileField(upload_to="uploads")
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.short_name
    class Meta:
       verbose_name = 'Wrangler file'

class ProjectState(models.Model):
    state = models.CharField(max_length=30, unique=True)
    description = models.CharField(max_length=100)
    def __str__(self):
       return self.state
    class Meta:
       verbose_name = 'Wrangler project state'

class OrganPart(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    ontology_id = models.CharField(max_length=32, blank=True, null=True)
    ontology_label = models.CharField(max_length=255, blank=True, null=True)
    description = models.CharField(max_length=255, blank=True)
    projects = models.ManyToManyField("Project", blank=True, through="project_organ_part")
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler organ part'

class Organ(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    ontology_id = models.CharField(max_length=32, blank=True, null=True)
    ontology_label = models.CharField(max_length=255, blank=True, null=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler organ'
    
class Disease(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    projects = models.ManyToManyField("Project", blank=True, through="project_disease", related_name='projects_diseases_relationship')
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler disease'

class Consent(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler consent'

class SampleType(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler sample type'

class AssayTech(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    projects = models.ManyToManyField("Project", blank=True, through="project_assay_tech", related_name='projects_assay_tech_relationship')
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler assay tech'

class AssayType(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    projects = models.ManyToManyField("Project", blank=True, through="project_assay_type", related_name='projects_assay_type_relationship')
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler assay type'

class Publication(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    title = models.CharField(max_length=255, blank=True)
    pmid = models.CharField(max_length=16)
    comments = models.CharField(max_length=255, blank=True)
    doi = models.CharField(max_length=32)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler publication'

class EffortType(models.Model):
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'Wrangler effort type'

class SoftwareDeveloper(models.Model):
    who = models.ForeignKey(Contributor, on_delete=models.PROTECT)
    favorite_languages = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.who.__str__()
    class Meta:
       verbose_name = 'Wrangler software developer'

class Intern(models.Model):
    who = models.ForeignKey(Contributor, on_delete=models.PROTECT)
    advisor = models.ForeignKey(Contributor, null=True, on_delete=models.SET_NULL, related_name='advisor')
    interests = models.CharField(max_length=128, blank=True)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.who.__str__()
    class Meta:
       verbose_name = 'Wrangler intern'

class Wrangler(models.Model):
    who = models.ForeignKey(Contributor, on_delete=models.PROTECT)
    favorite_site = models.URLField(null=True,blank=True)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.who.__str__()

class Project(models.Model):
    short_name = models.CharField(max_length=80)
    cur_state = models.ForeignKey(ProjectState, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="cur_state")
    comments = models.CharField(max_length=255, blank=True)
    state_reached = models.ForeignKey(ProjectState, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="state_reached")
    stars = models.IntegerField(blank=True,validators=[MinValueValidator(1),MaxValueValidator(5)], default=3)
    chat_url = models.URLField(blank=True, null=True)
    wrangler1 = models.ForeignKey(Wrangler, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="wrangler1");
    wrangler2 = models.ForeignKey(Wrangler, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="wrangler2");
    contacts = models.ManyToManyField(Contributor, blank=True, related_name="projcontacts")
    first_contact_date = models.DateField(blank=True, null=True, default=None)
    last_contact_date = models.DateField(blank=True, null=True, default=None)
    responders = models.ManyToManyField(Contributor, blank=True, related_name="projresponders")
    first_response_date = models.DateField(blank=True, null=True, default=None)
    last_response_date = models.DateField(blank=True, null=True, default=None)
    questionnaire_comments = models.CharField(max_length=255, blank=True)
    questionnaire_date = models.DateField(blank=True, null=True, default=None)
    tAndC_comments = models.CharField(max_length=255, blank=True)
    tAndC_date = models.DateField(blank=True, null=True, default=None)
    sheet_template = models.FileField(upload_to="uploads/project", blank=True, null=True, default=None)
    sheet_template_date = models.DateField(blank=True, null=True, default=None)
    sheet_from_lab = models.FileField(upload_to="uploads/project", blank=True, null=True, default=None)
    sheet_from_lab_date = models.DateField(blank=True, null=True, default=None)
    back_to_lab = models.FileField(upload_to="uploads/project", blank=True, null=True, default=None)
    back_to_lab_date = models.DateField(blank=True, null=True, default=None)
    lab_review_comments = models.CharField(max_length=255, blank=True)
    lab_review_date = models.DateField(blank=True, null=True, default=None)
    sheet_that_validated = models.FileField(upload_to="uploads/project", blank=True, null=True, default=None)
    sheet_validated_date = models.DateField(blank=True, null=True, default=None)
    staging_area = models.URLField(blank=True, null=True)
    staging_area_date = models.DateField(blank=True, null=True, default=None)
    submit_date = models.DateField(blank=True, null=True, default=None)
    submit_comments = models.CharField(max_length=255, blank=True)
    cloud_date = models.DateField(blank=True, null=True, default=None)
    pipeline_date = models.DateField(blank=True, null=True, default=None)
    orange_date = models.DateField(blank=True, null=True, default=None)
    title = models.CharField(max_length=120)
    description = models.TextField()
    labs = models.ManyToManyField(Lab, blank=True)
    organ = models.ManyToManyField(Organ, blank=True)
    organ_part = models.ManyToManyField(OrganPart, blank=True)
    disease = models.ManyToManyField(Disease, blank=True)
    sample_type = models.ManyToManyField(SampleType, blank=True)
    consent = models.ForeignKey(Consent, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    effort = models.ForeignKey(EffortType, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    origin_name = models.CharField(max_length=200, blank=True)
    assay_type = models.ManyToManyField(AssayType, blank=True)
    assay_tech = models.ManyToManyField(AssayTech, blank=True)
    cells_expected = models.IntegerField(blank=True, default=0)
    publications = models.ManyToManyField(Publication, blank=True)
    contributors = models.ManyToManyField(Contributor)
    species = models.ManyToManyField(Species, blank=True)
    grants = models.ManyToManyField("Grant", blank=True, through="grant_funded_projects")
    files = models.ManyToManyField(File, blank=True)
    urls = models.ManyToManyField(Url, blank=True)
    def __str__(self):
       return self.short_name

class Funder(models.Model):
    short_name = models.CharField(max_length=50)
    description = models.TextField()
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.short_name
    class Meta:
       verbose_name = 'Wrangler funder'


class Grant(models.Model):
    grant_title = models.CharField(max_length=200)
    grant_id = models.CharField(max_length=200)
    funder = models.ForeignKey(Funder, null=True, default=None, on_delete=models.SET_NULL)
    funded_projects = models.ManyToManyField(Project, blank=True)
    funded_labs = models.ManyToManyField(Lab, blank=True)
    funded_contributors = models.ManyToManyField(Contributor, blank=True)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.grant_title

