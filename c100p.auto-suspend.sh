#!/bin/sh

# vaguely, try to figure out if it is safe to suspend
# use case is when the computer is accidentally left unattended

# CRITERIA
# must be on battery
# computer must be idle
# loadavg must be low
# network must be low
# no USB drives mounted (hack)
# show a warning just before
# okay if this is X-only

poll_rate=60    # seconds 
grace=60        # seconds
idle_limit=5    # minutes
load_limit=20   # percent
net_limit=100   # bytes/sec

net_future=0    # expected minimum bytes

on_battery()
{
    battery=$(cat /sys/class/power_supply/bq27500-0/status)
    [[ "$battery" == "Discharging" ]]
}

x_is_idle()
{
    #idle_time=$(xprintidle)
    idle_time=$(DISPLAY=:0 xprintidle)
    idle_time=$(( $idle_time / 60000 ))
    (( idle_time > idle_limit ))
}

shell_is_idle()
{
    # requires PROMPT_COMMAND="touch /tmp/.bashidle" in .bashrc
    result=$(find /tmp/.bashidle -amin -${idle_limit})
    [[ -z "$result" ]]
}

low_load()
{
    load=$(awk '{print $1*100}' < /proc/loadavg)
    (( load < load_limit ))
}

flash_drive()
{
    mount | grep -q sda
}

network_bytes()
{
    # other interfaces ?
    rx=$(cat /sys/class/net/wlan0/statistics/rx_packets)
    tx=$(cat /sys/class/net/wlan0/statistics/tx_packets)
    echo $(( $rx + $tx ))
}

test_all()
{
    if ! on_battery; then
        return 1
    fi

    if ! shell_is_idle; then
        return 1
    fi

    if ! x_is_idle; then
        return 1
    fi

    if ! low_load; then
        return 1
    fi

    if flash_drive; then
        return 1
    fi

    nb=$(network_bytes)
    if (( nb > network_future )); then
        network_future=$(( $net_limit * $poll_rate + $nb ))
        return 1
    fi
    network_future=$(( $net_limit * $poll_rate + $nb ))

    return 0
}

kill_osd()
{
    active=$(pgrep osd_cat)
    if [[ "$active" != "" ]]; then
        kill $active
    fi
}

show_osd()  # $1 is text  $2 is time (optional)
{
    show_time="${2:-10}"
    echo -e "$1" | DISPLAY=:0 osd_cat -l 10 -d $show_time -c red -s 4 -p middle -A center -f  "-*-terminus-*-r-*-*-60-*-*-*-c-*-*-*"
}

maybe_wall()  # $1 is text
{
    if ! pgrep osd_cat; then
        wall "$1"
    fi
}

main_loop()
{
    while true; do
        # wait
        sleep $poll_rate
        # poll
        if ! test_all; then
            continue
        fi
        # possible event?  raise alert
        kill_osd
        show_osd "Warning, suspend imminent" $grace &
        maybe_wall "Warning, suspend imminent"
        # fast poll
        for i in $(seq $grace); do
            sleep 1
            if ! x_is_idle; then
                kill_osd
                show_osd "Suspend aborted" 3 &
                maybe_wall "Suspend aborted"
                break
            fi
            if ! shell_is_idle; then
                kill_osd
                show_osd "Suspend aborted" 3 &
                maybe_wall "Suspend aborted"
                break
            fi
        done
        if ! x_is_idle; then
            continue
        fi
        if ! shell_is_idle; then
            continue
        fi
        # suspend
        sudo systemctl suspend
    done
}

main_loop


