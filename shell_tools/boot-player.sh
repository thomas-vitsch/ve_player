#!/bin/sh

# Dit script wordt gestart vanuit /etc/rc.local

# Het start een 'screen' sessie met daarin onderstaand script.
# Je kunt een screen sessie 'attachen' door bijvoorbeeld via ssh in te loggen
# en in je terminal te tiepen:
#
#	screen -r player
#
# Met '[ctrl]+A D' verlaat je daarna de sessie weer, terwijl de applicatie op
# de achtergrond doordraait.
#

screen=/usr/bin/screen

# Turn off annoying screen blanking of console after 600 seconds..
(
	export TERM="linux"
	/usr/bin/setterm --reset --blank 0 --powerdown 0 --powersave off \
	    >/dev/tty1 </dev/tty1
)

cmd="/opt/ve_player/start-film-in-loop.sh"
name="player"

${screen} -dmS ${name} ${cmd}
