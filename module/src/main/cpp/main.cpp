#include <cstring>
#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include "hook.h"
#include "zygisk.hpp"

#include "string"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        env_ = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) {
            LOGE("Skip unknown process");
            return;
        }
        enable_hack = isGame(env_, args->app_data_dir);
    }

    void postAppSpecialize(const AppSpecializeArgs * args) override {
        const char *app_data_dir = env_->GetStringUTFChars(args->app_data_dir, nullptr);

        std::string path;
        path.append(app_data_dir).append("/lib/libil2cpp.so");

//        LOGI("libil2cpp.so: %s, %d", path.c_str(), access(path.c_str(), F_OK));

        env_->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);

        if (access(path.c_str(), F_OK) == 0) {
            int ret;
            pthread_t ntid;
            if ((ret = pthread_create(&ntid, nullptr, hack_thread, nullptr))) {
                LOGE("can't create thread: %s\n", strerror(ret));
            }
        }
    }

private:
    JNIEnv *env_{};
};

REGISTER_ZYGISK_MODULE(MyModule)