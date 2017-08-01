#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>

#include <va/va.h>
#include <va/va_x11.h>
#include <va/va_drm.h>

#define CEDRUS_DRM_DEVICE_PATH "/dev/video0"

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
  va_status = vaInitialize(dpy, &major_version, &minor_version);
  if (VA_STATUS_SUCCESS != va_status) {
    printf("vaInitialize failed\n");
    return -3;
  }

  va_max_num_profiles = vaMaxNumProfiles(dpy);
//  printf("nr profiles = %d. Must be < 0\n", va_max_num_profiles);    

  va_profiles = calloc(va_max_num_profiles, sizeof(VAProfile));
  if(!va_profiles) {
    printf("calloc va_profiles failed\n");  
    return -4;
  }

  // Get supported profiles
  va_status = vaQueryConfigProfiles(dpy, va_profiles, &va_max_num_profiles);
  if (va_status != VA_STATUS_SUCCESS) {
    printf("vaQueryConfigProfiles failed \n");  
    return -5;
  }  

  for (i = 0; i < va_max_num_profiles; i++) {
    printVaProfile(va_profiles[i]);
    
    // Get supported entrypoints
    va_entrypoints_num = vaMaxNumEntrypoints(dpy);
//    printf("\tnr of entry points %d\n", va_entrypoints_num);

    va_entrypoints = calloc(va_entrypoints_num, sizeof(VAEntrypoint));
    if(!va_entrypoints) {
      printf("calloc va_entrypoints failed\n");  
      return -6;
    }

    va_status = vaQueryConfigEntrypoints(dpy, va_profiles[i], va_entrypoints,
      &va_entrypoints_num);
    if (va_status != VA_STATUS_SUCCESS) {
      printf("vaMaxNumEntrypoints failed\n");
                        return -6;  
    }
    
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
  
  
  
  return 0;
}

