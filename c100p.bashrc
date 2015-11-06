# this gets sourced into my common bashrc

PROMPT_COMMAND="touch /tmp/.bashidle"
alias startx="sudo systemctl start sddm"
alias suspend="echo 'Five seconds!'; sleep 5; xinput -disable 'Elan Touchscreen'; sudo systemctl suspend; xinput -enable 'Elan Touchscreen'"
