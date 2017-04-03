fweelindir = $(datadir)/fweelin
fontsdir = $(datadir)/fweelin/fonts/truetype/ttf-bitstream-vera

fweelin_DATA = *.txt *.xml *.mid basic.sf2 gdb-stackdump-cmds
fonts_DATA = fonts/truetype/ttf-bitstream-vera/*


install-data-hook:
	sed -i "s/<freewheeling version=\"_FWEELIN_VERSION_\">/<freewheeling version=\"${VERSION}\">/" $(DESTDIR)$(fweelindir)/fweelin.xml
