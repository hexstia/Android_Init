/*
 * memory optimization code
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <errno.h>
#include <stdarg.h>
#include <mtd/mtd-user.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <selinux/selinux.h>
#include <selinux/label.h>
#include <selinux/android.h>

#include <libgen.h>

#include <cutils/list.h>
#include <cutils/android_reboot.h>
#include <cutils/sockets.h>
#include <cutils/iosched_policy.h>
#include <cutils/fs.h>
#include <private/android_filesystem_config.h>
#include <termios.h>

#include "devices.h"
#include "init.h"
#include "log.h"
#include "property_service.h"
#include "bootchart.h"
#include "signal_handler.h"
#include "keychords.h"
#include "init_parser.h"
#include "util.h"
#include "ueventd.h"
#include "watchdogd.h"

int g_total_mem = 0; /* unit: MB */

static int get_dram_size(void)
{
#define MEMINFO_NODE	"/proc/meminfo"
	FILE *fd;
	char data[128], *tmp;
	int dram_size = 1024;

	fd = fopen(MEMINFO_NODE, "r");
	if (fd == NULL) {
		ERROR("cannot open %s, return default 1G\n", MEMINFO_NODE);
		goto end;
	}

	while (fgets(data, sizeof(data), fd)) {
		if (strstr(data, "MemTotal")) {
			tmp = strchr(data, ':') + 1;
            dram_size = atoi(tmp) >> 10; /* convert to MBytes */
            break;
        }
    }

	fclose(fd);
end:
    INFO("%s: return %d\n", __func__, dram_size);
	return dram_size;
}

static bool get_lcd_resolution(int *width, int *height)
{
#define LCD_X_STR			"lcd_x          int       "
#define LCD_Y_STR			"lcd_y          int       "
#define DISP_SYS_NODE		"/sys/class/script/dump"
#define SCRIPT_BUF_SIZE		0x10000
	char *data = NULL, *temp;
	bool ret = false;
	int fd;

	if (!width || !height)
		return false;

	data = malloc(SCRIPT_BUF_SIZE);
	if (!data) {
		ERROR("%s err: alloc data buf failed\n", __func__);
		return false;
	}
	memset(data, 0, SCRIPT_BUF_SIZE);

	fd = open(DISP_SYS_NODE, O_RDWR);
	if (fd < 0) {
		ERROR("%s err: cannot open %s\n", __func__, DISP_SYS_NODE);
		free(data);
		return false;
	}

	if (write(fd, "lcd0_para", strlen("lcd0_para")) < 0) {
		ERROR("%s err: write lcd0_para to %s failed\n", __func__, DISP_SYS_NODE);
		goto end;
	}
	if (read(fd, data, SCRIPT_BUF_SIZE) < 0) {
		ERROR("%s err: read /sys/class/script/dump failed\n", __func__);
		goto end;
	}

	/* get lcd_x */
	temp = strstr(data, LCD_X_STR);
	if (!temp) {
		ERROR("%s err: get lcd_x failed\n", __func__);
		goto end;
	} else
		temp += strlen(LCD_X_STR);
	if (width)
		*width = atoi(temp);

	/* get lcd_y */
	temp = strstr(data, LCD_Y_STR);
	if (!temp) {
		ERROR("%s err: get lcd_y failed\n", __func__);
		goto end;
	} else
		temp += strlen(LCD_Y_STR);
	if (height)
		*height = atoi(temp);

	ret = true;

end:
	close(fd);
	free(data);
	return ret;
}

inline void trim(char *buf)
{
	char *temp, lastch;
	int i = 0, first_valid = false;

	if (!buf || *buf == 0)
		return;

	/* trim tail */
	while ((temp = buf + strlen(buf) - 1) && *temp != 0) {
		if (*temp==' ' || *temp=='\t'
				|| *temp=='\n' || *temp=='\r')
			*temp = 0;
		else
			break;
	}

	if (*buf == 0)
		return;

	/* trim head */
	while (i < (int)strlen(buf)) {
		if (buf[i]==' ' || buf[i]=='\t'
				|| buf[i]=='\n' || buf[i]=='\r') {
			i++;
			continue;
		} else if (buf[i] != 0) {
			strcpy(buf, &buf[i]);
			break;
		} else {
			buf[0] = 0;
			break;
		}
	}
}

#define CONFIG_MEM_FILE		"/config_mem.ini"

void config_item(char *buf)
{
	char data[1024], key[256], value[256];
	bool find = false;
	FILE *fd;
	int len;

	fd = fopen(CONFIG_MEM_FILE, "r");
	if (fd == NULL) {
		ERROR("cannot open %s\n", CONFIG_MEM_FILE);
		return;
	}

	while (!feof(fd)) {
		if (!fgets(data, sizeof(data), fd)) /* eof or read error */
			continue;

		if (strlen(data) >= sizeof(data) - 1) {
			ERROR("%s err: line too long!\n", __func__);
			goto end;
		}

		trim(data);

        if (data[0]=='#' || data[0]==0) /* comment or blank line */
			continue;

		if (!find) {
			if (data[0]=='[' && strstr(data, buf)) {
				find = true;
				continue;
			}
        } else {
			if (data[0]=='[')
				break; /* NEXT item, so break */
			else if (!strstr(data, "=") || data[strlen(data)-1] == '=')
				continue; /* not key=value style, or has no value field */

			len = strlen(data) - strlen(strstr(data, "="));
			strncpy(key, data, len);
			key[len] = '\0';
			trim(key);

			strcpy(value, strstr(data, "=") + 1);
			trim(value);

			INFO("%s: get key->value %s %s\n", __func__, key, value);
			if (key[0] == '/')  { /* file node, as: /sys/class/adj=12 */
				sprintf(data, "echo %s > %s", value, key);
				system(data);
			} else /* property node, as: dalvik.vm.heapsize=184m */
				property_set(key, value);
        }
    }

end:
	fclose(fd);
}

bool get_value_for_key(char *main_key, char *sub_key, char ret_value[], int len)
{
	char data[1024], tmp[256];
	bool find_mainkey = false, ret = false;
	FILE *fd = NULL;

	fd = fopen(CONFIG_MEM_FILE, "r");
	if (fd == NULL) {
		ERROR("cannot open %s\n", CONFIG_MEM_FILE);
		return false;
	}

	while (!feof(fd)) {
		if (!fgets(data, sizeof(data), fd)) /* eof or read error */
			continue;

		if (strlen(data) >= sizeof(data) - 1) {
			ERROR("%s err: line too long!\n", __func__);
			goto end;
		}

		trim(data);

        if (data[0]=='#' || data[0]==0) /* comment or blank line */
			continue;

		if (!find_mainkey) {
			if (data[0]=='[' && !strncmp(data+1, main_key,
				strlen(main_key))) { /* +1 means omit '[' */
				find_mainkey = true;
				continue;
			}
        } else {
			if (data[0]=='[')
				goto end; /* NEXT item, so break */
			else if (!strstr(data, "=") || data[strlen(data)-1] == '=')
				continue; /* not 'key = value' style, or has no value field */

			len = strlen(data) - strlen(strstr(data, "="));
			strncpy(tmp, data, len);
			tmp[len] = '\0';
			trim(tmp);

			if (strcmp(tmp, sub_key))
				continue; /* not subkey */

			strcpy(tmp, strstr(data, "=") + 1);
			trim(tmp);
			if ((int)strlen(tmp) >= len) {
				ERROR("%s err: %s->%s value too long!\n", __func__, main_key, sub_key);
				goto end;
			}
	
			NOTICE("%s: get %s->%s: %s\n", __func__, main_key, sub_key, tmp);
			strcpy(ret_value, tmp);
			ret = true;
			break;
        }
    }

end:
	fclose(fd);
	return ret;
}

void property_opt_for_mem(void)
{
	char buf[PROP_VALUE_MAX] = {0};
	static int width = 0, height = 0;

	NOTICE("%s: start!\n", __func__);
	if(property_get("ro.memopt.disable", buf) && !strcmp(buf,"true")) {
		NOTICE("%s: disable adaptive memory function!\n", __func__);
		return;
	}

	if (!g_total_mem)
		g_total_mem = get_dram_size();

	if (g_total_mem <= 512)
		property_set("ro.config.low_ram", "true");
	else
		property_set("ro.config.low_ram", "false");

	if (!width || !height) {
		if (!get_lcd_resolution(&width, &height)) {
			NOTICE("%s: get lcd resolution failed!\n", __func__);
			return;
		}
	}

	/* dalvik heap para */
	if (g_total_mem <= 512)
		strcpy(buf, "dalvik_512m");
	else if (g_total_mem > 512 && g_total_mem <= 1024)
		strcpy(buf, "dalvik_1024m");
	else if (g_total_mem > 1024 && g_total_mem <= 2048)
		strcpy(buf, "dalvik_2048m");
	config_item(buf);
	/* hwui para */
	sprintf(buf, "hwui_%d", (width > height ? width : height));
	config_item(buf);

	system("echo 12000 > /sys/module/lowmemorykiller/parameters/minfree");
	NOTICE("%s: end!\n", __func__);
}

