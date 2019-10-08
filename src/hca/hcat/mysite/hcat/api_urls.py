from django.urls import path

from . import api

app_name = 'api'
urlpatterns = [
    path('', api.api_index, name='api_index'),
    path('project', api.api_project_list, name='api_project_list'),
    path('project/', api.api_project_list, name='api_project_list'),
    path('project/<str:short_name>', api.api_project_detail, name='api_project_detail'),
    path('projectstatus', api.api_projectstatus_list, name='api_projectstatus_list'),
    path('cdnalibraryprep', api.api_cdnalibraryprep_list, name='api_cdnalibraryprep_list'),
    path('contributor', api.api_contributor_list, name='api_contributor_list'),
    path('disease', api.api_disease_list, name='api_disease_list'),
    path('organ', api.api_organ_list, name='api_organ_list'),
    path('organ/', api.api_organ_list, name='api_organ_list'),
    path('organ/<str:short_name>', api.api_organ_detail, name='api_organ_detail'),
    path('tracker', api.api_tracker_list, name='api_tracker_list'),
]

