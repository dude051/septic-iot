#!/bin/bash
mos put ~/.aws/${AWS_THING_CERT} &&
mos put ~/.aws/${AWS_THING_KEY} &&
mos config-set \
wifi.sta.enable=true \
wifi.ap.enable=false \
wifi.sta.ssid="${WIFI_SSID}" \
wifi.sta.pass="${WIFI_PASSWORD}" \
aws.thing_name="${AWS_THING}" \
mqtt.server="${AWS_IOT_ENDPOINT}" \
mqtt.ssl_ca_cert="ca.pem" \
mqtt.ssl_cert="${AWS_THING_CERT}" \
mqtt.ssl_key="${AWS_THING_KEY}" \
mqtt.enable="true" \
sys.tz_spec="CST6CDT5,M3.2.0/02:00:00,M11.1.0/02:00:00"
mos console
