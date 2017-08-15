#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void ve_cpy_32b(uint32_t *dst, uint32_t *src);

void std_cpy(uint32_t *dst, uint32_t *src) {
	memcpy(dst, src, 32);
}

void ve_cpy_32b(uint32_t *dst, uint32_t *src) {
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
	dst[4] = src[4];
	dst[5] = src[5];
	dst[6] = src[6];
	dst[7] = src[7];
}

void detile_buffer(uint32_t *dst_v, uint32_t *src_v, uint32_t t, uint32_t h, uint32_t w) {
	uint32_t i, j, k;
	uint32_t *dst_ptr, *src_ptr;

	uint32_t i_w;
	uint32_t i_w_j;
	uint32_t j_t;

	uint32_t width_step;
	uint32_t tile_step;
	
	width_step = w >> 2; // '/4'
	tile_step = t >> 2; // '/4'


	for (i = 0; i < h; i += t) {		//For each vertical tile height 32px
		i_w = i * w;

		for (j = 0; j < w; j += t) {                                            //For each horizontal tile width 32px
			i_w_j = i_w + j;
			j_t = j * t;

			dst_ptr = dst_v + ( (i_w_j) >> 2); // '/4'
			src_ptr = src_v + ( ((i_w) + j_t) >> 2); // '/4'
//			memcpy(dst_ptr, src_ptr, t);
//			ve_cpy_32b(dst_ptr, src_ptr);
//			std_cpy(dst_ptr, src_ptr);
			//^ 1st tile done, below the rest
			for (k = 1; k < t; k++) {                                       //For each tile_line_len
				dst_ptr = dst_ptr + width_step;
				src_ptr = src_ptr + tile_step;
//				ve_cpy_32b(dst_ptr, src_ptr);
				memcpy(dst_ptr, src_ptr, t);
			}
		}
	}
}

// #define WIDTH 	1920
// #define HEIGHT 	1088
#define WIDTH 	864
#define HEIGHT 	480
#define BUFFER_SIZE		(WIDTH * HEIGHT)
#define TILE_SIZE 32

#define DURATION(s, e) 		(( e.tv_sec - s.tv_sec )		\
						  	+ ( e.tv_nsec - s.tv_nsec )		\
	  						/ (1E9) )

int main() {
	uint32_t *a, *b;
	struct timespec start, end;

	a = malloc(BUFFER_SIZE);
	b = malloc(BUFFER_SIZE);

	printf("buffer a @ %p, b @ %p\n", a, b);
	memset(a, 0xffff0000, BUFFER_SIZE);
	memset(b, 0xff00ff00, BUFFER_SIZE);

	clock_gettime(CLOCK_REALTIME, &start);
	detile_buffer(a, b, TILE_SIZE, HEIGHT, WIDTH);
	clock_gettime(CLOCK_REALTIME, &end);

	printf( "%lf - max fps with h(%d) w(%d) = %lf Hz\n", DURATION(start, end),
		HEIGHT, WIDTH, 1 / DURATION(start, end));

	printf("Detiled buffers\nPress to contiune...\n");

	getchar();

	free(a);
	free(b);

	return 0;	
}