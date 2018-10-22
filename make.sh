#!/bin/bash
mos build --platform esp8266 --local --verbose
if [ $? -eq 0 ];then
  aws s3 cp --acl public-read build/fw.zip s3://septic-iot/
fi
