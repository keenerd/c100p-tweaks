#! /bin/sh

# amixer really does not want to be fine-grained
# mute and toggle are different?  but both set [on]

if [[ "$1" == "toggle" ]]; then
    amixer sset Speaker toggle
fi

sink=Headphone
if ( amixer -- sget Speaker | grep -q '\[on\]' ); then
    sink=Speaker
fi

if [[ "$1" == "up" ]]; then
    amixer -- sset $sink 1+ &
fi

if [[ "$1" == "down" ]]; then
    amixer -- sset $sink 1- &
fi

pkill osd_cat
amount=$(amixer get $sink | grep -o -m 1 '[0-9]*%' | tr -d '%')
font="-*-terminus-*-r-*-*-40-*-*-*-c-*-*-*"

osd_cat -d 1 -c skyblue -s 1 -p middle -A center -b percentage -P $amount -f "$font" -T $sink &

# historic OSS stuff
#VOL=$(ossmix | grep misc.pcm1 | awk '{print $4}' | awk -F : '{print $1}')
#VOL=$(echo $VOL+2 | bc)
#ossmix misc.pcm1 $VOL:$VOL
#ossmix -d0 jack.fp-gray.pcm1 -- +2
#ossmix -d0 vmix0-outvol -- +2
#amixer sset PCM 1+ && amixer sset Master 1+

