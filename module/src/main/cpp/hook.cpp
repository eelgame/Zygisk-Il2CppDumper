//
// Created by Perfare on 2020/7/4.
//

#include "hook.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <dobby.h>
#include "il2cpp_dump.h"
#include "game.h"
#include <dirent.h>

int isGame(JNIEnv *env, jstring appDataDir) {
    if (!appDataDir)
        return 0;
    const char *app_data_dir = env->GetStringUTFChars(appDataDir, nullptr);
    int user = 0;
    static char package_name[256];
    if (sscanf(app_data_dir, "/data/%*[^/]/%d/%s", &user, package_name) != 2) {
        if (sscanf(app_data_dir, "/data/%*[^/]/%s", package_name) != 1) {
            package_name[0] = '\0';
            LOGW("can't parse %s", app_data_dir);
            return 0;
        }
    }

    game_data_dir = new char[strlen(app_data_dir) + 1];
    strcpy(game_data_dir, app_data_dir);

    if (strcmp(package_name, GamePackageName) == 0) {
        LOGI("detect game: %s", package_name);
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 1;
    } else {
        env->ReleaseStringUTFChars(appDataDir, app_data_dir);
        return 0;
    }
}

static int GetAndroidApiLevel() {
    char prop_value[PROP_VALUE_MAX];
    __system_property_get("ro.build.version.sdk", prop_value);
    return atoi(prop_value);
}

void dlopen_process(const char *name, void *handle) {
    //LOGD("dlopen: %s", name);
    if (!il2cpp_handle) {
        if (strstr(name, "libil2cpp.so")) {
            il2cpp_handle = handle;
            LOGI("Got il2cpp handle!");
        }
    }
}

HOOK_DEF(void*, __loader_dlopen, const char *filename, int flags, const void *caller_addr) {
    void *handle = orig___loader_dlopen(filename, flags, caller_addr);
    dlopen_process(filename, handle);
    return handle;
}

HOOK_DEF(void*, do_dlopen_V24, const char *name, int flags, const void *extinfo,
         void *caller_addr) {
    void *handle = orig_do_dlopen_V24(name, flags, extinfo, caller_addr);
    dlopen_process(name, handle);
    return handle;
}

HOOK_DEF(void*, do_dlopen_V19, const char *name, int flags, const void *extinfo) {
    void *handle = orig_do_dlopen_V19(name, flags, extinfo);
    dlopen_process(name, handle);
    return handle;
}

//int il2cpp_init(const char* domain_name)
HOOK_DEF(int, il2cpp_init, const char* domain_name) {
    auto ret = orig_il2cpp_init(domain_name);
    LOGI("il2cpp_init");

    il2cpp_dump(il2cpp_handle, game_data_dir);

    return ret;
}

//void* (*dlsym_ptr)(void* __handle, const char* __symbol);
HOOK_DEF(void*, dlsym, void* __handle, const char* __symbol) {
    auto ret = orig_dlsym(__handle, __symbol);

    if (strcmp("il2cpp_init", __symbol) == 0)
    {
        LOGI("dlsym %p, %s", __handle, __symbol);
        il2cpp_handle = __handle;
        orig_il2cpp_init = (int (*)(const char* domain_name))ret;
        return (void*)new_il2cpp_init;
    }

    return ret;
}

void *hack_thread(void *arg) {
    LOGI("hack thread: %d", gettid());
    int api_level = GetAndroidApiLevel();
    LOGI("api level: %d", api_level);

//    void* libc_handle = dlopen("libc.so", RTLD_LAZY);
//    LOGI("libc handle: %p", libc_handle);

    void* addr = DobbySymbolResolver("libc.so", "dlsym");
    DobbyHook(addr, (void *) new_dlsym, (void **) &orig_dlsym);

    return nullptr;
}