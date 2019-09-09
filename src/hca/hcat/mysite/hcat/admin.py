
# Register your models here.
# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.contrib import admin
from .models import *

class ContributorAdmin(admin.ModelAdmin):
    list_filter = ['project_role']
    autocomplete_fields = ['comments', 'projects', 'labs', 'grants']
    search_fields = ['name', 'project_role', 'email',]
    list_display = ['name', 'project_role', 'email', ]

admin.site.register(Contributor, ContributorAdmin)


class LabAdmin(admin.ModelAdmin):
    autocomplete_fields = ['contributors', 'pi', 'contact', 'grants', 'projects', 'comments']
    search_fields = ['short_name', 'pi', 'contact']
    list_display = ['short_name', 'institution',]

admin.site.register(Lab, LabAdmin)

class ProjectStateAdmin(admin.ModelAdmin):
    list_display = ['state', 'description']
    autocomplete_fields = ['comments']
    
admin.site.register(ProjectState, ProjectStateAdmin)

class EffortTypeAdmin(admin.ModelAdmin):
    list_display = ['short_name', 'description']
    autocomplete_fields = ['comments']
    
admin.site.register(EffortType, EffortTypeAdmin)

class ProjectAdmin(admin.ModelAdmin):
    search_fields = ['short_name', 'title', 'contributors', 'labs', 'organ_part', 
    #'organ', 
    'disease', 'species', 'grants']
    autocomplete_fields = ["contributors", "labs", 
    	"organ", "organ_part", "disease", "files",
    	"species", "sample_type", "assay_type", "assay_tech", "publications", "comments", 
	"grants", "files", "urls", "contacts", "responders", "submit_comments"]
    list_display = ['short_name', 'stars', 'state_reached', 'wrangler1', 'title',]
    list_filter = ['species', 'effort', 'state_reached', 'assay_tech']
    fieldsets = (
        ('overall', { 'fields': (('short_name', 'state_reached', ), ('title', 'stars'), ('labs', 'consent'))}), 
	('wrangling',  { 'fields': (
		('wrangler1', 'wrangler2'), 
		('cur_state', 'comments'),
		('effort', 'origin_name',),
		('contacts', 'files'),
		('first_contact_date', 'last_contact_date'),
		'responders',
		('first_response_date', 'last_response_date'),
		)}),
	('submission steps',  { 'fields': (
		('questionnaire', 'questionnaire_date'),
		('tAndC', 'tAndC_date'),
		('sheet_template', 'sheet_template_date'),
		('sheet_from_lab', 'sheet_from_lab_date'),
		('sheet_curated', 'sheet_curated_date'),
		('sheet_validated', 'sheet_validated_date'),
		('staging_area', 'staging_area_date'),
		('submit_comments', 'submit_date'),
		)}),
	('post-submit',  { 'fields': (
		('cloud_date', 'pipeline_date', 'orange_date'),
		)}),
        ('biosample',  { 'fields': (('species', 'sample_type'), ('organ', 'organ_part'), 'disease')}),
        ('assay', { 'fields': ('assay_type', 'assay_tech', 'cells_expected')}),
	('pubs, people, and pay', { 'fields': ('description', 'publications', 'contributors', 'grants')}),
    )
    def formfield_for_foreignkey(self, db_field, request, **kwargs):
       if db_field.name == "wrangler1" or db_field.name == "wrangler2":
           kwargs["queryset"] = Contributor.objects.filter(project_role="wrangler")
       return super().formfield_for_foreignkey(db_field, request, **kwargs)

admin.site.register(Project, ProjectAdmin)

class GrantAdmin(admin.ModelAdmin):
    autocomplete_fields = ["funded_contributors", "funded_projects", "funded_labs", "comments"]
    search_fields = ["funded_contributors", "funded_projects", "funded_labs"]
    list_display = ['grant_id', 'funder', 'grant_title',]
    list_filter = ['funder']

admin.site.register(Grant, GrantAdmin)

class FunderAdmin(admin.ModelAdmin):
    list_display = ['short_name', 'description',]
    autocomplete_fields = ['comments']

admin.site.register(Funder, FunderAdmin)


class SpeciesAdmin(admin.ModelAdmin):
    search_fields = ["common_name", "scientific_name", "ncbi_taxon"]
    list_display = ['common_name', 'scientific_name', 'ncbi_taxon',]

admin.site.register(Species, SpeciesAdmin)

class ConsentAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments']

admin.site.register(Consent, ConsentAdmin)

class DiseaseAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments', 'projects']

admin.site.register(Disease, DiseaseAdmin)

class OrganAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    #autocomplete_fields = ['comments', 'projects']
    autocomplete_fields = ['comments', ]

admin.site.register(Organ, OrganAdmin)

class OrganPartAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments', 'projects']


admin.site.register(OrganPart, OrganPartAdmin)

class SampleTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments']

admin.site.register(SampleType, SampleTypeAdmin)

class AssayTechAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments', 'projects']

admin.site.register(AssayTech, AssayTechAdmin)

class AssayTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments', 'projects']

admin.site.register(AssayType, AssayTypeAdmin)

class PublicationAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "title"]
    list_display = ["short_name", "title"]
    autocomplete_fields = ['comments']

admin.site.register(Publication, PublicationAdmin)

class CommentTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "description"]
    list_display = ["short_name", "description"]
    short_description = "emotion"

admin.site.register(CommentType, CommentTypeAdmin)

class CommentAdmin(admin.ModelAdmin):
    search_fields = ["type", "text"]
    list_display = ["type", "text"]
    autocomplete_fields = ['projects', 'labs', 'contributors', 'grants', 'consents', 'organs', 'organ_parts', 'urls', 'files']
    list_filter = ['type']

admin.site.register(Comment, CommentAdmin)

class UrlAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "path"]
    list_display = ["short_name", "path"]
    autocomplete_fields = ['comments']

admin.site.register(Url, UrlAdmin)

class FileAdmin(admin.ModelAdmin):
    search_fields = ["short_name", "file"]
    list_display = ["short_name", "file"]
    autocomplete_fields = ['comments']

admin.site.register(File, FileAdmin)
