# Rum and Raisin Doom

Head to the [wiki](https://github.com/GooberMan/rum-and-raisin-doom/wiki) for more information.

# This repository uses submodules

To correctly clone this repository, you will also need to initialise all submodules. As cimgui also has submodules, it needs to be recursive.

`git submodule update --init --recursive`

# Compiling on Ubuntu 20.whatever and having Python problems?

`sudo apt-get install python-is-python3`

# Compiling on Windows?

First things first - you'll need to update the property sheets in the msvc folder to point to the correct locations for SDL.

You'll also need to install [vcpkg](https://github.com/Microsoft/vcpkg/) and use the following libraries:

`zlib:x64-windows`
`libpng:x64-windows`
`minizip:x64-windows`
`curl:x64-windows`
