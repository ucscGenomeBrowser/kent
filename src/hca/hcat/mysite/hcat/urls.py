from django.urls import path

from . import views

app_name = 'hcat'
urlpatterns = [
    path('', views.index, name='index'),
    path('api', views.api_index, name='api_index'),
    path('api/project', views.api_project_list, name='api_project_list'),
    path('api/projectstate', views.api_projectstate_list, name='api_projectstate_list'),
    path('api/assaytech', views.api_assaytech_list, name='api_assaytech_list'),
    path('api/contributor', views.api_contributor_list, name='api_contributor_list'),
    path('api/disease', views.api_disease_list, name='api_disease_list'),
    path('api/organ', views.api_organ_list, name='api_organ_list'),
    path('api/tracker', views.api_tracker_list, name='api_tracker_list'),
    path('project/<int:pk>/', views.ProjectDetailView.as_view(), name='project_detail'),
    path('project', views.ProjectListView.as_view(), name='project_list'),
]

