# ------------- Linux

targets:
	@grep '^[a-zA-Z0-9\.#-]\+:' Makefile | grep -v '^\.' | grep -v targets | sed 's/:.*//' | uniq | sed 's/\.elf/.hex/' | sed 's/^/Make clean \&\& Make /'

INCLUDES = \
-I./include \
-I./src/ \
-I./src/onp/ \
-I./tests \

LIB_OBJECTS = \
./src/lib/list.c \
./src/lib/value.c \
./src/onp/onp.c \
./src/onf/onf.c \

UNIX_C_SOURCE_FILES = \
./src/platforms/unix/properties.c \
./src/platforms/unix/serial.c \
./src/platforms/unix/channel-serial.c \
./src/platforms/unix/log.c \
./src/platforms/unix/mem.c \
./src/platforms/unix/time.c \
./src/platforms/unix/random.c \

TESTS_OBJECTS = \
./tests/assert.c \
./tests/test-properties.c \
./tests/test-list.c \
./tests/test-value.c \
./tests/test-onf.c \
./tests/main.c \


LINUX_FLAGS=-g3 -ggdb
LINUX_CC_SYMBOLS = -D${TARGET} ${CHANNELS}
CC_FLAGS = -c -std=gnu99 -Werror -Wall -Wextra -Wno-unused-parameter -fno-common -fno-exceptions -ffunction-sections -fdata-sections -fomit-frame-pointer

.c.o:
	$(CC) ${COMPILE_LINE} -o $@ -c $<

linux.library: libOnexKernel.a

libOnexKernel.a: COMPILE_LINE=${LINUX_FLAGS} ${CC_FLAGS} $(LINUX_CC_SYMBOLS) ${INCLUDES}
libOnexKernel.a: CC=/usr/bin/gcc
libOnexKernel.a: LD=/usr/bin/gcc
libOnexKernel.a: AR=/usr/bin/ar
libOnexKernel.a: TARGET=TARGET_LINUX
libOnexKernel.a: CHANNELS=-DONP_CHANNEL_SERIAL
libOnexKernel.a: $(UNIX_C_SOURCE_FILES:.c=.o) ${LIB_OBJECTS:.c=.o}
	$(AR) rcs $@ $^

android.library: libOnexAndroidKernel.a

libOnexAndroidKernel.a: android/onexkernel/src/main/jni/OnexApp.cpp
	(cd android; ./gradlew build)
	cp android/onexkernel/build/intermediates/ndkBuild/debug/obj/local/arm64-v8a/libOnexAndroidKernel.a .

tests.linux: COMPILE_LINE=${LINUX_FLAGS} ${CC_FLAGS} $(LINUX_CC_SYMBOLS) ${INCLUDES}
tests.linux: CC=/usr/bin/gcc
tests.linux: LD=/usr/bin/gcc
tests.linux: TARGET=TARGET_LINUX
tests.linux: CHANNELS=-DONP_CHANNEL_SERIAL
tests.linux: libOnexKernel.a ${TESTS_OBJECTS:.c=.o}
	$(LD) ${TESTS_OBJECTS:.c=.o} -pthread -L. -lOnexKernel -o $@

linux.tests: tests.linux
	./tests.linux

linux.valgrind: tests.linux
	valgrind --leak-check=yes --undef-value-errors=no ./tests.linux

android.tests: android.library
	adb shell pm uninstall network.object.onexkernel || echo not found
	adb install android/onexkernel/build/outputs/apk/debug/onexkernel-debug.apk
	adb shell pm grant network.object.onexkernel android.permission.READ_EXTERNAL_STORAGE
	adb shell pm grant network.object.onexkernel android.permission.WRITE_EXTERNAL_STORAGE
	adb shell rm -f sdcard/Onex/onex.ondb
	adb logcat OnexApp:D *:S

#############################:

clean:
	-find src tests -name '*.o' -o -name '*.d' | xargs rm -f
	rm -f ,* core
	find . -name onex.ondb | xargs rm -f
	@echo "------------------------------"

cleanx: clean
	rm -f *.linux
	rm -rf android/*/build android/*/.cxx/ android/.gradle/*/*

cleanlibs: cleanx
	rm -f libOnex*.a

############################################################################################

