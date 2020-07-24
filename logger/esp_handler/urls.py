from django.http import HttpResponse
from django.urls import path
from . import views

urlpatterns = [
    path('log_event/<str:direction>/', views.log_event),
    path('log_event/', views.log_event),
    path('submit_devices', views.log_devices),
    path('wifi_logs/', views.view_wifi_logs),
    path('dashboard/', views.view_dashboard, name='dashboard'),
]
