fweelin_dirname=fweelin
fonts_path=fonts/truetype/ttf-bitstream-vera
fweelindir = $(datadir)/$(fweelin_dirname)
fontsdir = ${datadir}/$(fweelin_dirname)/${fonts_path}

fweelin_DATA = *.txt *.xml *.mid basic.sf2 gdb-stackdump-cmds
fonts_DATA = fonts/truetype/ttf-bitstream-vera/*


install-data-hook:
	sed -i "s/<freewheeling version=\"_FWEELIN_VERSION_\">/<freewheeling version=\"${VERSION}\">/" $(DESTDIR)$(fweelindir)/fweelin.xml

uninstall-hook:
	-cd ${fweelindir} && rmdir -p ${fonts_path}
	-rmdir $(fweelindir)
