#!/bin/bash
mos1 ota https://s3.amazonaws.com/septic-iot/fw.zip && \
sleep 300 && \
mos1 call OTA.Commit
