#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([fweelin],[0.6.6],[https://github.com/free-wheeling/freewheeling/issues])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([src/fweelin.cc])
#AM_CONFIG_HEADER(config.h)

AC_ARG_ENABLE(fluidsynth,[  --enable-fluidsynth     use integrated FluidSynth softsynth (libfluidsynth) (default=auto) ])
AC_ARG_ENABLE(video,[  --disable-video         disable video output ])
AC_ARG_ENABLE(lcd,[  --enable-lcd         enable USB LCD display output ])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Add Xtra Paths for Checking
AC_PATH_XTRA
#if test x"$no_x" = xyes; then
#      AC_ERROR([Can not find X11 development headers or libraries.])
#fi

# Checks for libraries.

#AC_CHECK_LIB([X11], [XOpenDisplay], , [AC_MSG_ERROR(VIDEO: You need libx11-dev installed)])
AC_CHECK_LIB([nettle], [main], , [AC_MSG_ERROR(You need nettle-dev installed)])
AC_CHECK_LIB([dl], [main], , [AC_MSG_ERROR(You need libc6-dev installed)])
AC_CHECK_LIB([freetype], [FT_Init_FreeType], , 
  [AC_MSG_ERROR(VIDEO: You need libfreetype6-dev installed.
		http://freetype.sourceforge.net/index2.html)])

# Check for SDL_gfx with different function names..
AC_CHECK_LIB([SDL_gfx], [main], , 
  [AC_MSG_ERROR(VIDEO: You need libsdl-gfx1.2-dev installed.
                http://www.ferzkopp.net/wordpress/2016/01/02/sdl_gfx-sdl2_gfx/)])
AC_CHECK_FUNC(filledPieRGBA, [AC_DEFINE(CAPITAL_FILLED_PIE)])
AC_CHECK_LIB([SDL_ttf], [main], , 
  [AC_MSG_ERROR(VIDEO: You need libsdl-ttf2.0-dev installed.
		http://www.libsdl.org/projects/SDL_ttf/release-1.2.html)])
PKG_CHECK_MODULES([SDL], [sdl >= 1.2 sdl < 1.3], ,
  [AC_MSG_ERROR(VIDEO: You need libsdl1.2-dev installed. http://www.libsdl.org/)])

AC_CHECK_LIB([lo], [main], , [AC_MSG_ERROR(CONFIG: You need liblo-dev installed.
             http://http://liblo.sourceforge.net/)])

AC_CHECK_LIB([xml2], [main], , 
  [AC_MSG_ERROR(CONFIG: You need libxml2-dev installed.
	        http://www.xmlsoft.org/)])

AC_CHECK_LIB([m], [sqrt], , [AC_MSG_ERROR(CORE: You need libm installed)])
AC_CHECK_LIB([pthread], [pthread_self], , [AC_MSG_ERROR(CORE: You need libpthread installed)])

AC_CHECK_LIB([asound], [main], , 
  [AC_MSG_ERROR(AUDIO: You need ALSA installed (libasound2-dev).
		http://www.alsa-project.org/)])

AC_CHECK_LIB([jack], [jack_client_open], , 
  [AC_MSG_ERROR(AUDIO: You need JACK audio library (libjack-dev) installed.
		http://jackit.sourceforge.net/)])

if test "$enable_fluidsynth" != "no"; then
   AC_CHECK_LIB([fluidsynth], [main], fluidsynth=yes, fluidsynth=no)
   if test "$fluidsynth" = "yes"; then
      LIBS="$LIBS -lfluidsynth" 
      AC_DEFINE(USE_FLUIDSYNTH, 1,
        [Define if FluidSynth support should be enabled])
      echo "--- Enabling integrated FluidSynth synth ---";
   else
      echo "--- No libfluidsynth -- FluidSynth support will not be built! ---";
   fi
else
   echo "--- Disabling integrated FluidSynth synth ---";
fi

if test "$enable_video" == "no"; then
   AC_DEFINE(NO_VIDEO, 1, [Define if video output is disabled])
   echo "--- Disabling video output --";
fi

if test "$enable_lcd" == "yes"; then
   AC_DEFINE(LCD_DISPLAY, 1, [Define if USB LCD display is enabled])
   echo "--- Enabling USB LCD display --";
fi

#SDL_VERSION=1.2.4
#AM_PATH_SDL($SDL_VERSION,
#            :,
#	    AC_MSG_ERROR([VIDEO: *** SDL version $SDL_VERSION not found!
#		          http://www.libsdl.org/])
#)

AC_CHECK_LIB([vorbis], [main], , 
  [AC_MSG_ERROR(AUDIO: You need libvorbis-dev installed.
		http://www.xiph.org/ogg/vorbis/)])
AC_CHECK_LIB([vorbisfile], [main], , 
  [AC_MSG_ERROR(AUDIO: You need libvorbisfile installed.
		http://www.xiph.org/ogg/vorbis/)])
AC_CHECK_LIB([vorbisenc], [main], , 
  [AC_MSG_ERROR(AUDIO: You need libvorbis-dev installed.
		http://www.xiph.org/ogg/vorbis/)])
AC_CHECK_LIB([ogg], [main], , 
  [AC_MSG_ERROR(AUDIO: You need libogg-dev installed.
		http://www.xiph.org/ogg/vorbis/)])

AC_CHECK_LIB([sndfile], [main], , 
  [AC_MSG_ERROR(AUDIO: you need libsndfile-dev installed.
		http://www.http://www.mega-nerd.com/libsndfile/)])

CFLAGS="$CFLAGS $SDL_CFLAGS"
LIBS="$X_LIBS $X_PRE_LIBS -L/usr/X11R6/lib -lX11 $X_EXTRA_LIBS $LIBS"
LIBS="$LIBS $SDL_LIBS"

# Checks for header files.
AC_PATH_X
AC_HEADER_STDC
AC_CHECK_HEADERS([errno.h fcntl.h inttypes.h limits.h stdint.h stdlib.h string.h sys/resource.h sys/time.h termios.h unistd.h utime.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([atexit dup2 floor gettimeofday memset pow realpath select sqrt strcasecmp strchr strncasecmp strrchr strtol])

AC_OUTPUT(Makefile src/Makefile data/Makefile)
