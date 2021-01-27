
from django.http import HttpResponse
from django.shortcuts import get_object_or_404
import json
from .models import *

def api_index(request):
    objects = [Project, ProjectStatus, Contributor, Tracker, CdnaLibraryPrep, Disease, Organ]
    a = []
    for o in objects:
        p = {"class": o.__name__.lower(), "count": o.objects.count()}
        a.append(p)
    return HttpResponse(json.dumps(a), content_type="application/json")

def serializable_cdna_library_prep(c):
    projects = []
    for p in c.projects.all():
         projects.append(p.short_name)
    return {"short_name": c.short_name, "description": c.description, "projects": projects}

def api_cdnalibraryprep_list(request):
    a = []
    for p in CdnaLibraryPrep.objects.order_by("id"):
        j = serializable_cdna_library_prep(p)
        a.append(j)
    return HttpResponse(json.dumps(a), content_type="application/json")

def serializable_project_status(p):
    return {"status": p.status, "description": p.description}

def api_projectstatus_list(request):
    a = []
    for p in ProjectStatus.objects.order_by("id"):
        j = serializable_project_status(p)
        a.append(j)
    return HttpResponse(json.dumps(a), content_type="application/json")

def serializable_contributor(c):
    projects = []
    for p in c.projects.all():
         projects.append(p.short_name)
    #labs = []
    #for p in c.labs.all():
    #     labs.append(p.short_name)
    return {"name": c.name, "projects":projects}

def api_contributor_list(request):
    a = []
    for p in Contributor.objects.order_by("name"):
        c = serializable_contributor(p)
        a.append(c)
    return HttpResponse(json.dumps(a), content_type="application/json")

def serializable_organ(c):
    projects = []
    for p in c.projects.all():
         projects.append(p.short_name)
    return {"short_name": c.short_name, "description": c.description, 
        "ontology_id":c.ontology_id, "ontology_label":c.ontology_label,"projects":projects}

def api_organ_list(request):
    a = []
    for p in Organ.objects.order_by("short_name"):
        c = serializable_organ(p)
        a.append(c)
    return HttpResponse(json.dumps(a), content_type="application/json")

def api_organ_detail(request,short_name):
   c = get_object_or_404(Organ, short_name=short_name)
   return HttpResponse(json.dumps(serializable_organ(c)), content_type="application/json")

def serializable_tracker(c):
    return {
        "project": str(c.project),
        "uuid": c.uuid, "submission_id": c.submission_id, 
        "submission_bundles_exported_count":c.submission_bundles_exported_count, 
        "aws_primary_bundle_count":c.aws_primary_bundle_count,
        "gcp_primary_bundle_count":c.gcp_primary_bundle_count,
        "aws_analysis_bundle_count":c.aws_analysis_bundle_count,
        "gcp_analysis_bundle_count":c.gcp_analysis_bundle_count,
        "azul_analysis_bundle_count":c.azul_analysis_bundle_count,
        "succeeded_workflows":c.succeeded_workflows,
        "matrix_bundle_count":c.matrix_bundle_count,
        "matrix_cell_count":c.matrix_cell_count,
        }

def api_tracker_list(request):
    a = []
    for p in Tracker.objects.order_by("-project_id"):
        c = serializable_tracker(p)
        a.append(c)
    return HttpResponse(json.dumps(a), content_type="application/json")

def serializable_disease(c):
    projects = []
    for p in c.projects.all():
         projects.append(p.short_name)
    return {"short_name": c.short_name, "description": c.description, "projects":projects}

def api_disease_list(request):
    a = []
    for p in Disease.objects.order_by("short_name"):
        c = serializable_disease(p)
        a.append(c)
    return HttpResponse(json.dumps(a), content_type="application/json")

def serializable_project(p):
    contributors = []
    for c in p.contributors.all():
        contributors.append(str(c))
    contacts = []
    for c in p.contacts.all():
        contacts.append(str(c))
    organs = []
    for o in p.organ.all():
        organs.append(str(o))
    sample_type = []
    for s in p.sample_type.all():
        sample_type.append(str(s))
    species = []
    for s in p.species.all():
        species.append(str(s))
    preps = []
    for t in p.cdna_library_prep.all():
        preps.append(str(t))
    return {
        "short_name":p.short_name, "stars":p.stars, "status": str(p.status), 
        "origin_name": p.origin_name, "title":p.title, 
        "primary_wrangler": str(p.primary_wrangler), "secondary_wrangler": str(p.secondary_wrangler), "contacts":contacts,
        "species":species, "organs":organs, "sample_type":sample_type,
        "cdna_library_prep": preps, "contributors":contributors,
        "description":p.description, "comments":p.comments, "submit_date":str(p.submit_date)}
    
def api_project_list(request):
    a = []
    for p in Project.objects.order_by("-id"):
        j = serializable_project(p)
        a.append(j)
    return HttpResponse(json.dumps(a), content_type="application/json")


def api_project_detail(request,short_name):
   c = get_object_or_404(Project, short_name=short_name)
   return HttpResponse(json.dumps(serializable_project(c)), content_type="application/json")
