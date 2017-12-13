#!/bin/sh

( cd /root/ && ./connect_fe0_2_be0_full_hd.sh )

export LIBVA_DRIVER_NAME=sunxi_cedrus

export fps=60

cd /opt/ve_player
#./pre-buffer /mnt/media/norway-dump.bin | ./ve_player_v2 /dev/stdin
./pre-buffer /mnt/media/earth1-klok-1920x1080.bin | ./ve_player_v2 /dev/stdin
#./pre-buffer /mnt/media/earth2-klok-1920x1080.bin | ./ve_player_v2 /dev/stdin

