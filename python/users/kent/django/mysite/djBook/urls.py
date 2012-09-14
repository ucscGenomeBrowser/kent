from django.conf.urls.defaults import patterns, url
from views import current_datetime,hours_ahead

urlpatterns = patterns('',
    url(r'^time/$', current_datetime),
    url(r'^time/(\d{1,2})/$', hours_ahead),
    )



