#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <va/va.h>
#include <va/va_x11.h>
#include <va/va_drm.h>

#define CEDRUS_DRM_DEVICE_PATH "/dev/video0"
#define ONE_PLANES 1
#define NUM_IMAGE_BUFFERS 4

//Taken from modules/hw/vaapi/vlc_vaaapi.c
#define VA_CALL(f, ...)                     \
  do                                        \
  {                                         \
    VAStatus s = f(__VA_ARGS__);            \
    if (s != VA_STATUS_SUCCESS)             \
    {                                       \
      printf("%s: %s", #f, vaErrorStr(s));  \
      goto error;                           \
    }                                       \
  } while (0)

int printDpyInfo(VADisplay dpy);
int getFrameData(uint8_t **data, uint32_t *data_size);
int decodeMpeg2Frames(VADisplay dpy, VAContextID va_context, VASurfaceID *surfaces);
int decodeMpeg2(VADisplay dpy, uint32_t width, uint32_t height);
void printVaImgFmt(VAImageFormat fmt);
void printVAEntrypoint(VAEntrypoint p);
void printVAProfile(VAProfile p);


void printVAProfile(VAProfile p)
{
  switch (p) {
  case VAProfileMPEG2Simple:
    printf("\tprofile: VAProfileMPEG2Simple\n");
    break;
  case VAProfileMPEG2Main:
    printf("\tprofile: VAProfileMPEG2Main\n");
    break;
  case VAProfileMPEG4Simple:
    printf("\tprofile: VAProfileMPEG4Simple\n");
    break;
  case VAProfileMPEG4AdvancedSimple:
    printf("\tprofile: VAProfileMPEG4AdvancedSimple\n");
    break;
  case VAProfileMPEG4Main:
    printf("\tprofile: VAProfileMPEG4Main\n");
    break;
  case VAProfileH264Baseline:
    printf("\tprofile: VAProfileH264Baseline\n");
    break;
  case VAProfileH264Main:
    printf("\tprofile: VAProfileH264Main\n");
    break;
  case VAProfileH264High:
    printf("\tprofile: VAProfileH264High\n");
    break;
  case VAProfileVC1Simple:
    printf("\tprofile: VAProfileVC1Simple\n");
    break;
  case VAProfileVC1Main:
    printf("\tprofile: VAProfileVC1Main\n");
    break;
  case VAProfileVC1Advanced:
    printf("\tprofile: VAProfileVC1Advanced\n");
    break;
  case VAProfileH263Baseline:
    printf("\tprofile: VAProfileH263Baseline\n");
    break;
  case VAProfileJPEGBaseline:
    printf("\tprofile: VAProfileJPEGBaseline\n");
    break;
  case VAProfileH264ConstrainedBaseline:
    printf("\tprofile: VAProfileH264ConstrainedBaseline\n");
    break;
  case VAProfileVP8Version0_3:
    printf("\tprofile: VAProfileVP8Version0_3\n");
    break;
  case VAProfileH264MultiviewHigh:
    printf("\tprofile: VAProfileH264MultiviewHigh\n");
    break;
  case VAProfileH264StereoHigh:
    printf("\tprofile: VAProfileH264StereoHigh\n");
    break;
  case VAProfileHEVCMain:
    printf("\tprofile: VAProfileHEVCMain\n");
    break;
  case VAProfileHEVCMain10:
    printf("\tprofile: VAProfileHEVCMain10\n");
    break;
  case VAProfileVP9Profile0:
    printf("\tprofile: VAProfileVP9Profile0\n");
    break;
  case VAProfileVP9Profile1:
    printf("\tprofile: VAProfileVP9Profile1\n");
    break;
  case VAProfileVP9Profile2:
    printf("\tprofile: VAProfileVP9Profile2\n");
    break;
  case VAProfileVP9Profile3:
    printf("\tprofile: VAProfileVP9Profile3\n");
    break;
  default: 
    printf("Uknown profile");
    break;
  }
}

void printVAEntrypoint(VAEntrypoint p) {
  switch (p) {
  case VAEntrypointVLD:
    printf("\t\t entrypoint: VAEntrypointVLD \n");
    break;
  case VAEntrypointIZZ:
    printf("\t\t entrypoint: VAEntrypointIZZ \n");
    break;
  case VAEntrypointIDCT:
    printf("\t\t entrypoint: VAEntrypointIDCT \n");
    break;
  case VAEntrypointMoComp:
    printf("\t\t entrypoint: VAEntrypointMoComp \n");
    break;
  case VAEntrypointDeblocking:
    printf("\t\t entrypoint: VAEntrypointDeblocking \n");
    break;
  case VAEntrypointEncSlice:
    printf("\t\t entrypoint: VAEntrypointEncSlice \n");
    break;    
  case VAEntrypointEncPicture:
    printf("\t\t entrypoint: VAEntrypointEncPicture \n");
    break;    
  case VAEntrypointEncSliceLP:
    printf("\t\t entrypoint: VAEntrypointEncSliceLP \n");
    break;
  case VAEntrypointVideoProc:
    printf("\t\t entrypoint: VAEntrypointVideoProc \n");
    break;   
  default:
    printf("Unkown entrypoint\n");
    break;
  }
}

void printVaImgFmt(VAImageFormat fmt) {
  switch (fmt.fourcc) {
  case VA_FOURCC_NV12:
    printf("\tVAImageFormat VA_FOURCC_NV12\n");
    break;
  default:
    printf("\tUknown ImageFormat: see /usr/bin/include/va/va.h\n");
    break;
  }
  printf("\t\tbyte_order %d, \n     \
    \t\tbits_per_pixel %d \n      \
    \t--rest only valid for rgb formats \n  \
    \t\tdepth %x \n       \
    \t\tred_mask %x \n      \
    \t\tgreen_mask %x \n      \
    \t\tblue_mask %x \n     \
    \t\talpha_mask %x \n",
    fmt.byte_order, fmt.bits_per_pixel, fmt.depth, fmt.red_mask, 
    fmt.green_mask, fmt.blue_mask, fmt.alpha_mask);
}

int main(int argc, char const **argv)
{
  VADisplay dpy;

  int fd_drm;
  int major_version, minor_version;

  printf("Started ve_player\n");

  /* Open a dri file handle for accelerated hw decoding */
  fd_drm = open(CEDRUS_DRM_DEVICE_PATH, O_RDWR | O_CLOEXEC);
  if (fd_drm == -1) {
    printf("Failed opening dri file handle\n");
    goto error;
  }

  //Get a va drm display
  dpy = vaGetDisplayDRM(fd_drm);
  if (!dpy) {
    printf("vaGetDisplayDRM failed\n \
      \t *hint: run as video user, not root\n");
    goto error;
  }
  printf("vaGetDisplayDRM = %p\n", dpy);

  VA_CALL(vaInitialize, dpy, &major_version, &minor_version);

  printDpyInfo(dpy);

  if(decodeMpeg2(dpy, 864, 480) < 0) {
    printf("decodeMpeg2 failed\n");
    goto error;
  }

  return 0;

error:
  return -1;

}

/* play mpeg2 frames
 * width and height are the frames resolution in pixels
 */
int decodeMpeg2(VADisplay dpy, uint32_t width, uint32_t height)
{
  VAContextID va_context;
  VAConfigID va_config;
  VAConfigAttrib va_attrib;
  VASurfaceID *surfaces;
  uint32_t num_surfaces;

  va_attrib.type = VAConfigAttribRTFormat;

  VA_CALL(vaGetConfigAttributes, dpy, VAProfileMPEG2Main,
    VAEntrypointVLD, &va_attrib, 1);

  if (!(va_attrib.value & VA_RT_FORMAT_YUV420)) {
    printf("Video decoder cannot output YUV420.\n");
    goto error;
  }

  VA_CALL(vaCreateConfig, dpy, VAProfileMPEG2Main, VAEntrypointVLD,
    &va_attrib, 1, &va_config);

  num_surfaces = 2; //2 for double buffering?
  surfaces = calloc(num_surfaces, sizeof(VASurfaceID));
  if (!surfaces) {
    printf("Failed allocating sufaces\n");
    goto error;
  }

  VA_CALL(vaCreateSurfaces, dpy, VA_RT_FORMAT_YUV420, width, height,
    surfaces, num_surfaces, NULL, 0);

  /* The VAContextID will contain references to all buffers, display, 
   * formats and selected pipeline */
  VA_CALL(vaCreateContext, dpy, va_config, width, height, VA_PROGRESSIVE,
    surfaces, num_surfaces, &va_context);

  if (decodeMpeg2Frames(dpy, va_context, surfaces)) {
    printf("decodeMpeg2Frames failed");
  }

  VA_CALL(vaDestroyContext, dpy, va_context);
  VA_CALL(vaDestroySurfaces, dpy, surfaces, num_surfaces);
  VA_CALL(vaDestroyConfig, dpy, va_config);

  return 0;
error:

  printf("decodeMpeg2 failed\n");
  return -1;
}

int decodeMpeg2Frames(VADisplay dpy, VAContextID va_context, VASurfaceID *surfaces)
{
  //Used as data for various buffers which are created using VACreateBuffer();
  uint8_t *data;
  uint32_t data_size;
  uint32_t buf_cnt;

  VABufferID buffers[NUM_IMAGE_BUFFERS];
  buf_cnt = 0;

  /* Get a decoded frame to be decoded */
  if (getFrameData(&data, &data_size) < 0) {
    printf("getFrameData failed\n");
    goto error;
  }

  printf("data %p with len %d\n", &data, data_size);
  data[1] = 0xff;


  printf("1\n");
  VA_CALL(vaCreateBuffer, dpy, va_context, VAImageBufferType,
    data_size, ONE_PLANES, data, &buffers[buf_cnt++]);
  printf("2\n");
  VA_CALL(vaCreateBuffer, dpy, va_context, VAPictureParameterBufferType,
    data_size, ONE_PLANES, data, &buffers[buf_cnt++]);
  printf("3\n");
  VA_CALL(vaCreateBuffer, dpy, va_context, VASliceParameterBufferType,
    data_size, ONE_PLANES, data, &buffers[buf_cnt++]);
  printf("4\n");
  VA_CALL(vaCreateBuffer, dpy, va_context, VASliceDataBufferType,
    data_size, ONE_PLANES, data, &buffers[buf_cnt++]);
  printf("5\n");

  vaBeginPicture(dpy, va_context, surfaces[0]);
  printf("6\n");
  vaRenderPicture(dpy, va_context, buffers, buf_cnt);
  printf("7\n");
  vaEndPicture(dpy, va_context);
  printf("8\n");

  free(data);

  return 0;
error:
  if (data)
    free(data);

  return -1;
}

int getFrameData(uint8_t **data, uint32_t *data_size)
{
  int fd_frames;

  printf("a\n");
  /* Open file containing frames */
  fd_frames = open("/dev/urandom", O_RDONLY);
  if (fd_frames == -1) {
    printf("Failed opening frames file\n");
    return -1;
  }

  //make this the size of the frame
  *data_size = 800*600/10;

  *data = malloc(*data_size);
  if (*data == NULL) {
    printf("failed allocating data buffer\n");
    close(fd_frames);
    return -1;   
  }

  read(fd_frames, *data, *data_size);
  close(fd_frames);

  return 0;
}


/* Print supported profiles and entrypoints
 * Each profile may contain one or many entrypoints.
 * This functions iterates/prints each profile.
 * For each profile all supported entrypoints are printed/iterated.
 */
int printDpyInfo(VADisplay dpy)
{
  VAProfile *va_profiles;
  VAEntrypoint *va_entrypoints;
  int va_max_num_profiles, va_max_num_entrypoints;  
  uint32_t i, j;

  va_max_num_profiles = vaMaxNumProfiles(dpy);
  va_profiles = calloc(va_max_num_profiles, sizeof(VAProfile));
  if(!va_profiles)
    goto error;

  VA_CALL(vaQueryConfigProfiles, dpy, va_profiles, &va_max_num_profiles);

  for (i = 0; i < va_max_num_profiles; i++) {
    printVAProfile(va_profiles[i]);

    va_max_num_entrypoints = vaMaxNumEntrypoints(dpy);
    va_entrypoints = calloc(va_max_num_entrypoints, sizeof(VAEntrypoint));
    if (!va_entrypoints)
      goto error;

    VA_CALL(vaQueryConfigEntrypoints, dpy, va_profiles[i], va_entrypoints,
      &va_max_num_entrypoints);

    for (j = 0; j < va_max_num_entrypoints; j++)
      printVAEntrypoint(va_entrypoints[j]);
  }

  return 0;
error:
  printf("printDpyInfo failed\n");
  return -1;
}

