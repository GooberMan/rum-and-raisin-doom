# Rum and Raisin Doom

Head to the [wiki](https://github.com/GooberMan/rum-and-raisin-doom/wiki) for more information.

# Running the official releases on Windows?

Install the [MSVC Runtime](https://aka.ms/vs/17/release/vc_redist.x64.exe) before launching. You'll also need the [UCRT](https://www.microsoft.com/en-US/download/details.aspx?id=48234) installed if you're running on Windows 7 or 8 (but to be clear, those versions of Windows are unsupported).

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

Not much has changed from Chocolate Doom. I have, however, only been testing on Debian-based distros. This should work just fine for a from-scratch install if you're on such a distro:
```
sudo apt install clang automake autoconf build-essential python3 libtool git pkg-config libsdl2-dev libsdl2-mixer-dev libsdl2-net-dev libpng-dev libminizip-dev libcurl4-openssl-dev libsamplerate0-dev
git clone https://github.com/GooberMan/rum-and-raisin-doom.git
cd rum-and-raisin-doom
git submodule update --init --recursive
./autogen.sh
make
```

Rum and Raisin Doom only supports compiling with Clang on Linux. Non-Debian based distros are currently unsupported.

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
