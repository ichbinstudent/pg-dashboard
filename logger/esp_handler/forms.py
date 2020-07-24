from datetime import timedelta, datetime
from django import forms
import time

class FilterForm(forms.Form):
    start_time = forms.DateTimeField(label='Start Time', initial=datetime.now() + timedelta(days=-1), required=False)
    end_time = forms.DateTimeField(label='End Time', initial=datetime.now(), required=False)
    bssid = forms.CharField(initial='', empty_value='', label='Device Hash', max_length=64, required=False)
    vendor = forms.CharField(initial='', empty_value='', label='Vendor', max_length=64, required=False)
    ap_ssid = forms.CharField(initial='', empty_value='', label='AP-SSID', max_length=64, required=False)
