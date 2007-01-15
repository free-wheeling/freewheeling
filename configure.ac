# MAKING EFFECTIVE TOOLS
# 
# The more we incorporate complexity,
# the more complex we have to become
# to manage it. 
# We become tied up in our own knots.
# We are Nature,
# we and the tool are One, and
# we and the process of making the tool are One.
#
# It's not nostalgia for the past.
# It's moving harmoniously
# as One Whole Being of Nature
# when we consider "All My Relations"
# as fibers of our own Being. 
#

# Process this file with autoconf to produce a configure script.
AC_INIT(src/fweelin.cc)
AM_INIT_AUTOMAKE(fweelin,0.5.4)
AC_CONFIG_SRCDIR([src/fweelin.cc])
#AM_CONFIG_HEADER(config.h)

AC_ARG_ENABLE(fluidsynth,[  --enable-fluidsynth     use integrated FluidSynth softsynth (libfluidsynth) (default=auto) ])

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
AC_CHECK_LIB([gnutls-openssl], [main], , [AC_MSG_ERROR(You need libgnutls11-dev installed)])
AC_CHECK_LIB([dl], [main], , [AC_MSG_ERROR(You need libc6-dev installed)])
AC_CHECK_LIB([freetype], [FT_Init_FreeType], , 
  [AC_MSG_ERROR(VIDEO: You need libfreetype6-dev installed.
		http://freetype.sourceforge.net/index2.html)])

# Check for SDL_gfx with different function names..
AC_CHECK_LIB([SDL_gfx], [main], , 
  [AC_MSG_ERROR(VIDEO: You need libsdl-gfx1.2-dev installed.
                http://www.ferzkopp.net/~aschiffler/Software/SDL_gfx-2.0/)])
AC_CHECK_FUNC(filledPieRGBA, [AC_DEFINE(CAPITAL_FILLED_PIE)])

AC_CHECK_LIB([SDL_ttf], [main], , 
  [AC_MSG_ERROR(VIDEO: You need libsdl-ttf2.0-dev installed.
		http://www.libsdl.org/projects/SDL_ttf/)])

AC_CHECK_LIB([xml2], [main], , 
  [AC_MSG_ERROR(CONFIG: You need libxml2-dev installed.
	        http://www.xmlsoft.org/)])

AC_CHECK_LIB([m], [sqrt], , [AC_MSG_ERROR(CORE: You need libm installed)])
AC_CHECK_LIB([pthread], [pthread_self], , [AC_MSG_ERROR(CORE: You need libpthread installed)])

AC_CHECK_LIB([asound], [main], , 
  [AC_MSG_ERROR(AUDIO: You need ALSA installed (libasound2-dev).
		http://www.alsa-project.org/)])

AC_CHECK_LIB([jack], [jack_client_new], , 
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

AC_CHECK_LIB([sndfile], [main], , 
  [AC_MSG_ERROR(AUDIO: you need libsndfile installed.
		http://www.http://www.mega-nerd.com/libsndfile/)])

CFLAGS="$CFLAGS $SDL_CFLAGS"
LIBS="$X_LIBS $X_PRE_LIBS -L/usr/X11R6/lib -lX11 $X_EXTRA_LIBS $LIBS"
LIBS="$LIBS $SDL_LIBS"

# Checks for header files.
AC_PATH_X
AC_HEADER_STDC
AC_CHECK_HEADERS([errno.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME

# Checks for library functions.
AC_CHECK_FUNCS([floor gettimeofday memset pow sqrt])

AC_OUTPUT(Makefile src/Makefile data/Makefile)