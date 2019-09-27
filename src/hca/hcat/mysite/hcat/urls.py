from django.urls import path

from . import views
from . import api

app_name = 'hcat'
urlpatterns = [
    path('', views.index, name='index'),
    path('project/<int:pk>/', views.ProjectDetailView.as_view(), name='project_detail'),
    path('project', views.ProjectListView.as_view(), name='project_list'),
    path('project/', views.ProjectListView.as_view(), name='project_list'),
]

