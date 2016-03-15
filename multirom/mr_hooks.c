/*
 * This file contains device specific hooks.
 * Always enclose hooks to #if MR_DEVICE_HOOKS >= ver
 * with corresponding hook version!
 */
 
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#include <time.h>
#include <dirent.h>
 
#include <log.h>
 
#if MR_DEVICE_HOOKS >= 1
 
int mrom_hook_after_android_mounts(const char *busybox_path, const char *base_path, int type)
{
    INFO("mrom_hook_after_android_mounts: busybox_path=%s, base_path=%s, type=%d", busybox_path, base_path, type);
    if(access("/fstab2.qcom", F_OK) >= 0)
        remove("/fstab2.qcom");
 
    // remove mounting from .rc files
    int res = -1;
    int add_dummy = 0;
    char *l;
    char line[1024];
    char path_in[128];
    char path_out[128];
    FILE *in, *out;
    DIR *d;
    struct dirent *dp;
 
    d = opendir("/");
    if(!d)
        return -1;
 
    while((dp = readdir(d)))
    {
        if(dp->d_type != DT_REG || !strstr(dp->d_name, ".rc"))
            continue;
 
        snprintf(path_out, sizeof(path_out), "/%s", dp->d_name);
        snprintf(path_in, sizeof(path_in), "/%s.new", dp->d_name);
 
        if(rename(path_out, path_in) < 0)
            goto exit;
 
        in = fopen(path_in, "r");
        if(!in)
            goto exit;
 
        out = fopen(path_out, "w");
        if(!out)
        {
            fclose(in);
            goto exit;
        }
 
        while((fgets(line, sizeof(line), in)))
        {
            for(l = line; isspace(*l); ++l);
 
            if (strncmp(l, "on ", 3) == 0)
                add_dummy = 1;
            else if (strstr(l, "mount ") &&
                (strstr(l, "/data") || strstr(l, "/system") || strstr(l, "/cache")))
            {
                if(add_dummy == 1)
                {
		    INFO("found mount in rc file %s: %s", path_out, l);
                    add_dummy = 0;
                    fputs("    export DUMMY_LINE_INGORE_IT 1\n", out);
                }
 
                fputc((int)'#', out);
            }
 
            fputs(line, out);
        }
 
        fclose(out);
        fclose(in);
        remove(path_in);
 
        chmod(path_out, 0750);
    }
 
    res = 0;
exit:
    closedir(d);
    INFO("mrom_hook_after_android_mounts exit: res=%d", res);
    return res;
}
 
#endif /* MR_DEVICE_HOOKS >= 1 */
 
#if MR_DEVICE_HOOKS >= 2
// Screen gets cleared immediatelly after closing the framebuffer on this device,
// give user a while to read the message box until it dissapears.
void mrom_hook_before_fb_close(void)
{
    usleep(800000);
}
#endif /* MR_DEVICE_HOOKS >= 2 */
 
 
#if MR_DEVICE_HOOKS >= 3
static time_t gettime(void)
{
    struct timespec ts;
    int ret;
 
    ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ret < 0) {
        ERROR("clock_gettime(CLOCK_MONOTONIC) failed: %s\n", strerror(errno));
        return 0;
    }
 
    return ts.tv_sec;
}
 
static int wait_for_file(const char *filename, int timeout)
{
    struct stat info;
    time_t timeout_time = gettime() + timeout;
    int ret = -1;
    int waited = 0;
 
    while (gettime() < timeout_time && ((ret = stat(filename, &info)) < 0))
    {
        waited = 1;
        usleep(30000);
    }
 
    if(ret >= 0)
        return waited;
    else
        return ret;
}
 
static void wait_for_mmc(void)
{
    // userdata
    const char *filename = "/sys/devices/msm_sdcc.1/mmc_host/mmc0/mmc0:0001/block/mmcblk0/mmcblk0p16/uevent";
    int ret;
 
    INFO("Waiting for file %s\n", filename);
    ret = wait_for_file(filename, 5);
    if(ret > 0)
    {
        // had to wait, make sure the init is complete
        usleep(100000);
    }
    else if(ret < 0)
    {
        ERROR("Timeout while waiting for %s!\n", filename);
    }
}
 
void tramp_hook_before_device_init(void)
{
    // Some kernels are too fast and mmcblk initialization
    // occurs a bit too late, wait for it.
    wait_for_mmc();
}
#endif /* MR_DEVICE_HOOKS >= 3 */

#if MR_DEVICE_HOOKS >= 4
int mrom_hook_allow_incomplete_fstab(void) {
    return 1;
}
#endif
