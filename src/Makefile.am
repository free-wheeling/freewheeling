#LIBTOOL = libtool
#
#COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
#	$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
#LTCOMPILE = $(LIBTOOL) --mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) \
#	$(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
#CCLD = $(CC)
#LINK = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) \
#	$(AM_LDFLAGS) $(LDFLAGS) -o $@
## CFLAGS = @CFLAGS@
#CXXCOMPILE = $(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
#	$(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS)
#LTCXXCOMPILE = $(LIBTOOL) --mode=compile $(CXX) $(DEFS) \
#	$(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
#	$(AM_CXXFLAGS) $(CXXFLAGS)
#CXXLD = $(CXX)
#CXXLINK = $(LIBTOOL) --mode=link $(CXXLD) $(AM_CXXFLAGS) $(CXXFLAGS) \
#	$(AM_LDFLAGS) $(LDFLAGS) -o $@
## CXXFLAGS = @CXXFLAGS@

bin_PROGRAMS = fweelin

fweelin_SOURCES = stacktrace.c fweelin.cc fweelin_datatypes.cc fweelin_rcu.cc fweelin_osc.cc fweelin_event.cc fweelin_config.cc fweelin_paramset.cc fweelin_browser.cc fweelin_audioio.cc fweelin_sdlio.cc fweelin_midiio.cc fweelin_amixer.cc fweelin_videoio.cc fweelin_videoio_displays.cc fweelin_core.cc fweelin_mem.cc fweelin_block.cc fweelin_core_dsp.cc fweelin_fluidsynth.cc

fweelindir = $(datadir)/fweelin

FWEELIN_CFLAGS = -I. -g -Wall -Wextra -Wno-write-strings -D_REENTRANT -DPTHREADS -DNDEBUG -DVERSION=\"$(VERSION)\" -DFWEELIN_DATADIR=\"$(fweelindir)\" -DADDON_DIR=\"/usr/local/lib/jack\" -I/usr/include/freetype2 -I/usr/include/libxml2 -funroll-loops -finline-functions -fomit-frame-pointer -ffast-math -fexpensive-optimizations -fstrict-aliasing -falign-loops=2 -falign-jumps=2 -falign-functions=2 -O9
FWEELIN_CXXFLAGS = -Wno-non-virtual-dtor

AM_CFLAGS = $(CFLAGS) $(FWEELIN_CFLAGS)
AM_CXXFLAGS = $(CFLAGS) $(CXXFLAGS) $(FWEELIN_CFLAGS) $(FWEELIN_CXXFLAGS)
