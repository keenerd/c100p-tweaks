#!/bin/sh

# technically 0 is allowed, but not here
BL_MIN=1
BL_MAX=253

function bl_func()
{
    if [[ "$1" == "" ]]; then
        cat /sys/class/backlight/backlight.20/brightness
        return 0
    fi
    echo "$1" > /sys/class/backlight/backlight.20/brightness
}

bl_now=$(bl_func)

if [[ "$1" == "up" ]]; then 
    bl_now=$(( $bl_now * 2 ))
    if (( $bl_now < $BL_MIN )); then  # happens if bl is off
        bl_now=$BL_MIN
    fi
    if (( $bl_now > $BL_MAX )); then
        bl_now=$BL_MAX
    fi
    bl_func $bl_now
fi

if [[ "$1" == "down" ]]; then 
    bl_now=$(( $bl_now / 2 ))
    if (( $bl_now < $BL_MIN )); then
        bl_now=$BL_MIN
    fi
    bl_func $bl_now
fi

