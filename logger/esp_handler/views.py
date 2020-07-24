import hashlib
import os
from datetime import timedelta, datetime
import time

from django.http import HttpResponse, HttpResponseRedirect
from django.shortcuts import render, redirect
from django.utils.timezone import localtime
from django.views.decorators.http import require_http_methods

from .models import PersonEvent, WifiDevice
from .forms import FilterForm
from django.views.decorators.csrf import csrf_exempt
from django.contrib.auth.decorators import login_required, permission_required
from django.db import connections
import json

DEPLOYED = os.name == 'posix'


def dictfetchall(cursor):
    "Return all rows from a cursor as a dict"
    columns = [col[0] for col in cursor.description]
    return [
        dict(zip(columns, row))
        for row in cursor.fetchall()
    ]


def dbSize():
    try:
        with connections['default'].cursor() as cursor:
            cursor.execute("SELECT pg_size_pretty(pg_database_size('myproject'));")
            return dictfetchall(cursor)[0]['pg_size_pretty']
    except:
        return 0


def log_event(request, direction=None):
    if not (direction is None):
        if direction in ['in', 'out']:
            pe = PersonEvent()
            pe.direction = direction
            pe.save()
            return HttpResponse("OK", status=200)
        else:
            return HttpResponse('Only the parameters "in" and "out" are allowed."', status=500)
    else:
        return HttpResponse('You need to specify a direction (in/out) e.g. "log_event/in".', status=500)


vendors = {}
with open("macaddress.io-db.json", "r", encoding="utf8") as vendorfile:
    vendorslist = [json.loads(line) for line in vendorfile.readlines()]
    for v in vendorslist:
        vendors[v['oui'].replace(':', '')[:5]] = v['companyName']

BORING_RSSID = [
    # 'C8D7194C4342',
    # 'C8D719407BC7',
    # '00306EFB65AD',
    # 'C80E146498BD',
    # 'CCCE1E7DBF1C',
    # 'E8DF705CD81D',
    # 'C80CC8A7D7A2',
    # 'C80CC8A7D798',
    # 'F8ADCB01B7FB',
    # '0E00ABCD1457',
]

BORING_VENDORS = [
    'Avm Audiovisuelles Marketing und Computersysteme GmbH',    # Router
    'Actions Microelectronics Co, Ltd',
    'D-Link International',
    'FQ Ingenieria Electronica S.A.',
    'Ubiquiti Networks Inc',
    'Liteon Tech Corp',
    'Cisco-Linksys, Llc',
    'Penta Media Co, Ltd',
]


@csrf_exempt
@require_http_methods(['POST'])
def log_devices(request):
    if request.POST is not None:
        print(request.POST['data'])
        lastcomma = request.POST['data'].rfind(',')
        json_str = request.POST['data'][:lastcomma] + request.POST['data'][lastcomma + 1:]
        json_data = json.loads(json_str)
        print("{0} devices found.".format(len(json_data['devices'])))
        devices = json_data['devices']
        created = {}
        for device in devices:
            # if device['bssid'] in BORING_RSSID:
            #     print("skipping because of Boring Rssid")
            #     continue
            #
            vendor = vendors[device['bssid'][:5]] if device['bssid'][:5] in vendors else "Not Found"
            print(device['ssid'], hashlib.sha256(device['bssid'].encode('utf-8')).hexdigest(), vendor)
            if vendor not in BORING_VENDORS:
                d = WifiDevice(
                    ap_ssid=device['ssid'],
                    bssid=hashlib.sha256(device['bssid'].encode('utf-8')).hexdigest(),
                    vendor=vendor)
                d.save()
                created[d] = True
                print("Created " + vendor)

        print("Created {0} Objects".format(len(created)))
        return HttpResponse("OK", status=200)
    else:
        return HttpResponse('Only POST allowed.', status=500)


def getFilteredDevices(request):
    devices = []
    if 'start_time' not in request.GET or 'end_time' not in request.GET:
        return WifiDevice.objects.filter(timestamp__gte=localtime() - timedelta(hours=24)).exclude(vendor__in=BORING_VENDORS).order_by(
            'timestamp').reverse()

    form = FilterForm(request.GET)
    # check whether form it's valid:
    if form.is_valid():
        devices = WifiDevice.objects.filter(
            timestamp__gte=form.cleaned_data['start_time'],
            timestamp__lt=form.cleaned_data['end_time'],
            bssid__contains=form.cleaned_data['bssid'],
            ap_ssid__contains=form.cleaned_data['ap_ssid'],
            vendor__contains=form.cleaned_data['vendor'],
        ).order_by('timestamp').reverse()

    return devices.exclude(vendor__in=BORING_VENDORS)


@login_required(login_url='login')
def view_dashboard(request):
    form = FilterForm(request.GET) if len(request.GET) != 0 else FilterForm()
    devices = getFilteredDevices(request)

    if len(devices) >= 1:
        time_intervals = []
        last_time = devices.first().timestamp
        first_time = devices.last().timestamp
        day_index = 0
        year = (last_time - timedelta(days=day_index)).year
        month = (last_time - timedelta(days=day_index)).month
        day = (last_time - timedelta(days=day_index)).day

        while year != first_time.year or month != first_time.month or day != first_time.day:
            time_intervals.append([
                datetime(year, month, day, 0, 0, 0, 0, localtime().tzinfo),
                datetime(year, month, day, 23, 59, 59, 999, localtime().tzinfo)
            ])

            day_index += 1
            year = (last_time - timedelta(days=day_index)).year
            month = (last_time - timedelta(days=day_index)).month
            day = (last_time - timedelta(days=day_index)).day

        time_intervals.append([
            datetime(year, month, day, 0, 0, 0, 0, localtime().tzinfo),
            datetime(year, month, day, 23, 59, 59, 999, localtime().tzinfo)
        ])

        active_history = {
            'labels': ["{:%d.%m.%y}".format(x[0]) for x in time_intervals],
            'data': [],
        }

        for time_interval in time_intervals:
            if os.name == 'posix':
                d = WifiDevice.objects.filter(
                    timestamp__gte=time_interval[0],
                    timestamp__lt=time_interval[1]).exclude(
                    bssid__in=BORING_RSSID).exclude(vendor__in=BORING_VENDORS).distinct('bssid')
                active_history['data'].append(len(d))
            else:
                unique = {}
                for d in WifiDevice.objects.filter(
                        timestamp__gte=time_interval[0],
                        timestamp__lt=time_interval[1]).exclude(
                    bssid__in=BORING_RSSID).exclude(vendor__in=BORING_VENDORS):
                    unique[d.bssid] = 0

                active_history['data'].append(str(len(unique)))

        # Daily History
        time_intervals = []
        last_time = devices.first().timestamp
        hour_index = 0

        for hour_index in range(24):
            year = (last_time - timedelta(hours=hour_index)).year
            month = (last_time - timedelta(hours=hour_index)).month
            day = (last_time - timedelta(hours=hour_index)).day
            hour = (last_time - timedelta(hours=hour_index)).hour

            time_intervals.append([
                datetime(year, month, day, hour, 0, 0, 0, localtime().tzinfo),
                datetime(year, month, day, hour, 59, 59, 999, localtime().tzinfo)
            ])

        hourly_active_history = {
            'labels': ["{:%H:%M}".format(x[0]) for x in time_intervals],
            'data': [],
        }

        for time_interval in time_intervals:
            if os.name == 'posix':
                d = WifiDevice.objects.filter(
                    timestamp__gte=time_interval[0],
                    timestamp__lt=time_interval[1]).exclude(
                    bssid__in=BORING_RSSID).exclude(
                    vendor__in=BORING_VENDORS).distinct('bssid')
                hourly_active_history['data'].append(len(d))
            else:
                unique = {}
                for d in WifiDevice.objects.filter(
                        timestamp__gte=time_interval[0],
                        timestamp__lt=time_interval[1]).exclude(
                    bssid__in=BORING_RSSID).exclude(vendor__in=BORING_VENDORS):
                    unique[d.bssid] = 0
                hourly_active_history['data'].append(len(unique))

    else:
        active_history = {
            'labels': [],
            'data': [],
        }

        hourly_active_history = {
            'labels': [],
            'data': [],
        }

    active_history['data'].reverse()
    active_history['labels'].reverse()
    hourly_active_history['data'].reverse()
    hourly_active_history['labels'].reverse()

    return render(request, 'esp_handler/dashboard.html',
                  {
                      'devices': devices,
                      'form': form,
                      'weekly_history': active_history,
                      'hourly_history': hourly_active_history,
                      'dbsize': dbSize()
                  })


@login_required(login_url='login')
@permission_required('admin')
def view_wifi_logs(request):
    stats = []

    # create a form instance and populate it with data from the request:
    form = FilterForm(request.GET) if len(request.GET) != 0 else FilterForm()
    # check whether it's valid:
    devices = getFilteredDevices(request)

    d = WifiDevice.objects.filter(timestamp__gte=localtime() + timedelta(minutes=-10)).exclude(
        bssid__in=BORING_RSSID).exclude(vendor__in=BORING_VENDORS).distinct('bssid')
    stats.append({'text': 'Currently Active Devices', 'value': len(d)})

    return render(request, 'esp_handler/wifidevices.html',
                  {
                      'devices': devices,
                      'form': form,
                      'stats': stats,
                      'dbsize': dbSize()
                  })
