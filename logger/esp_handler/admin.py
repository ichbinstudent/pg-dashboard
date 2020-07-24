from django.contrib import admin
from .models import PersonEvent, WifiDevice

@admin.register(PersonEvent)
class PersonEventAdmin(admin.ModelAdmin):
    date_hierarchy = 'timestamp'
    list_display = ('timestamp', 'direction')

@admin.register(WifiDevice)
class WifiDeviceAdmin(admin.ModelAdmin):
    date_hierarchy = 'timestamp'
    list_display = ('timestamp', 'bssid', 'ap_ssid')
