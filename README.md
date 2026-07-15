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

### Download FFmpeg source
Download the official FFMpeg source from https://www.ffmpeg.org/download.html
eg. **ffmpeg-8.1.2.tar.bz2**.
Extract to a directory - eg. named **ffmpeg-8.1.2**.

### Install MSYS2
https://www.msys2.org/
Open MSYS2 MinGW64 shell
Install required build tools:
`pacman -S git make yasm pkg-config diffutils`
Add MSVC compiler to PATH inside MSYS2:
```
export CC="cl"
export CXX="cl"
```

### Configure FFmpeg build
Run the configure command:
```
./configure \
  --toolchain=msvc \
  --enable-static \
  --disable-shared \
  --disable-programs \
  --disable-doc \
  --disable-avdevice \
  --disable-swresample \
  --disable-postproc \
  --disable-avfilter \
  --disable-network \
  --disable-everything \
  --enable-decoder=mjpeg \
  --enable-parser=mjpeg \
  --enable-encoder=libvpx_vp8 \
  --enable-muxer=webm \
  --enable-protocol=file \
  --enable-swscale \
  --enable-avcodec \
  --enable-avformat \
  --enable-avutil
```

### Build FFmpeg
`make`
The static libs appear in:
```
./libavcodec/avcodec.lib
./libavformat/avformat.lib
./libavutil/avutil.lib
./libswscale/swscale.lib
```

### link the libs into FFMpegRecorder.dll


