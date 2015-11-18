For full details visit http://kmkeen.com/c100p-tweaks/

Notes on requirements:

### `c100p.bl-adjust.sh`

Quadratic ramp, takes "up/down" as an argument.  Requires `xosd`, `terminus-font` and `c100p.backlight.conf`.

### `c100p.tablet-mode.c`

Requires `xosd`, `terminus-font` and `xorg-input`.  Build with

    gcc -O2 -lm c100p.tablet-mode.c -o c100p.tablet-mode

### `c100p.vol-adjust.sh`

Takes "up/down/toggle" as an argument.  Requires `xosd`, `terminus-font` and `alsa-utils`.

### `c100p.status.sh`

Requires `xosd` and `terminus-font`.

### `c100p.auto-suspend.sh`

Optionally uses `xosd` and `terminus-font`.  Requires [xprintidle](https://aur.archlinux.org/packages/xprintidle/).

### `charge_current` (from `c100p.bashrc`)

Takes a number between `0` and `3456` as an argument, setting the milliamp charge rate of the battery.  Requires `ec-utils` and a properly configured `sudoers`.

### `charge_until` (from `c100p.bashrc`)

Takes a number between `0` and `100` as an argument, charging the battery until it reaches that percent capacity.  Charge rate may be specified with the `CHARGE_MA` environment variable.

