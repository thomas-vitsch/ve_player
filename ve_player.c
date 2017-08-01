#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>

#include <va/va.h>
#include <va/va_x11.h>
#include <va/va_drm.h>

#define CEDRUS_DRM_DEVICE_PATH "/dev/video0"

//Taken from modules/hw/vaapi/vlc_vaapi.c
#define VA_CALL(f, args...)				\
    do                                                  \
    {                                                   \
        VAStatus s = f(args);                           \
        if (s != VA_STATUS_SUCCESS)                     \
        {                                               \
            printf("%s: %s", #f, vaErrorStr(s));    	\
            goto error;                                 \
        }                                               \
    } while (0)


void printVaProfile(VAProfile p) {
  switch (p) {
	case VAProfileNone:
		printf("\tprofile: VAProfileNone\n");
		break;
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

void printEntrypoint(VAEntrypoint p) {
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
    printf("\t\tbyte_order %d, \n 		\
      \t\tbits_per_pixel %d \n 			\
      \t--rest only valid for rgb formats \n 	\
      \t\tdepth %x \n				\
      \t\tred_mask %x \n			\
      \t\tgreen_mask %x \n			\
      \t\tblue_mask %x \n			\
      \t\talpha_mask %x \n",
      fmt.byte_order,
      fmt.bits_per_pixel,
      fmt.depth,
      fmt.red_mask,
      fmt.green_mask,
      fmt.blue_mask,
      fmt.alpha_mask);

}

int main(int argc, char **argv) {
  VADisplay dpy;
  VANativeDisplay native_dpy;
  VAConfigID va_conf_id; 
//  VAProfile va_profile;
  VAStatus va_status;
  uint32_t major_version, minor_version;
  uint32_t va_max_num_profiles;  
  VAProfile *va_profiles;
  int fd_drm;
  uint32_t i, j;
  VAEntrypoint *va_entrypoints;
  uint32_t va_entrypoints_num;
  VAConfigAttrib va_attrib;

  printf("Starting ve_player.\n");

  //Open dri file handle
  fd_drm = open(CEDRUS_DRM_DEVICE_PATH, O_RDWR | O_CLOEXEC);
  if (fd_drm == -1) {
    printf("Failed opening drm\n"); 
    return -1;
  }

  //Get a va drm display
  dpy = vaGetDisplayDRM(fd_drm);
  if (dpy)
    printf("vaGetDisplayDRM got %p\n", dpy);
  else {
    printf("vaGetDisplayDRM failed\n");
    return -2;
  }

  printf("If err == va_getDriverName() returns -1\n\t run as video user, not root\n");
  VA_CALL(vaInitialize, dpy, &major_version, &minor_version);

  va_max_num_profiles = vaMaxNumProfiles(dpy);
//  printf("nr profiles = %d. Must be < 0\n", va_max_num_profiles);    

  va_profiles = calloc(va_max_num_profiles, sizeof(VAProfile));
  if(!va_profiles) {
    printf("calloc va_profiles failed\n");  
    return -4;
  }

  // Get supported profiles
  VA_CALL(vaQueryConfigProfiles, dpy, va_profiles, &va_max_num_profiles);

  for (i = 0; i < va_max_num_profiles; i++) {
    printVaProfile(va_profiles[i]);
    
    // Get supported entrypoints
    va_entrypoints_num = vaMaxNumEntrypoints(dpy);

    va_entrypoints = calloc(va_entrypoints_num, sizeof(VAEntrypoint));
    if(!va_entrypoints) {
      printf("calloc va_entrypoints failed\n");  
      return -6;
    }

    VA_CALL(vaQueryConfigEntrypoints, dpy, va_profiles[i], va_entrypoints,
      &va_entrypoints_num);
    
    for (j = 0; j < va_entrypoints_num; j++)
        printEntrypoint(va_entrypoints[j]);       
  }

/////////////////////SETUP for big_buck_bunny mpeg2@main Profile////////////////
  va_attrib.type = VAConfigAttribRTFormat;
  
  if (vaGetConfigAttributes(dpy, VAProfileMPEG2Main, VAEntrypointVLD, 
    &va_attrib, 1)) { //1 = just for one atrribute
    printf("Invalid entrypoint for chosen profile\n");
    return -7;
  }

  /* We do not care about the format I thing. But if we have to specify one 
  format it would by YUV420 */
  if ((va_attrib.value & VA_RT_FORMAT_YUV420) == 0) {
    printf("VAConfigAttribRTFormat does not support YUV420\n");
    return -8;
  }



  //We need a config to create a context (pipeline)
  VAConfigID config_id;
  VA_CALL(vaCreateConfig, dpy, VAProfileMPEG2Main, VAEntrypointVLD, 
   &va_attrib, 1, &config_id);
  
  /* This fails as our friend Florent did not implement this function
   * in the cedrus vaapi backend :(
   * Assume it is NV12 as output format :(
  // We also need a surface in order to create a context
  unsigned int nr_surface_attribs = 10;
  VASurfaceAttrib *surface_attribs;
  VA_CALL(vaQuerySurfaceAttributes, dpy, config_id, surface_attribs,
      &nr_surface_attribs);

  printf("Supported surface attributes for selected profile/entry:\n");
  for (i = 0; i < nr_surface_attribs; i++) {
    printf("\tsurface_attribs %d\n", i);
  } 
  */
  VASurfaceID *surfaces;
  unsigned int num_surfaces = 1; //Maybe 2 for double buffer?
  VA_CALL(vaCreateSurfaces, dpy, VA_RT_FORMAT_YUV420, 800, 600, 
    surfaces, num_surfaces, NULL, 0);

  VAContextID context_id;
  /* A context is needed in order to use vaCreateBuffer.
   * Create a context = vaCreateContext
   * !!! Nee, vaCreateImage maakt ook een buffer aan zonder een context
   * nodig te hebben :).
   */

   VA_CALL(vaCreateContext, dpy, config_id, 800, 600, VA_PROGRESSIVE,
     surfaces, num_surfaces, &context_id);


//   uint32_t va_img_fmt_num = vaMaxNumImageFormats(dpy);
//   VAImageFormat *va_img_fmts;
//   VA_CALL(vaQueryImageFormats, dpy, va_img_fmts, &va_img_fmt_num);   
//   for(i = 0; i < va_img_fmt_num; i++)
//     printVaImgFmt(va_img_fmts[i]);

   /* vaCreateImage
    * Create a VAImage structure The width and height fields returned in the 
    * VAImage structure may get enlarged for some YUV formats. 
    * Upon return from this function, image->buf has been created and proper 
    * storage allocated by the library. The client can access the image 
    * through the Map/Unmap calls. 
    */
  //YUV420
//  VAImageFormat img_fmt = {
//   .fourcc = VA_FOURCC_NV12,
//   };
//  VAImage *img;
//  VA_CALL(vaCreateImage, dpy, &img_fmt, 800, 600, img);
//  getchar();  


  VA_CALL(vaDestroyConfig, dpy, config_id);
//  VA_CALL(vaTerminate, dpy); Does not work :((((
// object_heap_destroy: Assertion `obj->next_free != -2' failed.
  return 0;
  
error:
  return -1;  
  
}

