
# Register your models here.
# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.contrib import admin
from .models import Grant,Project,Lab,Contributor,Funder,ProjectState,Consent,Species,Organ,OrganPart,Disease,SampleType,AssayType,AssayTech,Publication

class ContributorAdmin(admin.ModelAdmin):
    list_filter = ['project_role']
    search_fields = ['name', 'project_role', 'email',]
    list_display = ['name', 'project_role', 'email', ]

admin.site.register(Contributor, ContributorAdmin)


class LabAdmin(admin.ModelAdmin):
    autocomplete_fields = ['contributors', 'pi', 'contact']
    search_fields = ['short_name', 'pi', 'contact']
    list_display = ['short_name', 'institution',]

admin.site.register(Lab, LabAdmin)

class ProjectStateAdmin(admin.ModelAdmin):
    list_display = ['state', 'description']
    
admin.site.register(ProjectState, ProjectStateAdmin)

class ProjectAdmin(admin.ModelAdmin):
    search_fields = ['short_name', 'title', 'contributors', 'labs', 'organ_part', 'organ', 'disease', 'species']
    autocomplete_fields = ["contributors", "labs", "organ", "organ_part", "disease", "species", "sample_type", "assay_type", "assay_tech", "publications"]
    list_display = ['short_name', 'score', 'state', 'wrangler1', 'title',]
    list_filter = ['species', 'organ', 'state', 'assay_tech']
    fieldsets = (
        ('overall', { 'fields': ('short_name', 'title', 'description','publications')}),
        ('wrangling',  { 'fields': ('state', 'consent', 'score', 'wrangler1', 'wrangler2', 'contributors')}),
        ('biosample',  { 'fields': ('species', 'sample_type', 'organ', 'organ_part', 'disease')}),
        ('assay', { 'fields': ('assay_type', 'assay_tech', 'cells_expected')}),
    )
    def formfield_for_foreignkey(self, db_field, request, **kwargs):
       if db_field.name == "wrangler1" or db_field.name == "wrangler2":
           kwargs["queryset"] = Contributor.objects.filter(project_role="wrangler")
       return super().formfield_for_foreignkey(db_field, request, **kwargs)

admin.site.register(Project, ProjectAdmin)

class GrantAdmin(admin.ModelAdmin):
    autocomplete_fields = ["funded_contributors", "funded_projects", "funded_labs"]
    search_fields = ["funded_contributors", "funded_projects", "funded_labs"]
    list_display = ['grant_id', 'funder', 'grant_title',]
    list_filter = ['funder']

admin.site.register(Grant, GrantAdmin)

class FunderAdmin(admin.ModelAdmin):
    list_display = ['short_name', 'description',]

admin.site.register(Funder, FunderAdmin)


class SpeciesAdmin(admin.ModelAdmin):
    search_fields = ["common_name"]
    list_display = ['common_name', 'scientific_name', 'ncbi_taxon',]

admin.site.register(Species, SpeciesAdmin)

class ConsentAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]

admin.site.register(Consent, ConsentAdmin)

class DiseaseAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]

admin.site.register(Disease, DiseaseAdmin)

class OrganAdmin(admin.ModelAdmin):
     search_fields = ["short_name"]

admin.site.register(Organ, OrganAdmin)

class OrganPartAdmin(admin.ModelAdmin):
     search_fields = ["short_name"]

admin.site.register(OrganPart, OrganPartAdmin)

class SampleTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]

admin.site.register(SampleType, SampleTypeAdmin)

class AssayTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]

admin.site.register(AssayType, AssayTypeAdmin)

class AssayTechAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]

admin.site.register(AssayTech, AssayTechAdmin)

class PublicationAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "title"]

admin.site.register(Publication, PublicationAdmin)

