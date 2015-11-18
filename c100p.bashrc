# this gets sourced into my common bashrc

PROMPT_COMMAND="touch /tmp/.bashidle"
alias startx="sudo systemctl start sddm"
alias suspend="echo 'Five seconds!'; sleep 5; xinput -disable 'Elan Touchscreen'; sudo systemctl suspend; xinput -enable 'Elan Touchscreen'"
# default charging current is 3456
# do not go over that value
alias charge_current="sudo ectool chargestate param 1"

function charge_until()  # takes 0-100
{
    charge_ma="${CHARGE_MA:-3456}"
    charge_current $charge_ma
    while (( $(cat /sys/class/power_supply/bq27500-0/capacity) < $1 )); do
        sleep 10
    done
    charge_current 0
}
