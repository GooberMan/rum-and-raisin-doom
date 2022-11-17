# Rum and Raisin Doom

Head to the [wiki](https://github.com/GooberMan/rum-and-raisin-doom/wiki) for more information.

# This repository uses submodules

To correctly clone this repository, you will also need to initialise all submodules. As cimgui also has submodules, it needs to be recursive.

`git submodule update --init --recursive`

# Compiling on Linux?

Use the Chocolate Doom build steps, in particular:
```
./autogen.sh
make
```

Rum and Raisin Doom only supports compiling with Clang on Linux. It has also only been tested on a few Ubuntu variants, other distros are unsupported.

# Compiling on Ubuntu 20.whatever and having Python problems?

`sudo apt-get install python-is-python3`

# Compiling on MacOS?

Sorry, I don't have access to hardware so I haven't been able to work out why OpenGL fails to initialise. MacOS, as a result, is currently unsupported.

# Compiling on Windows?

First things first - you'll need to update the property sheets in the msvc folder to point to the correct locations for SDL. This property sheet has a deletion in its future, so it is only a temporary measure.

You'll also need to install [vcpkg](https://github.com/Microsoft/vcpkg/) and install the following libraries:

`zlib:x64-windows`
`libpng:x64-windows`
`minizip:x64-windows`
`curl:x64-windows`

Note that while the default MSVC compiler is supported, you're going to want to use the Clang configurations for non-debug builds. Unless you want to wait for over half an hour while MSVC tries to handle the template shenanigans used for the new flat renderer.
