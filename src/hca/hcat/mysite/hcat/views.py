from django.template import loader
from django.shortcuts import render,get_object_or_404
from django.urls import reverse
from django.views import generic
from django.http import HttpResponse
import json

from .models import *

# Create your views here (except API views go in api.py)

def index(request):
    template = loader.get_template('hcat/index.html')
    context = {}
    return HttpResponse(template.render(context,request))

class ProjectListView(generic.ListView):
    template_name = 'hcat/project_list.html'
    context_object_name = 'project_list'

    def get_queryset(self):
        return Project.objects.order_by("-id")[:100]
    
class ProjectDetailView(generic.DetailView):
    model=Project
    template_name = 'hcat/project_detail.html'
    context_object_name = 'project'

