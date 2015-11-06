#!/bin/sh

echo "Five seconds to shut the lid!" | osd_cat -d 4 -c skyblue -s 5 -p top -A center -f "-*-fixed-*-r-*-*-60-*-*-*-c-*-*-*"

sleep 5
sudo systemctl suspend

