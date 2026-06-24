#!/bin/bash

PLATFORMS=()
while getopts "p:" o; do
    case "$o" in
        p)
            PLATFORMS+=(${OPTARG})
            ;;
    esac
done

if [ $OPTIND -eq 1 ]; then
    PLATFORMS+=("opencr1")
    PLATFORMS+=("teensy4")
    PLATFORMS+=("teensy32")
    PLATFORMS+=("teensy35")
    PLATFORMS+=("teensy36")
    PLATFORMS+=("cortex_m0")
    PLATFORMS+=("cortex_m3")
    PLATFORMS+=("cortex_m4")
    # PLATFORMS+=("portenta-m4")
    PLATFORMS+=("portenta-m7")
    PLATFORMS+=("kakutef7-m7")
    PLATFORMS+=("esp32")
fi

shift $((OPTIND-1))

######## Init ########

apt update

cd /uros_ws

source /opt/ros/$ROS_DISTRO/setup.bash
source install/local_setup.bash

ros2 run micro_ros_setup create_firmware_ws.sh generate_lib

######## Adding extra packages ########
pushd firmware/mcu_ws > /dev/null

    # Workaround: Copy just tf2_msgs
    git clone -b jazzy https://github.com/ros2/geometry2
    cp -R geometry2/tf2_msgs ros2/tf2_msgs
    rm -rf geometry2

    # Import user defined packages
    mkdir extra_packages
    pushd extra_packages > /dev/null
        cp -R /project/extras/library_generation/extra_packages/* .
        vcs import --input extra_packages.repos
    popd > /dev/null

popd > /dev/null

######## Clean and source ########
find /project/src/ ! -name micro_ros_arduino.h ! -name *.c ! -name *.cpp ! -name *.c.in -delete

######## Build for OpenCR  ########
if [[ " ${PLATFORMS[@]} " =~ " opencr1 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/gcc-arm-none-eabi-5_4-2016q2/bin/arm-none-eabi-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/opencr_toolchain.cmake /project/extras/library_generation/colcon.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/cortex-m7/fpv5-sp-d16-softfp
    cp -R firmware/build/libmicroros.a /project/src/cortex-m7/fpv5-sp-d16-softfp/libmicroros.a
fi

######## Build for SAMD (e.g. Arduino Zero) ########
if [[ " ${PLATFORMS[@]} " =~ " cortex_m0 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/gcc-arm-none-eabi-7-2017-q4-major/bin/arm-none-eabi-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/cortex_m0_toolchain.cmake /project/extras/library_generation/colcon_verylowmem.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/cortex-m0plus
    cp -R firmware/build/libmicroros.a /project/src/cortex-m0plus/libmicroros.a
fi

######## Build for SAM (e.g. Arduino Due) ########
if [[ " ${PLATFORMS[@]} " =~ " cortex_m3 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/gcc-arm-none-eabi-4_8-2014q1/bin/arm-none-eabi-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/cortex_m3_toolchain.cmake /project/extras/library_generation/colcon_lowmem.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/cortex-m3
    cp -R firmware/build/libmicroros.a /project/src/cortex-m3/libmicroros.a
fi

######## Build for STM32F4 ########
if [[ " ${PLATFORMS[@]} " =~ " cortex_m4 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/gcc-arm-none-eabi-9-2020-q2-update/bin/arm-none-eabi-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/cortex_m4_toolchain.cmake /project/extras/library_generation/colcon_lowmem.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/cortex-m4
    cp -R firmware/build/libmicroros.a /project/src/cortex-m4/libmicroros.a
fi

######## Build for Teensy 3.2 ########
if [[ " ${PLATFORMS[@]} " =~ " teensy32 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/teensy-compile/tools/arm/bin/arm-none-eabi-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/teensy32_toolchain.cmake /project/extras/library_generation/colcon_lowmem.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/mk20dx256
    cp -R firmware/build/libmicroros.a /project/src/mk20dx256/libmicroros.a
fi

######## Build for Teensy 3.5 ########
if [[ " ${PLATFORMS[@]} " =~ " teensy35 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/teensy-compile/tools/arm/bin/arm-none-eabi-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/teensy35_toolchain.cmake /project/extras/library_generation/colcon_lowmem.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/mk64fx512/fpv4-sp-d16-hard
    cp -R firmware/build/libmicroros.a /project/src/mk64fx512/fpv4-sp-d16-hard/libmicroros.a
fi

######## Build for Teensy 3.6 ########
if [[ " ${PLATFORMS[@]} " =~ " teensy36 " ]]; then
    rm -rf firmware/build
    mkdir -p /project/src/mk66fx1m0/fpv4-sp-d16-hard

    # Reuse Teensy 3.5 build if possible
    if [[ " ${PLATFORMS[@]} " =~ " teensy35 " ]]; then
        ln /project/src/mk64fx512/fpv4-sp-d16-hard/libmicroros.a /project/src/mk66fx1m0/fpv4-sp-d16-hard/libmicroros.a
    else
        export TOOLCHAIN_PREFIX=/uros_ws/teensy-compile/tools/arm/bin/arm-none-eabi-
        ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/teensy35_toolchain.cmake /project/extras/library_generation/colcon_lowmem.meta

        find firmware/build/include/ -name "*.c"  -delete
        cp -R firmware/build/include/* /project/src/

        cp -R firmware/build/libmicroros.a /project/src/mk66fx1m0/fpv4-sp-d16-hard/libmicroros.a
    fi
fi

######## Build for Teensy 4 ########
if [[ " ${PLATFORMS[@]} " =~ " teensy4 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/teensy-compile/tools/arm/bin/arm-none-eabi-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/teensy4_toolchain.cmake /project/extras/library_generation/colcon.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/imxrt1062/fpv5-d16-hard
    cp -R firmware/build/libmicroros.a /project/src/imxrt1062/fpv5-d16-hard/libmicroros.a
fi

######## Build for Arduino Portenta M4 core ########
# if [[ " ${PLATFORMS[@]} " =~ " portenta-m4 " ]]; then
#     rm -rf firmware/build

#     export TOOLCHAIN_PREFIX=/uros_ws/gcc-arm-none-eabi-7-2017-q4-major/bin/arm-none-eabi-
#     ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/portenta-m4_toolchain.cmake /project/extras/library_generation/colcon.meta

#     find firmware/build/include/ -name "*.c"  -delete
#     cp -R firmware/build/include/* /project/src/

#     mkdir -p /project/src/cortex-m4/fpv4-sp-d16-softfp
#     cp -R firmware/build/libmicroros.a /project/src/cortex-m4/fpv4-sp-d16-softfp/libmicroros.a
# fi

######## Build for Arduino Portenta M7 core ########
if [[ " ${PLATFORMS[@]} " =~ " portenta-m7 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/gcc-arm-none-eabi-7-2017-q4-major/bin/arm-none-eabi-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/portenta-m7_toolchain.cmake /project/extras/library_generation/colcon.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/cortex-m7/fpv5-d16-softfp
    cp -R firmware/build/libmicroros.a /project/src/cortex-m7/fpv5-d16-softfp/libmicroros.a
fi

######## Build for Kakute F7 M7 core  ########
if [[ " ${PLATFORMS[@]} " =~ " kakutef7-m7 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/gcc-arm-none-eabi-9-2020-q2-update/bin/arm-none-eabi-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/kakutef7-m7_toolchain.cmake /project/extras/library_generation/colcon.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/cortex-m7/fpv5-sp-d16-hardfp
    cp -R firmware/build/libmicroros.a /project/src/cortex-m7/fpv5-sp-d16-hardfp/libmicroros.a
fi

######## Build for ESP32  ########
if [[ " ${PLATFORMS[@]} " =~ " esp32 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/xtensa-esp32-elf/bin/xtensa-esp32-elf-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/esp32_toolchain.cmake /project/extras/library_generation/colcon.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/esp32
    cp -R firmware/build/libmicroros.a /project/src/esp32/libmicroros.a
fi

######## Build for ESP32S3  ######
if [[ " ${PLATFORMS[@]} " =~ " esp32s3 " ]]; then
    rm -rf firmware/build

    export TOOLCHAIN_PREFIX=/uros_ws/xtensa-esp32-elf/bin/xtensa-esp32-elf-
    ros2 run micro_ros_setup build_firmware.sh /project/extras/library_generation/esp32_toolchain.cmake /project/extras/library_generation/colcon.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/esp32s3
    cp -R firmware/build/libmicroros.a /project/src/esp32s3/libmicroros.a
fi

######## Build for 86Duino (FreeDOS / Vortex86EX2, DJGPP) ########
# The DJGPP toolchain is not included in the image; instead, a mounted Linux x86-64 ELF version is used
# (located in /project/86Duino/djgpp). Its toolchain.cmake and colcon.meta are also under 86Duino/.
if [[ " ${PLATFORMS[@]} " =~ " 86duino " ]]; then
    rm -rf firmware/build

    export DJGPP_ROOT=/project/86Duino/djgpp
    export PATH=$DJGPP_ROOT/bin:$DJGPP_ROOT/i586-pc-msdosdjgpp/bin:$PATH
    export DJDIR=$DJGPP_ROOT/i586-pc-msdosdjgpp        # DJGPP uses this to find sys-include
    export GCC_EXEC_PREFIX=$DJGPP_ROOT/lib/gcc/
    export TOOLCHAIN_PREFIX=$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-

    ######## Auto-download the DJGPP toolchain if missing ########
    # The toolchain is NOT committed to git (too large). On first run it is fetched
    # from build-djgpp and extracted to /project/86Duino/djgpp; the mounted /project
    # volume caches it so subsequent runs skip the download.
    DJGPP_URL=https://github.com/andrewwutw/build-djgpp/releases/download/v3.0/djgpp-linux64-gcc830.tar.bz2
    if [ ! -x "$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-gcc" ]; then
        echo "[86duino] DJGPP toolchain not found, downloading from $DJGPP_URL ..."
        if ! command -v wget > /dev/null 2>&1; then
            apt update && apt -y install wget
        fi
        wget -q --show-progress -O /tmp/djgpp.tar.bz2 "$DJGPP_URL"
        # The archive extracts to a top-level "djgpp/" dir -> /project/86Duino/djgpp
        mkdir -p /project/86Duino
        tar -xjf /tmp/djgpp.tar.bz2 -C /project/86Duino
        rm -f /tmp/djgpp.tar.bz2
        if [ ! -x "$DJGPP_ROOT/bin/i586-pc-msdosdjgpp-gcc" ]; then
            echo "[86duino] ERROR: DJGPP download/extract failed; aborting." >&2
            exit 1
        fi
        echo "[86duino] DJGPP toolchain ready at $DJGPP_ROOT"
    fi

    # Ensure the mounted toolchain is executable (git/mounting often fails +x)
    chmod -R +x $DJGPP_ROOT/bin $DJGPP_ROOT/i586-pc-msdosdjgpp/bin 2>/dev/null || true

    ######## Applying DJGPP-specific source code for patching ########
    # Source is in /project/86Duino/FixForDJGPPFiles, target path is in the README.txt of that folder.
    MCU_WS=/uros_ws/firmware/mcu_ws
    FIX=/project/86Duino/FixForDJGPPFiles

    # (1~3) Overwrite existing files (same name and path, already recognized by the build system, just overwrite)
    cp -v $FIX/process.c  $MCU_WS/uros/rcutils/src/process.c
    cp -v $FIX/time.c     $MCU_WS/eProsima/Micro-XRCE-DDS-Client/src/c/util/time.c
    cp -v $FIX/traits.hpp $MCU_WS/ros2/rosidl/rosidl_runtime_cpp/include/rosidl_runtime_cpp/traits.hpp

    # (4) New file time_djgpp.c: Copy into rcutils and replace time_unix.c in CMakeLists with it
    #     (Three files with the same definition) time_now function, one for one to avoid duplicate symbols.
    cp -v $FIX/time_djgpp.c $MCU_WS/uros/rcutils/src/time_djgpp.c
    RCUTILS_CMAKE=$MCU_WS/uros/rcutils/CMakeLists.txt
    if grep -q "time_unix.c" "$RCUTILS_CMAKE"; then
        sed -i 's#\(src/\)\?time_unix\.c#src/time_djgpp.c#' "$RCUTILS_CMAKE"
        echo "[86duino] patched rcutils CMakeLists: time_unix.c -> time_djgpp.c"
    else
        echo "[86duino] WARNING: 'time_unix.c' not found in $RCUTILS_CMAKE; time_djgpp.c may not be compiled"
    fi

    ros2 run micro_ros_setup build_firmware.sh /project/86Duino/djgpp_toolchain.cmake /project/86Duino/colcon.meta

    find firmware/build/include/ -name "*.c"  -delete
    cp -R firmware/build/include/* /project/src/

    mkdir -p /project/src/86duino
    cp -R firmware/build/libmicroros.a /project/src/86duino/libmicroros.a
fi

######## Fix include paths  ########
pushd firmware/mcu_ws > /dev/null
    INCLUDE_ROS2_PACKAGES=$(colcon list | awk '{print $1}' | awk -v d=" " '{s=(NR==1?s:s d)$0}END{print s}')
popd > /dev/null

apt -y install rsync
for var in ${INCLUDE_ROS2_PACKAGES}; do
    rsync -r /project/src/${var}/${var}/* /project/src/${var}/ > /dev/null 2>&1
    rm -rf /project/src/${var}/${var}/ > /dev/null 2>&1
done

######## Generate extra files ########
find firmware/mcu_ws/ros2 \( -name "*.srv" -o -name "*.msg" -o -name "*.action" \) | awk -F"/" '{print $(NF-2)"/"$NF}' > /project/available_ros2_types
find firmware/mcu_ws/extra_packages \( -name "*.srv" -o -name "*.msg" -o -name "*.action" \) | awk -F"/" '{print $(NF-2)"/"$NF}' >> /project/available_ros2_types
# sort it so that the result order is reproducible
sort -o /project/available_ros2_types /project/available_ros2_types

cd firmware
echo "" > /project/built_packages
for f in $(find $(pwd) -name .git -type d); do pushd $f > /dev/null; echo $(git config --get remote.origin.url) $(git rev-parse HEAD) >> /project/built_packages; popd > /dev/null; done;
# sort it so that the result order is reproducible
sort -o /project/built_packages /project/built_packages

######## Fix permissions ########
sudo chmod -R 777 .