#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <va/va.h>
#include <va/va_x11.h>
#include <va/va_drm.h>
#include <errno.h>
#include <err.h>
#include <string.h>

#include <sys/time.h>

#define DUMP_FILE "vaapi_cedrus_buffer_output.bin"
#define HIGHEST_BUFFER_ID 42

//#define ENABLE_DEBUG_PRINT
#ifdef ENABLE_DEBUG_PRINT
#define dbgprintf(...) printf (__VA_ARGS__)
#else
#define dbgprintf(...) 
#endif

double get_ftime(void)
{
	struct timeval t;
	
	gettimeofday(&t, NULL);
	
	return t.tv_sec * 1.0 + t.tv_usec * 1.0e-6;
}

double time_per_frame = 1.0 / 25.0;

void try_mem_shuffle(void)
{
	uint8_t *bufs[128];
	uint8_t *buf;
	uint8_t *p;
	int idx, cnt;

	// Allocate all available memory in chunks of 16MB.
	idx = 0;
	do {
		buf = malloc(16 * 1024 * 1024);
		if (buf != NULL) {
			// For initialization of at least one byte on each page
			// to make sure we don't get lazy allocations without
			// initialized backing store.
			p = buf;
			for (cnt = 0; cnt < 4096; cnt++) {
				*p = 0xaa;
				p += 4096;
			}
		}
		bufs[idx] = buf;
	} while ((buf != NULL) && (++idx < 128));

	printf("Failed after allocating %d MB\n", idx * 16);
	
	// Free all the memory we did manage to allocate.
	while (idx-- > 0) {
		free(bufs[idx]);
	}
}

void try_to_run_real_time(void)
{
	static int first_round = 1;
	static double t_start = 0.0;
	static double t_now, t_lineair, t_left, t_last_round;
	static double t_last_shuffle = 0.0;
	static long frame_nr = 0;
	
	t_now = get_ftime();
	if (first_round) {
		first_round = 0;
		t_start = t_now;
		t_last_round = t_now;
	}
	// Check for timewarps (clock stepping caused by e.g. ntp during boot)
	if ((t_now < t_last_round - 20.0) || (t_now > t_last_round + 20.0)) {
		printf("timewarp detected. :)\n");
		t_start = t_now;
		frame_nr = 0;
	}
	t_last_round = t_now;
	// See where in time we should have been by this time.
	frame_nr++;
	t_lineair = t_start + (double)frame_nr * time_per_frame;
	t_left = t_lineair - t_now;
	if (t_left > 0.0) {
		// We've got time left to sleep a bit :)
		printf("zzz %0.1fms\n", t_left * 1.0e3);
		usleep(t_left * 1.0e6);
	}
	
	// Run memory reshuffle test once in a while.
	if (t_last_shuffle != 0.0) {
		t_left = t_now - t_last_shuffle;
		if (t_last_shuffle > 60.0) {
			t_last_shuffle = t_now;
			//try_mem_shuffle();
		}
	} else
		t_last_shuffle = t_now;
}

char *getBufferName(int buffer_type) {
	switch (buffer_type) {
	case 0:
		return "VAPictureParameterBufferType";
	case 1:
		return "VAIQMatrixBufferType";
	case 4:
		return "VASliceParameterBufferType";
	case 5:
		return "VASliceDataBufferType";  
	default:
		return "Unkown buffer, check va/va.h for declaration";
	}
}

int read_video_dump(char const *file);

int main(int argc, char const **argv)
{
	int res;

	if (argc < 2) {
		printf("Usage: \"read_dump input_file\"\n");
		return -1;
	}    

	res = read_video_dump(argv[1]);
	if (res < 0)
		printf("Could not read video file\n");
	
	return res;
}

static VAContextID va_context;
static VASurfaceID *surfaces;
static VAConfigID  va_config;
static uint32_t num_surfaces;
static VADisplay dpy;
static VAImage	va_image, va_image2;

#define CEDRUS_DRM_DEVICE_PATH "/dev/video0"
#define ONE_PLANES 1
#define NUM_IMAGE_BUFFERS 4

//Taken from modules/hw/vaapi/vlc_vaaapi.c
#define VA_CALL(f, ...)								\
	do 												\
	{												\
		VAStatus s = f(__VA_ARGS__);				\
		if (s != VA_STATUS_SUCCESS) {					\
			printf("%s: %s", #f, vaErrorStr(s));	\
			goto error;								\
		}											\
	} while (0)



// Every buffer is preceded by the following header in our dump files:
// (We use this struct to read all 4 variables in one read() system call
// instead of calling read() 4 times in a row.)
struct in_file_header_t {
	uint32_t		surface_id;
	uint32_t		buf_type;
	uint32_t		buf_size;
	uint32_t		num_elem;
};

// Internal structure wherein we keep track of info about memory buffers we
// allocate.
struct buf_info_t {
	// Filled-in by our 'allocator'
	int			allocated_size;
	void			*data;
	
	// Used by read_next_buf
	uint32_t		surface_id;
	uint32_t		buf_type;
	uint32_t		buf_size;
	uint32_t		num_elem;
};

#define OVERALLOC_SIZE		(128 * 1024)

void *ve_buf_alloc(struct buf_info_t *info, ssize_t requested_size)
{
	
	// First, see if we already have a buffer from an earlier round.
	if (info->data == NULL) {
		// Nope. Try to allocate a fresh one. Caller is responsible
		// for handling the error case.
		info->allocated_size = requested_size + OVERALLOC_SIZE;
		printf("allocating buf of %d\n", info->allocated_size);
		info->data = malloc(info->allocated_size);
		return info->data;
	}
	
	// If we make it to this point, we already had a buffer allocated.
	// Check if the existing buffer is large enough to hold the amount
	// that's requested this time.
	if (info->allocated_size < requested_size) {
		// Buffer is too small. free() it and allocate a bigger
		// buffer.
		free(info->data);
		info->allocated_size = requested_size + OVERALLOC_SIZE;
		printf("growing buf to %d\n", info->allocated_size);
		info->data = malloc(info->allocated_size);
	} else {
		// Yup. Existing buffer is big enough to re-use.
	}
	
	return info->data;
}

void ve_buf_free(struct buf_info_t *buf_info)
{

	// We expect the caller to only call us on buffers that have actually
	// been allocated succesfully earlier on.
	free(buf_info->data);
}




int prepare_decoder(uint32_t width, uint32_t height) {
	int fd_drm;
	VAConfigAttrib va_attrib;
	int major_version, minor_version;

	/* Open a dri file handle for accelerated hw decoding */
	fd_drm = open(CEDRUS_DRM_DEVICE_PATH, O_RDWR | O_CLOEXEC);
	if (fd_drm == -1) {
		printf("Failed opening dri file handle\n");
		return -1;
	}

	//Get a va drm display
	dpy = vaGetDisplayDRM(fd_drm);
	if (!dpy) {
		printf("vaGetDisplayDRM failed\n \
			\t *hint: run as video user, not root\n");
		return -1;
	}
	printf("vaGetDisplayDRM = %p\n", dpy);

	VA_CALL(vaInitialize, dpy, &major_version, &minor_version);

	va_attrib.type = VAConfigAttribRTFormat;

	VA_CALL(vaGetConfigAttributes, dpy, VAProfileMPEG2Main,
	VAEntrypointVLD, &va_attrib, 1);

	if (!(va_attrib.value & VA_RT_FORMAT_YUV420)) {
		printf("Video decoder cannot output YUV420.\n");
		return -1;
	}

	VA_CALL(vaCreateConfig, dpy, VAProfileMPEG2Main, VAEntrypointVLD,
		&va_attrib, 1, &va_config);

	num_surfaces = 4; //If 2 then we get green pixels all around :P 
	surfaces = calloc(num_surfaces, sizeof(VASurfaceID));
	if (!surfaces) {
		printf("Failed allocating sufaces\n");
		return -1;
	}

	VA_CALL(vaCreateSurfaces, dpy, VA_RT_FORMAT_YUV420, width, height,
		surfaces, num_surfaces, NULL, 0);

	/* The VAContextID will contain references to all buffers, display, 
	* formats and selected pipeline */
	VA_CALL(vaCreateContext, dpy, va_config, width, height, VA_PROGRESSIVE,
		surfaces, num_surfaces, &va_context);

	VAImageFormat va_image_format;
	va_image_format.fourcc = VA_FOURCC_NV12;
	va_image_format.byte_order = 0x140;
	va_image_format.bits_per_pixel = 4;
	va_image_format.depth = 0x30;
	VA_CALL(vaCreateImage, dpy, &va_image_format, height, width, &va_image);

	VAImageFormat va_image_format2;
	va_image_format2.fourcc = VA_FOURCC_NV12;
	va_image_format2.byte_order = 0;
	va_image_format2.bits_per_pixel = 0;
	va_image_format2.depth = 0;
	va_image_format2.red_mask = 0;
	va_image_format2.green_mask = 0;
	va_image_format2.blue_mask = 0;
	VA_CALL(vaCreateImage, dpy, &va_image_format2, width, height, &va_image2);

	return 0;

error:
	printf("Failed preparing decoder\n");
	return -1;
}

int clean_up_decoder(){
	VA_CALL(vaDestroyContext, dpy, va_context);
	VA_CALL(vaDestroySurfaces, dpy, surfaces, num_surfaces);
	VA_CALL(vaDestroyConfig, dpy, va_config);

	return 0;
error:
	return -1;
}

/* Hoewel vlc steeds een image met 4 buffers aanmaakt blijkt het *Image type slechts 
 * 2 buffers te hebben. !!!!! URGH dit zijn zeker de 2 planes .-_-.
 * en dan wel op deze manier toegevoegd aan de driver  0 1 4 5 
 *
 * VAPictureParameterBufferType		= 0,
 * VAIQMatrixBufferType				= 1,
 * VASliceParameterBufferType		= 4,
 * VASliceDataBufferType			= 5,
 */

#define NR_IMG_BUF 2
int decode_frame(struct buf_info_t *buf1, struct buf_info_t *buf2)
{
	VABufferID bufs[NR_IMG_BUF];

	VA_CALL(vaCreateBuffer, dpy, va_context, buf1->buf_type,
	    (buf1->buf_size / buf1->num_elem), buf1->num_elem, buf1->data,
	    &bufs[0]);

	VA_CALL(vaCreateBuffer, dpy, va_context, buf2->buf_type,
	    (buf2->buf_size / buf2->num_elem), buf2->num_elem, buf2->data,
	    &bufs[1]);

	vaRenderPicture(dpy, va_context, bufs, NR_IMG_BUF);

	// vaEndPicture(dpy, va_context);

	return 0;

error:
	return -1;
}


int read_next_buffer(int fd, struct buf_info_t *buf_info) {
	struct in_file_header_t hdr;
	int res, left;
	uint8_t *ptr;

	// Read the buffer header from file (possibly in multiple read()
	// attempts when we get less from read() than what we've asked for.)
	ptr = (uint8_t *)&hdr;
	left = sizeof(hdr);
	do {
		res = read(fd, ptr, left);
		if (res > 0) {
			ptr += res;
			left -= res;
		}
	} while ((res > 0) && (left > 0));
	if (left)
		printf("EEK! read header: wanted=%d, got=%d\n", sizeof(hdr),
		    res);
	if (res < 1) {
		printf("Failed to read the next buffer header from file. "
		    "EOF?\n");
		return -1;
	}
	buf_info->surface_id = hdr.surface_id;
	buf_info->buf_type = hdr.buf_type;
	buf_info->buf_size = hdr.buf_size;
	buf_info->num_elem = hdr.num_elem;
	
	// Read the buffer's data.
	if (buf_info->buf_size > 0) {
		ptr = ve_buf_alloc(buf_info, buf_info->buf_size);
		if (ptr == NULL) {
			printf("Failed allocating buffer data\n");
			return -1;
		}
		
		// Thomas: read() may return with less than requested bytes,
		// but more than 0. E.g. when piping application together (or
		// even on regular files under certain circumstances).
		left = buf_info->buf_size;
		do {
			res = read(fd, ptr, left);
			if (res > 0) {
				ptr += res;
				left -= res;
			}
		} while ((res > 0) && (left > 0));
		
		if (left)
			printf("EEK! read: wanted=%d, got=%d\n",
			    buf_info->buf_size, res);
		// ^ Read actual buffer block
		if (res < 1) {
			printf("Expected more data for buffer bit didn't get it, EOF?\n");
			return -1;
		}
	} else {
		printf("buf_size < 1\n");
		return -1;
	}

	dbgprintf("Succesfull read on read_next_buffer\n");
	dbgprintf("buf = %p\n", *buf);

	return 0;
}

int read_video_dump(char const *file) {
	int fd;
	struct buf_info_t buf_a, buf_b, buf_c, buf_d;
 	int frame_delay = 0;
 	int please_stop = 0;
 	int ret;
 	int fps;

	uint32_t buf_cnt[HIGHEST_BUFFER_ID] = {0};
	uint32_t i;

	uint32_t target_surface = 0;

	buf_a.data = NULL;
	buf_b.data = NULL;
	buf_c.data = NULL;
	buf_d.data = NULL;

	printf("Started read_dump\n");

	/* Open file containing frames */
	fd = open(file, O_RDONLY);
	if (fd == -1) {
		printf("Failed opening %s\n", file);
		return -1;
	}

	// Allow the user to specify a per-frame sleep time in miliseconds on
	// the fly.
	if (getenv("frame_delay") != NULL) {
		sscanf(getenv("frame_delay"), "%d", &frame_delay);
		frame_delay *= 1000;
	}
	// Or a framerate
	if (getenv("fps") != NULL) {
		sscanf(getenv("fps"), "%d", &fps);
		time_per_frame = 1.0 / (double)fps;
	}
	

//	prepare_decoder(854, 480);
	prepare_decoder(1920, 1080);
	//prepare_decoder(1600, 900);

	/* We expect the buffers to be in a specific order. If one of the buffer read
	 * is not what we expected we will skip buffers until it is what we expected. */
	while (! please_stop) {
		ret = read_next_buffer(fd, &buf_a);
		if (ret == -1) {
			please_stop = 1;
			continue;
		}
		if (buf_a.buf_type != VAPictureParameterBufferType) {
			printf("Aborting: Expected buffer type %d, got %d\n", 
			    VAPictureParameterBufferType, buf_a.buf_type);
			break;
		} else { 
			dbgprintf("Got VAPictureParameterBufferType \n"); 
//			buf_a_type ref deinh		
		}

		read_next_buffer(fd, &buf_b);
		if (buf_b.buf_type != VAIQMatrixBufferType) {
			printf("Aborting: Expected buffer type %d, got %d\n", 
			    VAIQMatrixBufferType, buf_b.buf_type);
			break;
		} else {
			dbgprintf("Got VAIQMatrixBufferType \n");
		}

		read_next_buffer(fd, &buf_c);
		if (buf_c.buf_type != VASliceParameterBufferType) {
			printf("Aborting: Expected buffer type %d, got %d\n", 
			    VASliceParameterBufferType, buf_c.buf_type);
			break;
		} else {
			dbgprintf("Got VASliceParameterBufferType \n");
		}

		read_next_buffer(fd, &buf_d);
		if (buf_d.buf_type != VASliceDataBufferType) {
			printf("Aborting: Expected buffer type %d, got %d\n", 
			    VASliceDataBufferType, buf_d.buf_type);
			break;
		} else {
			dbgprintf("Got VASliceDataBufferType \n");
		}

//		printf("target_surface = %d\n", target_surface);
//		if (target_surface == 3) 
//			continue;

		target_surface = buf_d.surface_id;

		vaBeginPicture(dpy, va_context, surfaces[target_surface]);
		
		// Prepare for next frame in cedrus driver
		decode_frame(&buf_a, &buf_b);
		dbgprintf("buf_a = %p, buf_b = %p\n", buf_a->data,
		    buf_b->data);

		//Decode frame in cedrus driver
		decode_frame(&buf_c, &buf_d);
		dbgprintf("buf_c = %p, buf_d = %p\n", buf_c->data,
		    buf_d->data);

		vaEndPicture(dpy, va_context);


//		printf("press enter for next frame\n");
//		getchar();
//		usleep(500000); //25 fps
//		usleep(40000); //25 fps
//		usleep(20000); //50fps
//		usleep(16700); //60 fps
//		usleep(8333); //120 fps
//		usleep(4167); //240 fps
//		usleep(2084); //480 fps
//		usleep(1500); //480 fps Uiterste limiet van de mixer processor 480*648

//		usleep(1042); //960 fps Dit haalt de mixer processor niet meer
//		usleep(1000000);

		//sleep(1);
		if (frame_delay > 0)
			usleep(100 * 1000);

		try_to_run_real_time();

		//Wait until all pending ops have finished for this surface
		//Inside this function detiling should be handled.
		vaSyncSurface(dpy, surfaces[target_surface]);


//		if (target_surface == (num_surfaces -1))
//			target_surface = 0;
//		else
//			target_surface++;


		//Counts all occurances of each buffer.
		buf_cnt[buf_a.buf_type]++;
		buf_cnt[buf_b.buf_type]++;
		buf_cnt[buf_c.buf_type]++;
		buf_cnt[buf_d.buf_type]++;

		//debug sleep
		dbgprintf("We should have 1 decoded frame by now\n");
	}
	
	printf("Going to shut down player..\n");

	clean_up_decoder();

	ve_buf_free(&buf_a);
	ve_buf_free(&buf_b);
	ve_buf_free(&buf_c);
	ve_buf_free(&buf_d);

	printf("\n buffer types counted:\n");
	for (i = 0; i <= HIGHEST_BUFFER_ID; i++)
		if (buf_cnt[i] > 0)
			printf("\tBuffer type %s(%d) counted %d times\n", 
				getBufferName(i), i, buf_cnt[i]);

	close(fd);

	return 0;
}