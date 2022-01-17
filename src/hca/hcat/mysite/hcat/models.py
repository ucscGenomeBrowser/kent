# -*- coding: utf-8 -*-
from __future__ import unicode_literals

import datetime
import uuid
from django.db import models
from django.utils import timezone
from django.core.validators import MaxValueValidator, MinValueValidator

class ContributorType(models.Model):
    """ Manage a controlled vocabulary that describes the type of 
    contribution to the project.
    """
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x contributor type'

class Contributor(models.Model):
    """ Someone who has contributed to a project.  They may or may not
    be staff with accounts with us.  Many are taken from the GEO
    or ENA submissions records.  Because addresses and other contact
    information can subject people to harrassment,  we limit the
    public access of contributor to nothing more than name.
    """
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
    """ A lab is kind of a vague concept. Some labs are huge and have internal
    sublab structures.  Some are tiny.  Some last a long time, others are
    fleeting.  People go back and forth between labs and can belong to more
    than one lab at a time.  The relationship between lab and project is also
    many to many.  In spite of all of this vagueness, in scientific databases
    it is a decent way to group together people.
    """
    short_name =  models.CharField(max_length=50, unique=True)
    institution = models.CharField(max_length=255, null=True, blank=True)
    pi = models.ForeignKey(Contributor, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="pi")
    contributors = models.ManyToManyField(Contributor, blank=True, related_name="contributors")
    projects = models.ManyToManyField("Project", blank=True, through="project_labs")
    grants = models.ManyToManyField("Grant", blank=True, through="grant_funded_labs")
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.short_name

class Species(models.Model):
    """ This list is not meant to be inclusive.  In fact it takes something close
    to superuser status to add a new species.  Most of our data is human and mouse.
    """
    common_name = models.CharField(max_length = 40)
    scientific_name = models.CharField(max_length=150)
    ncbi_taxon = models.IntegerField(unique=True)
    def __str__(self):
       return self.common_name
    class Meta:
       verbose_name_plural = 'x species'

class Url(models.Model):
    """ These are URLs that are significant enough we want to track and describe.
    They can be associated with a project.
    """
    short_name = models.CharField(unique=True, max_length=50)
    path = models.URLField()
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.short_name
    class Meta:
       verbose_name = 'x url'

class File(models.Model):
    """ These are files that are significant enough we want to track and describe.
    They can be associated with a project.
    """
    short_name = models.CharField(unique=True, max_length=80)
    file = models.FileField(upload_to="uploads")
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.short_name
    class Meta:
       verbose_name = 'x file'

class ProjectStatus(models.Model):
    """ High level project state.  Covers wrangling through analysis and interpretation """
    status = models.CharField(max_length=30, unique=True)
    description = models.CharField(max_length=150)
    def __str__(self):
       return self.status
    class Meta:
       verbose_name = 'x project status'
       verbose_name_plural = 'x project status'

class Tag(models.Model):
    """ Some label you can tag a project with so as to group together related project
    or projects that are used for a particular analysis freeze etc
    """
    tag = models.CharField(max_length=40, unique=True)
    description = models.CharField(max_length=150)
    projects = models.ManyToManyField("Project", blank=True, through="project_tags")
    def __str__(self):
        return self.tag
    class Meta:
        verbose_name = 'x project tag'

class WranglingStatus(models.Model):
    """ State within wrangling.  Covers from primary wrangler's first sight to successful 
    submission and perhaps through multiple updates
    """
    status = models.CharField(max_length=30, unique=True)
    description = models.CharField(max_length=150)
    def __str__(self):
       return self.status
    class Meta:
       verbose_name = 'x wrangling status'
       verbose_name_plural = 'x wrangling status'

class OrganPart(models.Model):
    """ This is an informal notion of something smaller than an organ.  Typically this
    will need some curating to find the right ontology and term.
    """
    short_name = models.CharField(max_length=50, unique=True)
    ontology_id = models.CharField(max_length=32, blank=True, null=True)
    ontology_label = models.CharField(max_length=255, blank=True, null=True)
    description = models.CharField(max_length=255, blank=True)
    projects = models.ManyToManyField("Project", blank=True, through="project_organ_part")
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x organ part'

class Organ(models.Model):
    """ This is an organ or a an term of similar level from an anatomical ontology.
    The number of organs is small enough that typically the ontology_id and _label
    fields are filled in or will be shortly by the wranglers.
    """
    short_name = models.CharField(max_length=50, unique=True)
    ontology_id = models.CharField(max_length=32, blank=True, null=True)
    ontology_label = models.CharField(max_length=255, blank=True, null=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    projects = models.ManyToManyField("Project", blank=True, through="project_organ",
        related_name='projects_organ_relationship')
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x organ'
    
class Disease(models.Model):
    """ Something that describes the disease (or health) status of a sample or the 
    organism that it came from.  
    """
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    projects = models.ManyToManyField("Project", blank=True, through="project_disease", 
        related_name='projects_diseases_relationship')
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x disease'

class Consent(models.Model):
    """ This Concent class is a controlled vocabulary that describes broadly the justification
    for thinking we have the right to share the data with anyone.  This does not
    go into or include the specific consent agreement when human subjects are involved.
    """
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x consent'

class SampleType(models.Model):
    """ The SampleType is a broad classification of how close to in-vivo the tissue sample is.
    """
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x sample type'

class PreservationMethod(models.Model):
    """ The PreservationType describes how a sample was preserved before measurement """
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x preservation method'

class CdnaLibraryPrep(models.Model):
    """ CdnaLibraryPrep describes the technology used to make the cDNA library. In the single cell
    world this term has come to encompass to an extent both the device and chemistry used to
    separate single cells and turn the RNA inside of those into DNA for sequencing.  Bar codes
    to specify individual cells or individual (pre-PCR) RNA molecules may be added here.
    """
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    projects = models.ManyToManyField("Project", blank=True, 
        through="project_cdna_library_prep")
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x cDNA library prep'

class Publication(models.Model):
    """ Most often a publication is an article in a scientific journal. """
    short_name = models.CharField(max_length=50, unique=True)
    title = models.CharField(max_length=255, blank=True)
    pmid = models.CharField(max_length=16, blank=True)
    comments = models.CharField(max_length=255, blank=True)
    doi = models.CharField(max_length=32, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x publication'

class ProjectSourceType(models.Model):
    """ Distinguishes high level classes of effort such as importing data from the
    scientific literature vs. helping labs organize, understand, and share
    prepublication effort.
    """
    short_name = models.CharField(max_length=50, unique=True)
    description = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.short_name
    class Meta:
       verbose_name = 'x project source type'

class SoftwareDeveloper(models.Model):
    """ Software developers contributing to the project. """
    who = models.ForeignKey(Contributor, on_delete=models.PROTECT)
    favorite_languages = models.CharField(max_length=255)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.who.__str__()
    class Meta:
       verbose_name = 'x software developer'

class Intern(models.Model):
    """ Interns learning from the project. """
    who = models.ForeignKey(Contributor, on_delete=models.PROTECT)
    advisor = models.ForeignKey(Contributor, null=True, on_delete=models.SET_NULL, related_name='advisor')
    interests = models.CharField(max_length=128, blank=True)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.who.__str__()
    class Meta:
       verbose_name = 'x intern'

class Wrangler(models.Model):
    """ A wrangler helps get projects from lab into database where they can be shared
    with others. 
    """
    who = models.ForeignKey(Contributor, on_delete=models.PROTECT)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
        return self.who.__str__()
    class Meta:
       verbose_name = 'x wrangler'

class Project(models.Model):
    """ A project, like a lab, is a somewhat vague concept that is still useful to describe
    scientific work.  Typically a project is the work of a single lab or a small group of
    closely collaborating labs.  Some projects will be focused around a particular organ.
    Others will be focused on technologies or on different states of the same cell type.
    Very often a successful project will be associated with a publication, or a record in
    one of the public gene expression or sequencing databases.
    """
    short_name = models.CharField(max_length=80)
    wrangling_status = models.ForeignKey(WranglingStatus, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    comments = models.CharField(max_length=255, blank=True)
    status = models.ForeignKey(ProjectStatus, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="state_reached")
    stars = models.IntegerField(blank=True,validators=[MinValueValidator(1),MaxValueValidator(5)], default=3)
    tags = models.ManyToManyField(Tag, blank=True)
    git_ticket_url = models.URLField(blank=True, null=True)
    primary_wrangler = models.ForeignKey(Wrangler, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="wrangler1");
    secondary_wrangler = models.ForeignKey(Wrangler, blank=True, null=True, default=None, on_delete=models.SET_NULL, related_name="secondary_wrangler");
    contacts = models.ManyToManyField(Contributor, blank=True, related_name="projcontacts")
    first_contact_date = models.DateField(blank=True, null=True, default=None)
    last_contact_date = models.DateField(blank=True, null=True, default=None)
    questionnaire_comments = models.CharField(max_length=255, blank=True)
    questionnaire_date = models.DateField(blank=True, null=True, default=None)
    tAndC_comments = models.CharField("T&C", max_length=255, blank=True)
    tAndC_date = models.DateField("T&C date", blank=True, null=True, default=None)
    sheet_template = models.FileField(upload_to="uploads/project", blank=True, null=True, default=None)
    sheet_template_date = models.DateField(blank=True, null=True, default=None)
    sheet_from_lab = models.FileField(upload_to="uploads/project", blank=True, null=True, default=None)
    shared_google_sheet  = models.URLField(blank=True, null=True, default=None)
    sheet_from_lab_date = models.DateField(blank=True, null=True, default=None)
    back_to_lab = models.FileField(upload_to="uploads/project", blank=True, null=True, default=None)
    back_to_lab_date = models.DateField(blank=True, null=True, default=None)
    lab_review_comments = models.CharField(max_length=255, blank=True)
    lab_review_date = models.DateField(blank=True, null=True, default=None)
    sheet_submitted = models.FileField(upload_to="uploads/project", blank=True, null=True, default=None)
    sheet_validated_date = models.DateField(blank=True, null=True, default=None)
    staging_area = models.CharField('Staging S3 bucket', max_length=255,blank=True, default="")
    staging_area_date = models.DateField(blank=True, null=True, default=None)
    submit_date = models.DateField(blank=True, null=True, default=None)
    submit_comments = models.CharField(max_length=255, blank=True)
    title = models.CharField(max_length=120)
    description = models.TextField()
    labs = models.ManyToManyField(Lab, blank=True)
    organ = models.ManyToManyField(Organ, blank=True)
    organ_part = models.ManyToManyField(OrganPart, blank=True)
    disease = models.ManyToManyField(Disease, blank=True)
    sample_type = models.ManyToManyField(SampleType, blank=True)
    preservation_method = models.ManyToManyField(PreservationMethod, blank=True)
    consent = models.ForeignKey(Consent, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    project_source = models.ForeignKey(ProjectSourceType, blank=True, null=True, default=None, on_delete=models.SET_NULL)
    origin_name = models.CharField(max_length=200, blank=True)
    cdna_library_prep = models.ManyToManyField(CdnaLibraryPrep, blank=True, verbose_name=' cDNA library prep')
    cells_expected = models.IntegerField(blank=True, default=0)
    publications = models.ManyToManyField(Publication, blank=True)
    contributors = models.ManyToManyField(Contributor)
    species = models.ManyToManyField(Species, blank=True)
    grants = models.ManyToManyField("Grant", blank=True, through="grant_funded_projects")
    wrangler_drive = models.URLField(blank=True)
    urls = models.ManyToManyField(Url, blank=True)
    def __str__(self):
       return self.short_name

class Tracker(models.Model):
    """ In Tuatara most of the weight of a project is presubmission.  Happily things don't
    stop there.  The tracker class just extracts some of the easier to understand fields
    from Parth's tracker.
    """
    project = models.OneToOneField(Project, on_delete=models.CASCADE, primary_key=True)
    uuid = models.CharField(max_length=37, unique=True)
    submission_id = models.CharField(max_length=33, unique=True)
    submission_bundles_exported_count = models.IntegerField("ingest", blank=True, default=0)
    aws_primary_bundle_count = models.IntegerField("aws 1", blank=True, default=0)
    gcp_primary_bundle_count = models.IntegerField("gcp 1", blank=True, default=0)
    aws_analysis_bundle_count = models.IntegerField("asw 2", blank=True, default=0)
    gcp_analysis_bundle_count = models.IntegerField("gcp 2", blank=True, default=0)
    azul_analysis_bundle_count = models.IntegerField("azul", blank=True, default=0)
    succeeded_workflows = models.IntegerField("workflow", blank=True, default=0)
    matrix_bundle_count = models.IntegerField("matrix", blank=True, default=0)
    matrix_cell_count = models.IntegerField("cells", blank=True, default=0)
    def __str__(self):
       return self.uuid
    class Meta:
       verbose_name_plural = 'Tracking after submission'

class Funder(models.Model):
    """ It's possible that CZI will ask us to track grants, and other funders might as
    well.  This class is here as a placeholder for that need.
    """
    short_name = models.CharField(max_length=50)
    description = models.TextField()
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.short_name
    class Meta:
       verbose_name = 'x funder'


class Grant(models.Model):
    """ It's possible that CZI will ask us to track grants, and other funders might as
    well.  This class is here as a placeholder for that need.
    """
    grant_title = models.CharField(max_length=200)
    grant_id = models.CharField(max_length=200)
    funder = models.ForeignKey(Funder, null=True, default=None, on_delete=models.SET_NULL)
    funded_projects = models.ManyToManyField(Project, blank=True)
    funded_labs = models.ManyToManyField(Lab, blank=True)
    funded_contributors = models.ManyToManyField(Contributor, blank=True)
    comments = models.CharField(max_length=255, blank=True)
    def __str__(self):
       return self.grant_title
    class Meta:
       verbose_name = 'x grant'

