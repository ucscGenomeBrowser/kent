
# Register your models here.
# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.contrib import admin
from .models import *

class ContributorTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]

admin.site.register(ContributorType, ContributorTypeAdmin)

class ContributorAdmin(admin.ModelAdmin):
    list_filter = ['type']
    autocomplete_fields = ['projects', 'labs', 'grants', 'type']
    search_fields = ['name', 'email','city']
    list_display = ['name', 'email', 'type', 'city']

admin.site.register(Contributor, ContributorAdmin)

class SoftwareDeveloperAdmin(admin.ModelAdmin):
    autocomplete_fields = ['who']
    list_display = ['who', 'favorite_languages']

admin.site.register(SoftwareDeveloper, SoftwareDeveloperAdmin)

class InternAdmin(admin.ModelAdmin):
    autocomplete_fields = ['who']
    list_display = ['who', 'advisor', 'interests']

admin.site.register(Intern, InternAdmin)

class WranglerAdmin(admin.ModelAdmin):
    autocomplete_fields = ['who']
    list_display = ['who', ]

admin.site.register(Wrangler, WranglerAdmin)

class LabAdmin(admin.ModelAdmin):
    autocomplete_fields = ['contributors', 'pi', 'grants', 'projects']
    search_fields = ['short_name', 'institution', ]
    list_display = ['short_name', 'pi', 'institution',]

admin.site.register(Lab, LabAdmin)

class ProjectStatusAdmin(admin.ModelAdmin):
    list_display = ['status', 'description']
    
admin.site.register(ProjectStatus, ProjectStatusAdmin)

class WranglingStatusAdmin(admin.ModelAdmin):
    list_display = ['status', 'description']
    
admin.site.register(WranglingStatus, WranglingStatusAdmin)

class EffortTypeAdmin(admin.ModelAdmin):
    list_display = ['short_name', 'description']
    
admin.site.register(EffortType, EffortTypeAdmin)

class TrackerAdmin(admin.ModelAdmin):
    readonly_fields = ['project','uuid','submission_id', 'submission_bundles_exported_count',
        'aws_primary_bundle_count', 'gcp_primary_bundle_count',
        'aws_analysis_bundle_count', 'gcp_analysis_bundle_count',
        'azul_analysis_bundle_count', 'succeeded_workflows', 
        'matrix_bundle_count', 'matrix_cell_count']
    list_display = ['project', 'submission_bundles_exported_count', 
        'aws_primary_bundle_count', 'gcp_primary_bundle_count', 
        'aws_analysis_bundle_count', 'gcp_analysis_bundle_count', 
        'azul_analysis_bundle_count', 'succeeded_workflows',
        'matrix_bundle_count', 'matrix_cell_count']

admin.site.register(Tracker, TrackerAdmin)


class TrackerInline(admin.TabularInline):
    model = Tracker
    verbose_name_plural = 'Post-submission tracking - bundles and cells'
    fields = ['submission_bundles_exported_count', 'aws_primary_bundle_count', 
        'aws_analysis_bundle_count', 'azul_analysis_bundle_count', 
        'succeeded_workflows', 'matrix_bundle_count', 'matrix_cell_count']
    readonly_fields = ['submission_bundles_exported_count', 'aws_primary_bundle_count', 
        'aws_analysis_bundle_count', 'azul_analysis_bundle_count', 
        'succeeded_workflows', 'matrix_bundle_count', 'matrix_cell_count']
    can_delete = False

class ProjectAdmin(admin.ModelAdmin):
    search_fields = ['short_name', 'title']
    autocomplete_fields = ["contributors", "labs", 
    	"organ", "organ_part", "disease", "files",
    	"species", "sample_type", "assay_tech", "publications", 
	"grants", "files", "urls", "contacts", ]
    list_display = ['short_name', 'stars', 'status', 'primary_wrangler', 'submit_date',]
    list_filter = ['species', 'effort', 'primary_wrangler', 'status', 'assay_tech']
    inlines = [TrackerInline,]
    fieldsets = (
        ('overall', { 'fields': (('short_name', 'status', ), ('title', 'stars'), ('labs', 'consent'), ('git_ticket_url'))}), 
        ('biosample',  { 'fields': (('species', 'sample_type'), ('organ', 'organ_part'), 'disease')}),
        ('assay', { 'fields': (('assay_tech', 'cells_expected'))}),
	('pubs, people, and pay', { 'fields': ('description', 'publications', 'contributors', 'grants')}),
	('wrangling',  { 'fields': (
		('primary_wrangler', 'secondary_wrangler'), 
		('wrangling_status', 'comments'),
		('effort', 'origin_name',),
		('contacts', 'files'),
		('first_contact_date', 'last_contact_date'),
		)}),
	('submission steps',  { 'fields': (
		('questionnaire_date', 'questionnaire_comments'),
		('tAndC_date', 'tAndC_comments'),
		('sheet_template_date', 'sheet_template', ),
                ('shared_google_sheet'),
		('sheet_from_lab_date', 'sheet_from_lab', ),
		('back_to_lab_date', 'back_to_lab', ),
                ('lab_review_date', 'lab_review_comments', ),
		('sheet_validated_date', 'sheet_that_validated', ),
		('staging_area_date', 'staging_area', ),
		('submit_date', 'submit_comments', ),
		)}),
    )

admin.site.register(Project, ProjectAdmin)

class GrantAdmin(admin.ModelAdmin):
    autocomplete_fields = ["funded_contributors", "funded_projects", "funded_labs", ]
    search_fields = ['grant_id','grant_title']
    list_display = ['grant_id', 'funder', 'grant_title',]
    list_filter = ['funder']

admin.site.register(Grant, GrantAdmin)

class FunderAdmin(admin.ModelAdmin):
    list_display = ['short_name', 'description',]

admin.site.register(Funder, FunderAdmin)


class SpeciesAdmin(admin.ModelAdmin):
    search_fields = ["common_name", "scientific_name", "ncbi_taxon"]
    list_display = ['common_name', 'scientific_name', 'ncbi_taxon',]

admin.site.register(Species, SpeciesAdmin)

class ConsentAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]

admin.site.register(Consent, ConsentAdmin)

class DiseaseAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['projects']

admin.site.register(Disease, DiseaseAdmin)

class OrganAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description", "ontology_id"]
    list_display = ["short_name", "ontology_id", "description"]

admin.site.register(Organ, OrganAdmin)

class OrganPartAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['projects']


admin.site.register(OrganPart, OrganPartAdmin)

class SampleTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]

admin.site.register(SampleType, SampleTypeAdmin)

class AssayTechAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['projects']

admin.site.register(AssayTech, AssayTechAdmin)

class PublicationAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "title"]
    list_display = ["short_name", "title"]

admin.site.register(Publication, PublicationAdmin)

class UrlAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "path"]
    list_display = ["short_name", "path"]

admin.site.register(Url, UrlAdmin)

class FileAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "file"]
    list_display = ["short_name", "file"]

admin.site.register(File, FileAdmin)
