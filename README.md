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

### Launch MSYS2 from x64 Native Tools Command Prompt for VS 2022
Open x64 Native Tools Command Prompt for VS 2022
Launch MSYS2:

`C:\msys64\usr\bin\bash.exe -l`

Change directory to the ffmpeg source:

`cd /c/dwn/ffmpeg-8.1.2`

### Set the path to SDK tools
In x64 Native Tools Command Prompt for VS 2022 run:

`where cl`

Take that <path> and execute the following:

`export PATH="$PATH:/c/<path>"`

Confirm cl is found:

`which cl`

Repeat for link.exe and rc.exe

MSVC also depends on the Windows SDK:

`export PATH="$PATH:/c/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"`

If cl.exe, rc.exe or link.exe cannot be found you may have to install VS2019/2022 build tools.

### Install NASM in MSYS2
Inside MSYS2 MinGW64, run:

`pacman -S nasm`

This installs NASM into:

`/mingw64/bin/nasm.exe`

Then test:

`which nasm`

### Build libvpx - MSVC cannot link MinGW libraries 
(this may be optional depending on how libvpx is built)

build libvpx manually:

```
git clone https://chromium.googlesource.com/webm/libvpx
cd libvpx
```

Then configure for MSVC:

`./configure --target=x86_64-win64-vs15 --enable-static --disable-shared`

`make`

This produces:

```
vpx.lib
vpx_encoder.h
vpx_decoder.h
```

You then:
- put vpx.lib somewhere FFmpeg can find it
- set PKG_CONFIG_PATH to point to your MSVC‑built vpx.pc

Open the generated solution in Visual Studio:
Navigate to libvpx and open:

`vpx.sln`

in Visual Studio 2022 Professional.
Build the library in Release x64.
Inside Visual Studio:
- Select Release
- Select x64
- Build ALL projects (or just the main vpx project)

This produces:

```
vpx.lib
vpx.pdb
```

The output will be in:

`libvpx\build\msvc\vs15\x64\Release\`

Copy Library:

`vpx.lib`

And Headers:

```
vpx/*.h
vpx/vp8/*.h
vpx/vp9/*.h
vpx_dsp/*.h
vpx_scale/*.h
```

To somewhere FFmpeg can find them, e.g.:

```
C:\dwn\libvpx-msvc\lib\vpx.lib
C:\dwn\libvpx-msvc\include\vpx\*.h
```

Create a pkg-config file for MSVC libvpx:
FFmpeg uses pkg-config to detect external libraries.
Create:

`C:\dwn\libvpx-msvc\lib\pkgconfig\vpx.pc`

Contents:

```
prefix=C:/dwn/libvpx-msvc
exec_prefix=${prefix}
libdir=${prefix}/lib
includedir=${prefix}/include

Name: vpx
Description: VP8/VP9 codec library
Version: 1.16.0
Libs: -L${libdir} -lvpx
Cflags: -I${includedir}
```

### Install libvpx in MSYS2 (
Inside MSYS2 MinGW64:

`pacman -S mingw-w64-x86_64-libvpx`

This installs:

```
/mingw64/lib/libvpx.lib
/mingw64/include/vpx/vpx_encoder.h
/mingw64/lib/pkgconfig/vpx.pc
```

### Point MSYS2 to libvpx

Add MinGW64 pkgconfig path:

`export PKG_CONFIG_PATH=/mingw64/lib/pkgconfig`

or

`export PKG_CONFIG_PATH=/c/.../libvpx-msvc/lib/pkgconfig`

Depending on which libvpx you have (see above sections).

Then test:

```
pkg-config --libs vpx
pkg-config --cflags vpx
```

If both commands output paths → libvpx is installed correctly.

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


