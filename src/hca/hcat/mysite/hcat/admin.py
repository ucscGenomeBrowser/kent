
# Register your models here.
# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.contrib import admin
from .models import Grant,Project,Lab,Contributor,VocabFunder,VocabProjectState,VocabConsent,Species,VocabOrgan,VocabOrganPart,VocabDisease,VocabSampleType,VocabAssayTech,VocabAssayType,VocabPublication,VocabComment,VocabCommentType,VocabProjectOrigin

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

class VocabProjectStateAdmin(admin.ModelAdmin):
    list_display = ['state', 'description']
    autocomplete_fields = ['comments']
    
admin.site.register(VocabProjectState, VocabProjectStateAdmin)

class VocabProjectOriginAdmin(admin.ModelAdmin):
    list_display = ['short_name', 'description']
    autocomplete_fields = ['comments']
    
admin.site.register(VocabProjectOrigin, VocabProjectOriginAdmin)

class ProjectAdmin(admin.ModelAdmin):
    search_fields = ['short_name', 'title', 'contributors', 'labs', 'organ_part', 'organ', 'disease', 'species']
    autocomplete_fields = ["contributors", "labs", "organ", "organ_part", "disease", "species", "sample_type", "assay_type", "assay_tech", "publications", "comments", "grants"]
    list_display = ['short_name', 'stars', 'state', 'wrangler1', 'title',]
    list_filter = ['species', 'origin', 'state', 'assay_tech']
    fieldsets = (
        ('overall', { 'fields': ('short_name', 'title', 'description','publications', )}),
        ('wrangling',  { 'fields': ('state', 'consent', 'stars', 'wrangler1', 'wrangler2', 'contributors', 'labs', 'grants', 'comments', 'origin')}),
        ('biosample',  { 'fields': ('species', 'sample_type', 'organ', 'organ_part', 'disease')}),
        ('assay', { 'fields': ('assay_type', 'assay_tech', 'cells_expected')}),
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

class VocabFunderAdmin(admin.ModelAdmin):
    list_display = ['short_name', 'description',]
    autocomplete_fields = ['comments']

admin.site.register(VocabFunder, VocabFunderAdmin)


class SpeciesAdmin(admin.ModelAdmin):
    search_fields = ["common_name"]
    list_display = ['common_name', 'scientific_name', 'ncbi_taxon',]

admin.site.register(Species, SpeciesAdmin)

class VocabConsentAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments']

admin.site.register(VocabConsent, VocabConsentAdmin)

class VocabDiseaseAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments']

admin.site.register(VocabDisease, VocabDiseaseAdmin)

class VocabOrganAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments']

admin.site.register(VocabOrgan, VocabOrganAdmin)

class VocabOrganPartAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments']


admin.site.register(VocabOrganPart, VocabOrganPartAdmin)

class VocabSampleTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments']

admin.site.register(VocabSampleType, VocabSampleTypeAdmin)

class VocabAssayTechAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments']

admin.site.register(VocabAssayTech, VocabAssayTechAdmin)

class VocabAssayTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]
    autocomplete_fields = ['comments']

admin.site.register(VocabAssayType, VocabAssayTypeAdmin)

class VocabPublicationAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "title"]
    autocomplete_fields = ['comments']

admin.site.register(VocabPublication, VocabPublicationAdmin)

class VocabCommentTypeAdmin(admin.ModelAdmin):
    search_fields = ["short_name"]
    list_display = ["short_name", "description"]
    short_description = "emotion"

admin.site.register(VocabCommentType, VocabCommentTypeAdmin)

class VocabCommentAdmin(admin.ModelAdmin):
    search_fields = ["type", "text"]
    list_display = ["type", "text"]

admin.site.register(VocabComment, VocabCommentAdmin)

