AUX_DIST_GEN =                          \
        $(ac_aux_dir)/install-sh        \
        $(ac_aux_dir)/missing

CMAKE_FILES=                                   \
        CMakeLists.txt                         \
        cmake/FindSDL2.cmake                   \
        cmake/FindSDL2_mixer.cmake             \
        cmake/FindSDL2_net.cmake               \
        cmake/Findm.cmake                      \
        cmake/Findsamplerate.cmake             \
        cmake/config.h.cin

DOC_FILES=                              \
        COPYING.md                      \
        README.md                       \
        README.Music.md                 \
        NEWS.md                         \
        PHILOSOPHY.md                   \
        ChangeLog

EXTRA_DIST=                             \
        $(AUX_DIST_GEN)                 \
        $(CMAKE_FILES)                  \
        $(DOC_FILES)                    \
        NOT-BUGS.md                     \
        README.Strife.md                \
        .lvimrc                         \
        HACKING.md                      \
        TODO.md                         \
        rpm.spec                        \
        win32/win_opendir.c             \
        win32/win_opendir.h

doomdocsdir = ${docdir}/../${PROGRAM_PREFIX}doom
doomdocs_DATA = $(DOC_FILES) NOT-BUGS.md

MAINTAINERCLEANFILES =  $(AUX_DIST_GEN)

SUBDIRS=textscreen midiproc opl pcsound data src

DIST_SUBDIRS=pkg $(SUBDIRS)

if HAVE_DOC

INSTALL : man/INSTALL.template man/simplecpp
	./man/simplecpp -DDOOM \
            -DLONG_GAME_NAME="@PACKAGE_SHORTNAME@ Doom" \
            -DLONG_EXE_NAME="@PROGRAM_PREFIX@doom" \
	                < man/INSTALL.template > $@

endif
