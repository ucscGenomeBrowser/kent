from django.contrib import admin
from models import *

#admin.site.register(AccToKeyword)
#admin.site.register(AccToTaxon)
admin.site.register(Author)
admin.site.register(Citation)
#admin.site.register(CitationRc)
admin.site.register(CitationRp)
#admin.site.register(Comment)
admin.site.register(CommentType)
admin.site.register(CommentVal)
#admin.site.register(CommonName)
admin.site.register(Description)

class FeatureInline(admin.StackedInline):
    model = Feature
    
class DisplayIdAdmin(admin.ModelAdmin):
    inlines = [FeatureInline]
    pass

admin.site.register(DisplayId, DisplayIdAdmin)

admin.site.register(ExtDb)
#admin.site.register(ExtDbRef)

class FeatureAdmin(admin.ModelAdmin):
    readonly_fields = ["acc"]
    raw_id_fields = ["featureType", "featureId"]

admin.site.register(Feature, FeatureAdmin)

admin.site.register(FeatureClass)
admin.site.register(FeatureId)
admin.site.register(FeatureType)
#admin.site.register(Gene)
#admin.site.register(GeneLogic)
admin.site.register(Info)
admin.site.register(Keyword)
admin.site.register(Organelle)
#admin.site.register(OtherAcc)
#admin.site.register(PathogenHost)
admin.site.register(Protein)
#admin.site.register(ProteinEvidence)
admin.site.register(ProteinEvidenceType)
admin.site.register(RcType)
admin.site.register(RcVal)
admin.site.register(Reference)
#admin.site.register(ReferenceAuthors)
admin.site.register(Taxon)
admin.site.register(VarAcc)
admin.site.register(VarProtein)
