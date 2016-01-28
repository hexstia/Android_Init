
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>

#include <cutils/misc.h>
#include <cutils/sockets.h>
#include <cutils/multiuser.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <private/android_filesystem_config.h>

#include <selinux/selinux.h>
#include <selinux/label.h>

#include "log.h"

#define AW_BOOST_UP_DRAM "/sys/class/devfreq/sunxi-ddrfreq/dsm/scene"
#define AW_BOOST_UP_CPUS "/sys/devices/platform/sunxi-budget-cooling/roomage"
#define AW_BOOST_UP_TASK "/dev/cpuctl/tasks"
#define AW_BOOST_UP_GPU  "/sys/devices/platform/pvrsrvkm/dvfs/android"

#ifdef SUN9IW1P1
#define AW_BOOST_UP_CPUS_PERF       "1200000 1 1608000 2 1200000 4 1608000 4 0"
#define AW_BOOST_UP_CPUS_NORMAL     "0       0       0 0 1200000 4 1608000 4 0"
#define AW_BOOST_UP_CPUS_VIDEO      "0       0       0 0 1200000 4 1608000 4 0"
#elif defined SUN8IW5P1
#define AW_BOOST_UP_CPUS_PERF       "1008000 4 0 0 1200000 4 0 0 0"
#define AW_BOOST_UP_CPUS_NORMAL     "0 0 0 0 1200000 4 0 0 0"
#define AW_BOOST_UP_CPUS_VIDEO      "0 0 0 0 1200000 4 0 0 2"
#elif  defined SUN8IW6P1
#define AW_BOOST_UP_CPUS_PERF       "1608000 4 1608000 0 2016000 4 2016000 4 0"
#define AW_BOOST_UP_CPUS_NORMAL     "0 0 0 0 2016000 4 2016000 4 0"
#define AW_BOOST_UP_CPUS_VIDEO      "0 0 0 0 2016000 4 2016000 4 0"
#endif

/* dram scene value defined */
#define AW_BOOST_UP_DRAM_DEFAULT        "0"
#define AW_BOOST_UP_DRAM_HOME           "1"
#define AW_BOOST_UP_DRAM_LOCALVIDEO     "2"
#define AW_BOOST_UP_DRAM_BGMUSIC        "3"
#define AW_BOOST_UP_DRAM_4KLOCALVIDEO   "4"

/* gpu scene value defined */
#define AW_BOOST_UP_GPU_DEFAULT         "4\n"
#define AW_BOOST_UP_GPU_HOME            "4\n"
#define AW_BOOST_UP_GPU_LOCALVIDEO      "4\n"
#define AW_BOOST_UP_GPU_BGMUSIC         "4\n"
#define AW_BOOST_UP_GPU_4KLOCALVIDEO    "4\n"
#define AW_BOOST_UP_GPU_PERF            "8\n"

static int boost_up_dram_fd = -1;
static int boost_up_cpus_fd = -1;
static int boost_up_task_fd = -1;
static int boost_up_gpu_fd = -1;
static bool BOOST_UP_DEBUG = true;
static int runmode = 0xa7;

/*
   struct scene_desc {
   int scene_id;
   int scene_fd;
   char data[0];
   };
   */

/*
 * benchmark list order by index
 *
 * 1.com.antutu.ABenchMark
 * 2.com.qihoo360.mobilesafe.opti
 * 3.com.qihoo360.mobilesafe.bench
 * 4.com.glbenchmark.glbenchmark27
 * 5.com.glbenchmark.glbenchmark30
 * 6.com.ludashi.benchmark
 * 7.com.rightware.tdmm2v10jni.free
 * 8.com.futuremark.dmandroid.application
 * 9.com.greenecomputing.linpack
 * 10.com.tactel.electopia
 * 11.eu.chainfire.cfbench
 * 12.com.quicinc.vellamo
 * 13.com.aurorasoftworks.quadrant.ui.advanced
 * 14.com.thread.jpct.bench
 * 15.se.nena.nenamark2
 * 16.com.epicgames.EpicCitadel
 * 17.com.ixia.ixchariot
 * 18.com.magicandroidapps.iperf
 * 19.com.qqfriends.com.music
 * 20.com.powervr.Cat
 * 21.com.antutu.AbenchMark5
 * 22.
 * 23.
 * 24.
 */

#ifdef SUN9IW1P1
const char *roomage_a7[] = {
    "912000 2 1296000 2 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "1200000 1 1608000 4 1200000 4 1800000 4 1",
    "912000 4 1296000 4 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "912000 1 1296000 3 1200000 4 1800000 4 1",
    "1200000 1 1608000 0 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "912000 1 1296000 0 1200000 4 1800000 4 1",
    "1200000 1 1608000 1 1200000 4 1800000 4 1",
    "1200000 1 1608000 1 1200000 4 1800000 4 1",
    "1200000 1 1608000 1 1200000 4 1800000 4 1",
    "1200000 1 1608000 1 1200000 4 1800000 4 1",
    "1200000 1 1608000 1 1200000 4 1800000 4 1",
    "1200000 1 1608000 1 1200000 4 1800000 4 1",
    "1200000 1 1608000 1 1200000 4 1800000 4 1",
};

const char *roomage_a15[] = {
    "0 0 1296000 2 1200000 4 1800000 4 1",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1608000 4 1200000 4 1800000 4 1",
    "0 0 1296000 4 1200000 4 1800000 4 0",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1296000 4 1200000 4 1800000 4 1",
    "0 0 1608000 4 1200000 4 1800000 4 1",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1296000 1 1200000 4 1800000 4 1",
    "0 0 1608000 1 1200000 4 1800000 4 1",
    "1200000 4 600000 1 1200000 4 1800000 4 0",
    "1200000 4 600000 1 1200000 4 1800000 4 0",
    "0 0 1296000 2 1200000 4 1800000 4 1",
    "0 0 0 0 1200000 4 1800000 4 1",
    "0 0 1296000 2 1200000 4 1800000 4 1",
    "0 0 1296000 2 1200000 4 1800000 4 1",
};
#elif defined SUN8IW5P1
const char *roomage_a7[] = {
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1200000 4 0 0 1",
    "1008000 4 0 0 1200000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
    "1008000 4 0 0 1344000 4 0 0 1",
};
#elif defined SUN8IW6P1
const char *roomage_a7[] = {
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1800000 1 1800000 0 2016000 4 2016000 4 0",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
    "1008000 1 1008000 0 2016000 4 2016000 4 1",
};
#endif

int aw_init_boostup(int mode)
{
    if ((boost_up_dram_fd = open(AW_BOOST_UP_DRAM, O_RDWR)) < 0)
        ERROR("cann't open %s!", AW_BOOST_UP_DRAM);

    if ((boost_up_cpus_fd = open(AW_BOOST_UP_CPUS, O_RDWR)) < 0)
        ERROR("cann't open %s!", AW_BOOST_UP_CPUS);

    if ((boost_up_task_fd = open(AW_BOOST_UP_TASK, O_RDWR)) < 0)
        ERROR("cann't open %s!", AW_BOOST_UP_TASK);

    if ((boost_up_gpu_fd = open(AW_BOOST_UP_GPU, O_RDWR)) < 0)
        ERROR("cann't open %s!", AW_BOOST_UP_GPU);

    runmode = mode;
    return 0;
}

static int aw_get_para(const char *value, int *pid, unsigned int *index)
{
    //value like: mode_xxx,pid,index
    char buf[32];
    int mPid = 0;
    unsigned int mIndex = 0;
    sscanf(value, "%s %d %d", buf, &mPid, &mIndex);
    *pid = mPid;
    *index = mIndex;
    return 0;
}

/**
 * mode_activity
 * mode_rotation
 * mode_extreme
 * mode_bgmusic
 * mode_home
 * mode_localvideo
 * mode_4klocalvideo
 * mode_normal
 * mode_debug
 */
int aw_boost_up_perf(const char *name, const char *value)
{
    int ret = 0;

    if (!strncmp(name, "sys.boost_up_perf.mode", strlen("sys.boost_up_perf.mode"))) {
        int pid = 0;
        unsigned int index = 0;
        aw_get_para(value, &pid, &index);

        if (BOOST_UP_DEBUG)
            ERROR("aw_boost_up_perf to set name:%s, value:%s, pid:%d, index:%d\n", name, value, pid,index);

        if (!strncmp(value, "mode_", strlen("mode_"))) {
            switch(value[5]) {
                //cpu mode_activyty
                case 'a':
                    //cpu mode_rotation
                case 'r':
                    if (boost_up_cpus_fd > 0)
                        ret = write(boost_up_cpus_fd, AW_BOOST_UP_CPUS_PERF, strlen(AW_BOOST_UP_CPUS_PERF));
                    if (boost_up_dram_fd > 0)
                        ret = write(boost_up_dram_fd, AW_BOOST_UP_DRAM_DEFAULT, strlen(AW_BOOST_UP_DRAM_DEFAULT));
                    if (boost_up_gpu_fd > 0) {
                        ret = write(boost_up_gpu_fd, AW_BOOST_UP_GPU_DEFAULT, strlen(AW_BOOST_UP_GPU_DEFAULT));
                    }
                    break;
                    //cpu mode_extreme
                case 'e':
#ifdef SUN9IW1P1
                    if (0xa15 == runmode) {
                        if (index <= 0 || index > sizeof(roomage_a15)/sizeof(*roomage_a15))
                            index = 1;
                        if(boost_up_cpus_fd > 0)
                            ret = write(boost_up_cpus_fd, roomage_a15[index-1], strlen(roomage_a15[index-1]));
                    } else {
#endif
                        if (index <= 0 || (index > sizeof(roomage_a7) / sizeof(*roomage_a7)))
                            index = 1;
                        if (boost_up_cpus_fd > 0)
                            ret = write(boost_up_cpus_fd, roomage_a7[index-1], strlen(roomage_a7[index-1]));
#ifdef SUN9IW1P1
                    }
#endif
                    if (boost_up_task_fd > 0 && pid > 0) {
                        char buf[8];
                        sprintf(buf, "%d", pid);
                        write(boost_up_task_fd, buf, strlen(buf));
                    }
                    if (boost_up_dram_fd > 0)
                        ret = write(boost_up_dram_fd, AW_BOOST_UP_DRAM_DEFAULT, strlen(AW_BOOST_UP_DRAM_DEFAULT));
                    if (boost_up_gpu_fd > 0 && roomage_a7[index-1][strlen(roomage_a7[index-1])-1] == '1') {
                        ret = write(boost_up_gpu_fd, AW_BOOST_UP_GPU_PERF, strlen(AW_BOOST_UP_GPU_PERF));
                    }

                    break;
                    //dram mode_bgmusic
                case 'b':
                    if (boost_up_cpus_fd > 0)
                        ret = write(boost_up_cpus_fd, AW_BOOST_UP_CPUS_NORMAL, strlen(AW_BOOST_UP_CPUS_NORMAL));
                    if (boost_up_dram_fd > 0)
                        ret = write(boost_up_dram_fd, AW_BOOST_UP_DRAM_BGMUSIC, strlen(AW_BOOST_UP_DRAM_BGMUSIC));
                    if (boost_up_gpu_fd > 0) {
                        ret = write(boost_up_gpu_fd, AW_BOOST_UP_GPU_BGMUSIC, strlen(AW_BOOST_UP_GPU_BGMUSIC));
                    }

                    break;
                    //dram mode_home
                case 'h':
                    if (boost_up_cpus_fd > 0)
                        ret = write(boost_up_cpus_fd, AW_BOOST_UP_CPUS_NORMAL, strlen(AW_BOOST_UP_CPUS_NORMAL));
                    if (boost_up_dram_fd > 0)
                        ret = write(boost_up_dram_fd, AW_BOOST_UP_DRAM_HOME, strlen(AW_BOOST_UP_DRAM_HOME));
                    if (boost_up_gpu_fd > 0) {
                        ret = write(boost_up_gpu_fd, AW_BOOST_UP_GPU_HOME, strlen(AW_BOOST_UP_GPU_HOME));
                    }
                    break;
                    //dram mode_localvideo
                case 'l':
                    if (boost_up_cpus_fd > 0)
                        ret = write(boost_up_cpus_fd, AW_BOOST_UP_CPUS_VIDEO, strlen(AW_BOOST_UP_CPUS_VIDEO));
                    /*
                    if (boost_up_dram_fd > 0)
                        ret = write(boost_up_dram_fd, AW_BOOST_UP_DRAM_LOCALVIDEO, strlen(AW_BOOST_UP_DRAM_LOCALVIDEO));
                    if (boost_up_gpu_fd > 0 && roomage_a7[index-1][strlen(roomage_a7[index-1])-1] == '1') {
                        temp = AW_BOOST_UP_GPU_LOCALVIDEO;
                        ret = write(boost_up_gpu_fd, AW_BOOST_UP_GPU_LOCALVIDEO, strlen(AW_BOOST_UP_GPU_LOCALVIDEO));
                    }
                    */
                    break;
                case '4':
                    if(boost_up_cpus_fd > 0){
                        ret = write(boost_up_cpus_fd, AW_BOOST_UP_CPUS_VIDEO, strlen(AW_BOOST_UP_CPUS_VIDEO));
                    }
                    /*
                    if (boost_up_dram_fd > 0)
                       ret = write(boost_up_dram_fd, AW_BOOST_UP_DRAM_4KLOCALVIDEO, strlen(AW_BOOST_UP_DRAM_4KLOCALVIDEO));
                    if (boost_up_gpu_fd > 0 && roomage_a7[index-1][strlen(roomage_a7[index-1])-1] == '1') {
                        ret = write(boost_up_gpu_fd, AW_BOOST_UP_GPU_LOCALVIDEO, strlen(AW_BOOST_UP_GPU_LOCALVIDEO));
                    }
                       */
                    break;
                    //cpu mode_normal
                case 'n':
                    if (boost_up_cpus_fd > 0)
                        ret = write(boost_up_cpus_fd, AW_BOOST_UP_CPUS_NORMAL, strlen(AW_BOOST_UP_CPUS_NORMAL));
                    //dram mode_default
                    if (boost_up_dram_fd > 0)
                        ret = write(boost_up_dram_fd, AW_BOOST_UP_DRAM_DEFAULT, strlen(AW_BOOST_UP_DRAM_DEFAULT));

                    if (boost_up_gpu_fd > 0) {
                        ret = write(boost_up_gpu_fd, AW_BOOST_UP_GPU_DEFAULT, strlen(AW_BOOST_UP_GPU_DEFAULT));
                    }

                    break;
                case 'd':
                    BOOST_UP_DEBUG = true;
                    break;
                default:
                    ERROR("aw_boost_perf set invalid value:%s\n", value);
                    break;
            }
        }
        if (BOOST_UP_DEBUG)
            ERROR("aw_boost_perf ret:%d, dram_fd:%d, cpus_fd:%d, task_fd:%d, gpu_fd:%d\n", ret, boost_up_dram_fd, boost_up_cpus_fd, boost_up_task_fd, boost_up_gpu_fd);
    }

    return 0;
}
