# Rum and Raisin Doom

Head to the [wiki](https://github.com/GooberMan/rum-and-raisin-doom/wiki) for more information.

# Installing on Raspberry Pi?

Ubuntu has made it needlessly hard to install packages from the internet. Not only do they open in the default compressed archive program, but the installer doesn't install dependencies. Instead you should use the previous package manager program, gdebi:

```
sudo apt install gdebi
```

Note that it also installs the graphical `gdebi-gtk` program if you'd rather use that once it's installed.

# This repository uses submodules

To correctly clone this repository, you will also need to initialise all submodules. As cimgui also has submodules, it needs to be recursive.

```
git submodule update --init --recursive
```

# Compiling on Linux?

Use the Chocolate Doom build steps, in particular:
```
./autogen.sh
make
```

Rum and Raisin Doom only supports compiling with Clang on Linux. It has also only been tested on a few Ubuntu variants, other distros are unsupported.

# Compiling on Ubuntu 20.x and later? Having Python problems?

```
sudo apt-get install python-is-python3
```

# Compiling on MacOS?

Sorry, I don't have access to hardware so I haven't been able to work out why OpenGL fails to initialise. MacOS, as a result, is currently unsupported.

# Compiling on Windows?

[vcpkg](https://github.com/Microsoft/vcpkg/) is in active use, and requires you to install the following libraries:

`zlib:x64-windows`
`libpng:x64-windows`
`minizip:x64-windows`
`curl:x64-windows`
`libsamplerate:x64-windows`
`sdl2:x64-windows`
`sdl2-mixer:x64-windows`
`sdl2-net:x64-windows`

Note that while the default MSVC compiler is supported, you're going to want to use the Clang configurations for non-debug builds. Unless you want to wait for over half an hour while MSVC tries to handle the template shenanigans used for the new flat renderer.
