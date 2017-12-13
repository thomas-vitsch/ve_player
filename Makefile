

%.o: %.c

default: ve_player_v2

read_dump:
	gcc read_dump.c -o read_dump -Wall -Werror

ve_player:
	gcc ve_player.c -o ve_player -lva-drm -lva -Wall

pre-buffer:
	gcc pre-buffer.c -o pre-buffer -Wall -lpthread
	
ve_player_v2: pre-buffer
	gcc ve_player_mpeg2.c -o ve_player_v2 -lva-drm -lva -Wall	

unroll:
	gcc  detile_unroll_test.c -o unroll -Wall -fverbose-asm

fe_ioctl_test:
	gcc fe_ioctl_test.c -o fe_ioctl_test -Wall -Werror

.PHONY: read_dump ve_player ve_player_v2 unroll fe_ioctl_test pre-buffer