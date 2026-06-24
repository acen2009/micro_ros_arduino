# djgpp_toolchain.cmake
# Cross-compilation toolchain for 86Duino (FreeDOS / Vortex86EX2) using DJGPP.
#
# This file is meant to be used INSIDE the micro_ros_static_library_builder
# Docker container, where the DJGPP toolchain is mounted at /project/86Duino/djgpp
# and library_generation.sh has exported TOOLCHAIN_PREFIX / DJDIR / GCC_EXEC_PREFIX.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i586)   # 對應 i586-pc-msdosdjgpp，Vortex86EX2 為 i586 等級

set(CMAKE_CROSSCOMPILING 1)
# 交叉編譯時 CMake 預設用可執行檔做 try-compile，DJGPP 產出 DOS 執行檔無法在 host 跑，
# 改用靜態庫做編譯器測試，並直接宣告編譯器可用，避免偵測階段失敗。
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 編譯器設定：用 library_generation.sh 匯出的 TOOLCHAIN_PREFIX（絕對路徑），
# 比依賴 PATH 上的裸名稱穩定。
set(CMAKE_C_COMPILER   $ENV{TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER $ENV{TOOLCHAIN_PREFIX}g++)
set(CMAKE_AR           $ENV{TOOLCHAIN_PREFIX}ar)
set(CMAKE_RANLIB       $ENV{TOOLCHAIN_PREFIX}ranlib)
set(CMAKE_STRIP        $ENV{TOOLCHAIN_PREFIX}strip)

set(CMAKE_C_COMPILER_WORKS   1 CACHE INTERNAL "")
set(CMAKE_CXX_COMPILER_WORKS 1 CACHE INTERNAL "")

# 告訴 Micro-XRCE-DDS-Client 這是哪個 Generic 平台
# FreeDOS 不在內建清單，transport 由 Custom Transport 處理
set(PLATFORM_NAME "FreeDOS" CACHE STRING "Target platform name")

# DJGPP sysroot：i586-pc-msdosdjgpp 子目錄含 sys-include 與目標端 lib。
# DJGPP_ROOT 由 library_generation.sh 匯出（容器內 = /project/86Duino/djgpp）。
set(CMAKE_FIND_ROOT_PATH "$ENV{DJDIR}")

# 只在 sysroot 內搜尋 headers/libs，不使用 host 系統的
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# BOTH：允許在 sysroot 和 host 系統路徑（/opt/ros/$ROS_DISTRO）同時搜尋。
# ament_cmake_* / rosidl 產生器是 host 端建置工具，必須能在 host 路徑找到。
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# DJGPP 不支援 shared library，強制靜態連結
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS   "")   # 防止 CMake 自動加入 -rdynamic
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

# 停用 pthread（FreeDOS 沒有 thread 支援）
set(THREADS_HAVE_PTHREAD_ARG OFF)
set(CMAKE_THREAD_LIBS_INIT "")

# 編譯旗標
# -march=i586：86Duino Vortex86EX2 為 Pentium/i586 等級
# 不加 -DDJGPP：DJGPP 編譯器已內建定義 DJGPP 與 __DJGPP__，重複定義會產生 warning
# -ffunction-sections/-fdata-sections：配合 link 階段做 section GC，縮小成品
# -DCLOCK_MONOTONIC=0：micro-ROS 多處假設此巨集存在（參考 esp32_toolchain）
# -Dstatic_assert=_Static_assert：DJGPP C99 mode 把 static_assert 當外部函式呼叫
#   （會 linker error），映射到 GCC 內建 _Static_assert。
set(COMMON_FLAGS "-march=i586 -Os -ffunction-sections -fdata-sections -DCLOCK_MONOTONIC=0 -D'RCUTILS_LOG_MIN_SEVERITY=RCUTILS_LOG_MIN_SEVERITY_NONE'")

set(CMAKE_C_FLAGS_INIT   "-std=c11 ${COMMON_FLAGS} -Dstatic_assert=_Static_assert" CACHE STRING "" FORCE)
# DJGPP 透過 SJLJ 支援 C++ 例外，不加 -fno-exceptions 以免 try/catch 編譯錯誤；
# 同樣不加 -fno-rtti。gcc 8.3 對 jazzy 程式碼建議用 C++17。
set(CMAKE_CXX_FLAGS_INIT "-std=c++17 ${COMMON_FLAGS}" CACHE STRING "" FORCE)

# 靜態連結，並在 link 階段移除未使用的 section
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -Wl,--gc-sections" CACHE STRING "" FORCE)

set(__BIG_ENDIAN__ 0)
