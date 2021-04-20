Modify from [RPiPlay](https://github.com/FD-/RPiPlay), [WinRTC decoder](https://github.com/microsoft/winrtc/tree/master/patches_for_WebRTC_org/m84/src/modules/video_coding/codecs/h264/win/decoder) and [OWT WebRTC](https://github.com/open-webrtc-toolkit/owt-deps-webrtc/tree/83-sdk/common_video)

# Contribution
* Porting RpiPlay from Raspberry Pi to Windows 10
* Integrate Media Funcation from [WinRTC](https://github.com/microsoft/winrtc/tree/master/patches_for_WebRTC_org/m84/src/modules/video_coding/codecs/h264/win) to WinAirPlay
* Modularize h264_decoder(media foundation), aac_decoder(media foundation) and fdk-aac static lib

# Introduction
An open-source implementation of an AirPlay mirroring server for Windows.
The goal is to dump video(media foundation hardware decoded .h264) and audio(fdk-aac software decoded .pcm) files in Windows.

# Prerequisite
* Install [VS 2019 community](https://visualstudio.microsoft.com/zh-hant/downloads/). Make sure Windows 10 SDK (10.0.19041.0) is checked in the installation option.
* Install [OpenSSL 1.1.1k](https://slproweb.com/download/Win64OpenSSL-1_1_1k.msi) Windows x64 version
* Install [Gstreamer 1.18.4](https://gstreamer.freedesktop.org/download/) MSVC 64-bit runtime and development installer
* Install [CMake 3.20.1](https://github.com/Kitware/CMake/releases/download/v3.20.1/cmake-3.20.1-windows-x86_64.msi)

# Building on Windows 10
* Launch x64 Native Tools Command Prompt
C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat
* mkdir build && cd build && cmake ..
* Open rpiplay.sln and build out rpiplay.exe

# Usage
* The client(ipad/iphone) should be in the same Lan network with WinAirPlay server. 
* Execute rpiplay.exe with the options bellow. For example rpiplay.exe -n RPiPlay -d -l.
* Open Client(ipad/iphone) and click airplay. The name RPiPlay will appear, click it to share screen.
* Open Youtube or other audio player in Client(ipad/iphone). If no audio played in Client. audio decoder won't be created and dump pcm file.
* H264Output.yuv and AAC-EldOutput.pcm will be generated
* Please use the [yuv player](https://sourceforge.net/projects/raw-yuvplayer/files/latest/download) and [Audacity](https://www.audacityteam.org/) for playing dump files.
* H264Output.yuv is set Color NV12 with Custom Size 688*976.
* AAC-EldOutput.pcm is imported as Raw Data with Signed 16-bit PCM, Little-endian, 2 Channels (Stereo) and Sample rate 44100 Hz.

**-n name**: Specify the network name of the AirPlay server.

**-b (on|auto|off)**: Show black background always, only during active connection, or never.

**-l**: Enables low-latency mode. Low-latency mode reduces latency by effectively rendering audio and video frames as soon as they are received, ignoring the associated timestamps. As a side effect, playback will be choppy and audio-video sync will be noticably off.

**-a (hdmi|analog|off)**: Set audio output device

**-d**: Enables debug logging. Will lead to choppy playback due to heavy console output.

**-v/-h**: Displays short help and version information.

# State
Screen mirroring and audio works for iOS 9 or newer. Recent macOS versions also seem to be compatible. The GPU is used for decoding the h264 video stream. Media foundation can only decode AAC-LC but not AAC-LD/AAC-ELD (AirPlay mirroring uses AAC-ELD), so the FDK-AAC decoder is used for AAC_ELD software decoding.

By using OpenSSL for AES decryption, it can speed up the decryption of video packets from up to 0.2 seconds to up to 0.007 seconds for large packets (On the Pi Zero). Average is now more like 0.002 seconds. I've not verified the performance in Windows.

There still are some minor issues. Have a look at the TODO list below.

[inofficial AirPlay specification](https://nto.github.io/AirPlay.html#screenmirroring).

# Todo
* Actually media foundation can't decode AAC-ELD, that's why I use fdk-aac instead. 
I hope to find out the other way to hardware decode AAC-ELD and license free.
If you know, please create a issue to let me know, thanks!
