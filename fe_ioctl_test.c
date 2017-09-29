#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <misc/sunxi_front_end.h>
#include <drm/drm_fourcc.h>

#define FE_DEV "/dev/sunxi_front_end"

struct sfe_config conf = {
    .input_fmt = DRM_FORMAT_YUV420,
    .output_fmt = DRM_FORMAT_XRGB8888,
    .in_width = 864,
    .in_height = 840,
    .out_width = 864,
    .out_height = 840,
};


int main(int argc, char const **argv) {
    int fd_sfe;
    
    fd_sfe = open(FE_DEV, O_RDWR | O_CLOEXEC);
    if (fd_sfe == -1) {
        printf("Failed opening %s\n", FE_DEV);
        return -1;
    }
    
    if( ioctl(fd_sfe, SFE_IOCTL_TEST) < 0) {
        printf("IOCTL SFE_IOCTL_TES FAILED\n");
    } 

    
    
    if( ioctl(fd_sfe, SFE_IOCTL_SET_CONFIG, &conf) < 0) {
        printf("IOCTL SFE_IOCTL_SET_CONFI FAILED\n");       
    }

    getchar();
    
    close(fd_sfe);
    
    return 0;
}
