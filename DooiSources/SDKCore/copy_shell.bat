@echo off
setlocal enabledelayedexpansion

REM 源目录：当前脚本所在目录
set "SRC=%~dp0"

REM 路径映射（源文件夹 -> 目标相对路径）
REM 注意：目标路径都是从 SDKCore 返回三级目录到 .sdk
set "MAP_csk6_dooi_robot_v1=..\..\..\..\.sdk\csk\boards\arm"
set "MAP_display=..\..\..\..\.sdk\csk\drivers"
set "MAP_pwm=..\..\..\..\.sdk\csk\drivers"
set "MAP_sessions=..\..\..\..\.sdk\modules\lschat\src"
set "MAP_src=..\..\..\..\.sdk\modules\hal\listenai\csk6_cm33"
set "MAP_zephyr=..\..\..\..\.sdk\modules\lib\gui\lvgl"
set "MAP_display_zephyr=..\..\..\..\.sdk\zephyr\dts\bindings\display"
set "MAP_input=..\..\..\..\.sdk\zephyr\dts\bindings"

for %%D in (csk6_dooi_robot_v1 display pwm sessions src zephyr display_zephyr input) do (
    set "SRC_DIR=%SRC%%%D"
    set "DEST_DIR=%SRC%!MAP_%%D!"

    if not exist "!DEST_DIR!" (
        mkdir "!DEST_DIR!"
    )

    if "%%D"=="display_zephyr" (
        REM 特殊：只拷贝文件，不新建子目录
        for %%F in (!SRC_DIR!\*) do (
            if exist "!DEST_DIR!\%%~nxF" (
                REM 如果目标文件已存在并且不同，则拷贝
                copy /Y "%%F" "!DEST_DIR!\%%~nxF"
            ) else (
                REM 如果目标文件不存在，则拷贝
                copy "%%F" "!DEST_DIR!\%%~nxF"
            )
        )
    ) else (
        set "FINAL_DEST=!DEST_DIR!\%%D"
        if not exist "!FINAL_DEST!" (
            mkdir "!FINAL_DEST!"
        )
        for %%F in (!SRC_DIR!\*) do (
            if exist "!FINAL_DEST!\%%~nxF" (
                REM 如果目标文件已存在并且不同，则拷贝
                copy /Y "%%F" "!FINAL_DEST!\%%~nxF"
            ) else (
                REM 如果目标文件不存在，则拷贝
                copy "%%F" "!FINAL_DEST!\%%~nxF"
            )
        )
    )

    echo process %%D to !DEST_DIR!
)

echo success

pause
