

%.o: %.c

default: read_dump ve_player ve_player_v2

read_dump:
	gcc read_dump.c -o read_dump -Wall -Werror

ve_player:
	gcc ve_player.c -o ve_player -lva-drm -lva -Wall
	
ve_player_v2:
	gcc ve_player_mpeg2.c -o ve_player_v2 -lva-drm -lva -Wall	

unroll:
	gcc  detile_unroll_test.c -o unroll -Wall -fverbose-asm

.PHONY: read_dump ve_player ve_player_v2 unroll