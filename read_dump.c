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

int main(int argc, char const **argv)
{
  int fd;
  uint32_t data_size;
  uint32_t buffer_type;
  uint32_t *buffer_data;
  uint32_t num_elements;
  int ret;
  
  uint32_t buf_cnt[HIGHEST_BUFFER_ID] = {0};
  uint32_t i;

  printf("Started read_dump\n");

  if (argc < 2) {
    printf("Usage: \"read_dump input_file\"\n");
    return -1;
  }    

  /* Open file containing frames */
  fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    printf("Failed opening %s\n", argv[1]);
    return -1;
  }

  while (1) {
  	ret = read(fd, &buffer_type, 4); //Skip surf_id
  	ret = read(fd, &buffer_type, 4);
  	if (ret < 1)
  	  break;

    ret = read(fd, &data_size, 4);
    if (ret < 1)
      break;

    ret = read(fd, &num_elements, 4);
    if (ret < 1)
      break;

          if (data_size > 0) {
            buffer_data = malloc(data_size);
            if (buffer_data == NULL) {
              printf("Failed allocating buffer data\n");
              return -1;
            }

            //Read actual buffer block
            ret = read(fd, buffer_data, data_size);
          
            //Do something with buffer block         
            free(buffer_data);
          } else {
            printf("data size < 1\n");
          }
  	
          //Counts all occurances of each buffer.
          buf_cnt[buffer_type]++;
                  
  	printf("Buffer_id = %d, with size %d, num_elements %d\n", 
      buffer_type, data_size, num_elements);
  }

  if(ret < 0)
    goto read_err;

  printf("\n buffer types counted:\n");
  for (i = 0; i <= HIGHEST_BUFFER_ID-1; i++)
    if (buf_cnt[i] > 0)
      printf("\tBuffer type %s(%d) counted %d times\n", 
        getBufferName(i), i, buf_cnt[i]);

  close(fd);

  return 0;

read_err:
        printf("%s\n", strerror(ret));
	return -1;
}
