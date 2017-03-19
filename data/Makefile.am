fweelindir = $(datadir)/fweelin

fweelin_DATA = *.txt *.xml basic.sf2 vera.ttf verabd.ttf gdb-stackdump-cmds

install-data-hook:
	sed -i "s/<freewheeling version=\"_FWEELIN_VERSION_\">/<freewheeling version=\"${VERSION}\">/" $(DESTDIR)$(fweelindir)/fweelin.xml
