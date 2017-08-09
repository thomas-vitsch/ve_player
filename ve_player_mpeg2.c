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

#define DUMP_FILE "vaapi_cedrus_buffer_output.bin"
#define HIGHEST_BUFFER_ID 42

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
		if (s != VA_STATUS_SUCCESS)					\
		{											\
			printf("%s: %s", #f, vaErrorStr(s));	\
			goto error;								\
		}											\
	} while (0)


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

	num_surfaces = 2; //2 for double buffering?
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
	VA_CALL(vaCreateImage, dpy, &va_image_format, 854, 480, &va_image);

	VAImageFormat va_image_format2;
	va_image_format2.fourcc = VA_FOURCC_NV12;
	va_image_format2.byte_order = 0;
	va_image_format2.bits_per_pixel = 0;
	va_image_format2.depth = 0;
	va_image_format2.red_mask = 0;
	va_image_format2.green_mask = 0;
	va_image_format2.blue_mask = 0;
	VA_CALL(vaCreateImage, dpy, &va_image_format2, 854, 480, &va_image2);

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
int decode_frame(uint32_t buf_a_type, uint32_t buf_a_size, uint32_t *buf_a, uint32_t buf_a_elements,
			uint32_t buf_b_type, uint32_t buf_b_size, uint32_t *buf_b, uint32_t buf_b_elements) {
	VABufferID bufs[NR_IMG_BUF];

	VA_CALL(vaCreateBuffer, dpy, va_context, 
		buf_a_type, (buf_a_size / buf_a_elements), buf_a_elements, buf_a, &bufs[0]);

	VA_CALL(vaCreateBuffer, dpy, va_context, 
		buf_b_type, (buf_b_size / buf_b_elements), buf_b_elements, buf_b, &bufs[1]);

	vaRenderPicture(dpy, va_context, bufs, NR_IMG_BUF);

	// vaEndPicture(dpy, va_context);

	return 0;

error:
	return -1;
}

int read_next_buffer(int fd, uint32_t **buf, uint32_t *buf_size, uint32_t *buf_type, uint32_t *num_elem) {
	int res;

	res = read(fd, buf_type, 4);
	if (res < 1) {
		printf("Could not read buffer_type from file\n");
		return -1;
	}

	res = read(fd, buf_size, 4);
	if (res < 1) {
		printf("Could not read buffer_size from file\n");
		return -1;
	}

	res = read(fd, num_elem, 4);
	if (res < 1) {
		printf("Could not read num_elem from file\n");
		return -1;
	}

	if (buf_size > 0) {
		*buf = malloc(*buf_size);
		if (*buf == NULL) {
			printf("Failed allocating buffer data\n");
			return -1;
		}

		//Read actual buffer block
		res = read(fd, *buf, *buf_size);
		if (res < 1) {
			printf("Expected more data for buffer bit didn't get it, EOF?\n");
			return -1;
		}
	} else {
		printf("buf_size < 1\n");
		return -1;
	}

	printf("Succesfull read on read_next_buffer\n");
	printf("buf = %p\n", *buf);

	return 0;
}

int read_video_dump(char const *file) {
	int fd;
	uint32_t *buf_a, *buf_b;
	uint32_t buf_a_size, buf_b_size;
	uint32_t buf_a_type, buf_b_type;
 	uint32_t buf_a_num_elem, buf_b_num_elem;

	uint32_t buf_cnt[HIGHEST_BUFFER_ID] = {0};
	uint32_t i;

	buf_a = NULL;
	buf_b = NULL;

	printf("Started read_dump\n");

	/* Open file containing frames */
	fd = open(file, O_RDONLY);
	if (fd == -1) {
		printf("Failed opening %s\n", file);
		return -1;
	}

	prepare_decoder(854, 480);

	/* We expect the buffers to be in a specific order. If one of the buffer read
	 * is not what we expected we will skip buffers until it is what we expected. */
	while (1) {
//		sleep(0.01);
		read_next_buffer(fd, &buf_a, &buf_a_size, &buf_a_type, &buf_a_num_elem);	
		if (buf_a_type != VAPictureParameterBufferType) {
			printf("Aborting: Expected buffer type %d, got %d\n", 
				VAPictureParameterBufferType, buf_a_type);
			break;
		} else { printf("Got VAPictureParameterBufferType \n"); }

//		sleep(0.01);
		read_next_buffer(fd, &buf_b, &buf_b_size, &buf_b_type, &buf_b_num_elem);	
		if (buf_b_type != VAIQMatrixBufferType) {
			printf("Aborting: Expected buffer type %d, got %d\n", 
				VAIQMatrixBufferType, buf_b_type);
			break;
		} else { printf("Got VAIQMatrixBufferType \n"); }

		vaBeginPicture(dpy, va_context, surfaces[0]);
//		sleep(0.01);

		// Decode frame in cedrus driver
		decode_frame(buf_a_type, buf_a_size, buf_a, buf_a_num_elem,
			buf_b_type, buf_b_size, buf_b, buf_b_num_elem);
		printf("buf_a = %p, buf_b = %p\n", buf_a, buf_b);

//		sleep(0.01);

		free(buf_a);
		free(buf_b);

		//Counts all occurances of each buffer.
		buf_cnt[buf_a_type]++;
		buf_cnt[buf_b_type]++;

//		sleep(0.01);
		read_next_buffer(fd, &buf_a, &buf_a_size, &buf_a_type, &buf_a_num_elem);	
		if (buf_a_type != VASliceParameterBufferType) {
			printf("Aborting: Expected buffer type %d, got %d\n", 
				VASliceParameterBufferType, buf_a_type);
			break;
		} else { printf("Got VASliceParameterBufferType \n"); }

//		sleep(0.01);
		read_next_buffer(fd, &buf_b, &buf_b_size, &buf_b_type, &buf_b_num_elem);	
		if (buf_b_type != VASliceDataBufferType) {
			printf("Aborting: Expected buffer type %d, got %d\n", 
				VASliceDataBufferType, buf_b_type);
			break;
		} else { printf("Got VASliceDataBufferType \n"); }

		//Decode frame in cedrus driver
		decode_frame(buf_a_type, buf_a_size, buf_a, buf_a_num_elem,
			buf_b_type, buf_b_size, buf_b, buf_b_num_elem);

		vaEndPicture(dpy, va_context);
		printf("buf_a = %p, buf_b = %p\n", buf_a, buf_b);

		printf("press enter for next frame\n");
		getchar();
//		sleep(0.01);

		free(buf_a);
		free(buf_b);

		//Counts all occurances of each buffer.
		buf_cnt[buf_a_type]++;
		buf_cnt[buf_b_type]++;

		//debug sleep
		printf("We should have 1 decoded frame by now\n");
	}

	clean_up_decoder();

	printf("\n buffer types counted:\n");
	for (i = 0; i <= HIGHEST_BUFFER_ID; i++)
		if (buf_cnt[i] > 0)
			printf("\tBuffer type %s(%d) counted %d times\n", 
				getBufferName(i), i, buf_cnt[i]);

	close(fd);

	return 0;
}