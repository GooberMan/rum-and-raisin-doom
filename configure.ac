AC_INIT(Rum and Raisin Doom, 0.3.1-pre,
        nothingyet@noemail.com, rum-and-raisin-doom)

PACKAGE_SHORTNAME=${PACKAGE_NAME% Doom}
PACKAGE_SHORTDESC="Fork of Chocolate Doom focusing on renderer optimisations"
PACKAGE_COPYRIGHT="Copyright (C) 1993-2022"
PACKAGE_LICENSE="GNU General Public License, version 2"
PACKAGE_MAINTAINER="Ethan Watson"
PACKAGE_URL="https://github.com/GooberMan/rum-and-raisin-doom"
PACKAGE_RDNS="org.rum_and_raisin_doom"

AC_DEFINE( PACKAGE_BANNER_BACK, TXT_COLOR_BROWN )

AC_CONFIG_AUX_DIR(autotools)
AC_CANONICAL_HOST

orig_CFLAGS="$CFLAGS"

orig_CC="$CC"
orig_CXX="$CXX"

CC="clang"
CXX="clang++"

OPT_LEVEL=3
WARNINGS="-Wall -Wdeclaration-after-statement -Wredundant-decls"
CFLAGS="-O$OPT_LEVEL -g -DNDEBUG $WARNINGS $orig_CFLAGS"
CXXFLAGS="$CFLAGS $CXXFLAGS -std=c++20 -Wc++20-extensions"

AM_PROG_AR
AC_PROG_CC( [clang])
AC_PROG_CXX( [clang++])
AC_PROG_RANLIB
AC_CHECK_PROG(HAVE_PYTHON, python, true, false)

PKG_CHECK_MODULES(SDL, [sdl2 >= 2.0.1])
PKG_CHECK_MODULES(SDLMIXER, [SDL2_mixer >= 2.0.0])
PKG_CHECK_MODULES(SDLNET, [SDL2_net >= 2.0.0])
PKG_CHECK_MODULES(PNG, libpng >= 1.2.50)
PKG_CHECK_MODULES(LIBCURL, [libcurl])
PKG_CHECK_MODULES(MINIZIP, [minizip])

# Check for bash-completion.
AC_ARG_ENABLE([bash-completion],
AS_HELP_STRING([--disable-bash-completion], [Disable bash-completion])
)
AS_IF([test "x$enable_bash_completion" != xno], [
    AS_IF([test HAVE_PYTHON = false], [
        AC_MSG_WARN([Building bash-completion requires Python, but Python not found])
        enable_bash_completion=no
    ])
])

# Check for doc.
AC_ARG_ENABLE([doc],
AS_HELP_STRING([--disable-doc], [Disable documentation])
)
AS_IF([test "x$enable_doc" != xno], [
    AS_IF([test HAVE_PYTHON = false], [
        AC_MSG_WARN([Building documentation requires Python, but Python not found])
        enable_doc=no
    ])
])

# Check for fonts.
AC_ARG_ENABLE([fonts],
AS_HELP_STRING([--disable-fonts], [Disable fonts])
)
AS_IF([test "x$enable_fonts" != xno], [
    AS_IF([test HAVE_PYTHON = false], [
        AC_MSG_WARN([Building fonts require Python, but Python not found])
        enable_fonts=no
    ])
])

# Check for icons.
AC_ARG_ENABLE([icons],
AS_HELP_STRING([--disable-icons], [Disable icons])
)
AS_IF([test "x$enable_icons" != xno], [
    AS_IF([test HAVE_PYTHON = false], [
        AC_MSG_WARN([Building icons require Python, but Python not found])
        enable_icons=no
    ])
])

# Check for libsamplerate.
AC_ARG_WITH([libsamplerate],
AS_HELP_STRING([--without-libsamplerate],
    [Build without libsamplerate @<:@default=check@:>@]),
[],
[
    [with_libsamplerate=check]
])
AS_IF([test "x$with_libsamplerate" != xno], [
    PKG_CHECK_MODULES(SAMPLERATE, samplerate >= 0.1.8, [
        AC_DEFINE([HAVE_LIBSAMPLERATE], [1], [libsamplerate installed])
    ], [
        AS_IF([test "x$with_libsamplerate" != xcheck], [AC_MSG_FAILURE(
            [--with-libsamplerate was given, but test for libsamplerate failed])
        ])
    ])
])

# TODO: We currently link everything against libraries that don't need it.
# Use the specific library CFLAGS/LIBS variables instead of setting them here.
CFLAGS="$CFLAGS $SDL_CFLAGS $PNG_CFLAGS $LIBCURL_CFLAGS $MINIZIP_CFLAGS -pthread ${SAMPLERATE_CFLAGS:-}"
LDFLAGS="$LDFLAGS $SDL_LIBS $PNG_LIBS $LIBCURL_LIBS $MINIZIP_LIBS -pthread -ldl ${SAMPLERATE_LIBS:-}"
AC_CHECK_LIB(m, log)

AC_CHECK_HEADERS([dirent.h linux/kd.h dev/isa/spkrio.h dev/speaker/speaker.h])
AC_CHECK_FUNCS(mmap ioperm)
AC_CHECK_DECLS([strcasecmp, strncasecmp], [], [], [[#include <strings.h>]])

# OpenBSD I/O i386 library for I/O port access.
# (64 bit has the same thing with a different name!)

AC_CHECK_LIB(i386, i386_iopl)
AC_CHECK_LIB(amd64, amd64_iopl)

case "$host" in
  *-*-mingw* | *-*-cygwin* | *-*-msvc* )
    AC_CHECK_TOOL(WINDRES, windres, )
    ;;
  *)
    WINDRES=
    ;;
esac

AC_CHECK_TOOL(OBJDUMP, objdump, )
AC_CHECK_TOOL(STRIP, strip, )

AC_ARG_ENABLE([werror], AS_HELP_STRING([--enable-werror], [Treat warnings as errors]))

AS_IF([test "x$enable_werror" = "xyes"], [
        CFLAGS="$CFLAGS -Werror"
])

AM_CONDITIONAL(HAVE_WINDRES, test "$WINDRES" != "")
AM_CONDITIONAL(HAVE_BASH_COMPLETION, [test "x$enable_bash_completion" != xno])
AM_CONDITIONAL(HAVE_DOC, [test "x$enable_doc" != xno])
AM_CONDITIONAL(HAVE_FONTS, [test "x$enable_fonts" != xno])
AM_CONDITIONAL(HAVE_ICONS, [test "x$enable_icons" != xno])

dnl Automake v1.8.0 is required, please upgrade!

AM_INIT_AUTOMAKE([1.8.0 foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

WINDOWS_RC_VERSION=`echo $PACKAGE_VERSION | sed 's/-.*//; s/\./, /g; s/$/, 0/'`

dnl Without a hyphen. This is used for the bash-completion scripts.
PROGRAM_SPREFIX=$(echo $PACKAGE_SHORTNAME | tr A-Z a-z | tr " " -)

dnl With a hyphen, used almost everywhere else.
PROGRAM_PREFIX=${PROGRAM_SPREFIX}-

AC_SUBST(PROGRAM_PREFIX)
AC_DEFINE_UNQUOTED(PROGRAM_PREFIX, "$PROGRAM_PREFIX",
                   Change this when you create your awesome forked version)
AC_SUBST(PROGRAM_SPREFIX)

AM_CONFIG_HEADER(config.h:config.hin)

AC_SUBST(WINDOWS_RC_VERSION)
AC_SUBST(SDLMIXER_CFLAGS)
AC_SUBST(SDLMIXER_LIBS)

AC_SUBST(SDLNET_CFLAGS)
AC_SUBST(SDLNET_LIBS)

AC_SUBST(ac_aux_dir)

AC_SUBST(PACKAGE_SHORTNAME)
AC_SUBST(PACKAGE_SHORTDESC)
AC_SUBST(PACKAGE_COPYRIGHT)
AC_SUBST(PACKAGE_LICENSE)
AC_SUBST(PACKAGE_MAINTAINER)
AC_SUBST(PACKAGE_URL)
AC_SUBST(PACKAGE_RDNS)
AC_SUBST(PACKAGE_BUGREPORT)

dnl Shut up the datarootdir warnings.
AC_DEFUN([AC_DATAROOTDIR_CHECKED])

AC_OUTPUT([
Makefile
man/Makefile
man/bash-completion/Makefile
man/bash-completion/doom.template
midiproc/Makefile
opl/Makefile
opl/examples/Makefile
pcsound/Makefile
pkg/Makefile
pkg/config.make
pkg/osx/Info.plist
rpm.spec
data/Makefile
src/Makefile
src/Doom.metainfo.xml
src/Doom.desktop
src/Doom_Screensaver.desktop
src/doom/Makefile
src/resource.rc
textscreen/Makefile
textscreen/examples/Makefile
textscreen/fonts/Makefile
])

