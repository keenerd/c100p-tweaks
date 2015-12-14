#!/bin/bash

active=$(pgrep osd_cat)
if [[ "$active" != "" ]]; then
    kill $active
    exit
fi

show_time=5

read -r plug_status < /sys/class/power_supply/bq27500-0/status
if [[ "$plug_status" == "Charging" ]]; then
    read -r time_now < /sys/class/power_supply/bq27500-0/time_to_full_now
else
    read -r time_now < /sys/class/power_supply/bq27500-0/time_to_empty_now
fi
time_now=$(dc -e "2 k $time_now 60 / 60 / p")
read -r capacity < /sys/class/power_supply/bq27500-0/capacity
read -r voltage < /sys/class/power_supply/bq27500-0/voltage_now
read -r current < /sys/class/power_supply/bq27500-0/current_now
current=${current//-/_}  # make DC happy
wattage=$(dc -e "3 k $voltage 1000000 / $current 1000000 / * p")

memory=$(free | awk '/Mem/{printf("free ram: %.2f%% (%1.1fGB)", 100*$7/$2, $7/1e6)}')

loadavg=$(cut -d ' ' -f '1-3' /proc/loadavg)

temp=$(sensors -A | sed -e 's/-virtual-0//' -e 's/temp1:     //' -e 's/(.*)//' -e 's/_thermal//' -e 's/bq27500-0/batt/' -e 's/+//' | paste -s -d ' \n\0')

echo -e "${time_now} hours (${capacity}%)\n${wattage} watts\n${memory}\nload: ${loadavg}\n\n${temp}" | osd_cat -l 10 -d $show_time -c skyblue -s 4 -p middle -A center -f "-*-terminus-*-r-*-*-60-*-*-*-c-*-*-*"



