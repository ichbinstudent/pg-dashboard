# Generated by Django 2.2.9 on 2020-02-21 16:50

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('esp_handler', '0002_auto_20200119_1844'),
    ]

    operations = [
        migrations.AlterField(
            model_name='wifidevice',
            name='bssid',
            field=models.CharField(max_length=256),
        ),
    ]