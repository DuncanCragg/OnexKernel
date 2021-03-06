
LOCAL_PATH := $(call my-dir)/../../../..

######################################################
include $(CLEAR_VARS)

LOCAL_MODULE := OnexAndroidKernel

LOCAL_C_INCLUDES :=
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../src/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../src/onp/

LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/../src/platforms/android/*.c) \
                   $(wildcard $(LOCAL_PATH)/../src/platforms/unix/time.c) \
                   $(wildcard $(LOCAL_PATH)/../src/platforms/unix/random.c) \
                   $(wildcard $(LOCAL_PATH)/../src/platforms/unix/properties.c) \
                   $(wildcard $(LOCAL_PATH)/../src/platforms/unix/mem.c) \
                   $(wildcard $(LOCAL_PATH)/../src/lib/*.c) \
                   $(wildcard $(LOCAL_PATH)/../src/on[fp]/*.c) \

LOCAL_CFLAGS := -std=c11
LOCAL_CFLAGS += -D__STDC_LIMIT_MACROS
LOCAL_CFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_CFLAGS += -DONP_CHANNEL_SERIAL

include $(BUILD_STATIC_LIBRARY)

######################################################
include $(CLEAR_VARS)

LOCAL_MODULE := onexkernel

LOCAL_C_INCLUDES :=
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../src/
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../tests/

LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/../tests/assert.c) \
                   $(wildcard $(LOCAL_PATH)/../tests/test-*.c) \
                   $(wildcard $(LOCAL_PATH)/onexkernel/src/main/jni/Files.cpp) \
                   $(wildcard $(LOCAL_PATH)/onexkernel/src/main/jni/OnexApp.cpp) \

LOCAL_CPPFLAGS := -std=c++11
LOCAL_CPPFLAGS += -D__STDC_LIMIT_MACROS
LOCAL_CPPFLAGS += -DVK_NO_PROTOTYPES
LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR

LOCAL_LDLIBS := -landroid -llog -lz

LOCAL_STATIC_LIBRARIES += android_native_app_glue
LOCAL_STATIC_LIBRARIES += cpufeatures
LOCAL_STATIC_LIBRARIES += OnexAndroidKernel

LOCAL_DISABLE_FORMAT_STRING_CHECKS := true
LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true

include $(BUILD_SHARED_LIBRARY)

$(call import-module, android/native_app_glue)
$(call import-module, android/cpufeatures)
