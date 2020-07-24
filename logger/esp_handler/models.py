from django.db import models
from django.utils.timezone import localtime


class PersonEvent(models.Model):
    timestamp = models.DateTimeField(auto_now_add=True)
    direction = models.CharField(max_length=10)


class WifiDevice(models.Model):
    id = models.AutoField(primary_key=True)

    timestamp = models.DateTimeField(auto_now_add=True)

    bssid = models.CharField(max_length=128)
    ap_ssid = models.CharField(max_length=128)
    vendor = models.CharField(max_length=128)
