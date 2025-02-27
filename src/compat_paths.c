// compat_paths addresses following compatibility issue:
// topic: default search paths for config file
// issue: Linux and Windows use different common paths for config files
// solution: provide specific default paths for each system

#ifndef _WIN32
// Linux variant

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "compat_paths.h"

uint8_t **compat_get_default_conf_paths(void)
{
    static uint8_t *paths[4] = { NULL };
    static uint8_t buf[256] = "";
    uint8_t *env_config_home = getenv("XDG_CONFIG_HOME");
    if (!paths[0]) {
        paths[0] = "rtl_433.conf";
        if (env_config_home && *env_config_home)
            snprintf(buf, sizeof(buf), "%s%s", env_config_home, "/rtl_433/rtl_433.conf");
        else
            snprintf(buf, sizeof(buf), "%s%s", getenv("HOME"), "/.config/rtl_433/rtl_433.conf");
        paths[1] = buf;
#define STR_VALUE(arg) #arg
#define STR_EXPAND(s)  STR_VALUE(s)
        paths[2] = STR_EXPAND(INSTALL_SYSCONFDIR) "/rtl_433/rtl_433.conf";
        paths[3] = NULL;
    };
    return paths;
}

#else
// Windows variant

#include <stdbool.h>
#include <stddef.h>
#include <shlobj.h>

#include "compat_paths.h"

uint8_t **compat_get_default_conf_paths(void)
{
    static uint8_t bufs[3][256];
    static uint8_t *paths[4] = { NULL };
    if (paths[0]) return paths;
    // Working directory, i.e. where the binary is located
    if (GetModuleFileName(NULL, bufs[0], sizeof(bufs[0]))) {
        uint8_t *last_backslash = strrchr(bufs[0], '\\');
        if (last_backslash)
            *last_backslash = '\0';
        strcat_s(bufs[0], sizeof(bufs[0]), "\\rtl_433.conf");
        paths[0] = bufs[0];
    }
    else {
        paths[0] = NULL;
    }
    // Local per user configuration files %LocalAppData% (e.g. Win7: C:\Users\myusername\AppData\Local\rtl_433\rtl_433.conf)
    if (SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, bufs[1]) == S_OK) {
        strcat_s(bufs[1], sizeof(bufs[1]), "\\rtl_433\\rtl_433.conf");
        paths[1] = bufs[1];
    }
    else {
        paths[1] = NULL;
    }
    // Per machine configuration data %ProgramData% (e.g. Win7: C:\ProgramData\rtl_433\rtl_433.conf)
    if (SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, bufs[2]) == S_OK) {
        strcat_s(bufs[2], sizeof(bufs[2]), "\\rtl_433\\rtl_433.conf");
        paths[2] = bufs[2];
    }
    else {
        paths[2] = NULL;
    }
    paths[3] = NULL;
    return paths;
}
#endif // _WIN32 / !_WIN32
