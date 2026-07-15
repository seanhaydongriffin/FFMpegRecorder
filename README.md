# FFMpegRecorder
a lightweight FFmpeg‑based recorder.
A fully fledged FFMpeg DLL stack is in the order of 136 Mb.
This is a rebuild of FFMpeg to only include the screen capture features.  Reducing the DLL stack to a single DLL of around 2 Mb.
VP8 encoder is also replaced with the more advanced libvpx (Google’s official VP8/VP9 encoder).

## Goal
To reduce the full FFmpeg suite of:
- avcodec-63.dll
- avdevice-63.dll
- avfilter-12.dll
- avformat-63.dll
- avutil-61.dll
- swresample-7.dll
- swscale-10.dll

Do what is needed for a screen recorder of:
- avcodec
- avformat
- avutil
- swscale

## How to build

### Install MSYS2
https://www.msys2.org/
Open MSYS2 MinGW64 shell
Install required build tools:

``` pacman -S git make yasm pkg-config diffutils


Download the official FFMpeg source from https://www.ffmpeg.org/download.html
eg. **ffmpeg-8.1.2.tar.bz2**.
Extract to a directory - eg. named **ffmpeg-8.1.2**.

