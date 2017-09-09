extras.tar: dirhelper com.evad3rs.evasi0n.list com.evad3rs.evasi0n.prerm com.evad3rs.evasi0n.postrm com.evad3rs.evasi0n.extrainst_
	$(eval TEMPDIR := $(shell mktemp -d -t evasi0n-extras))
	mkdir -p $(TEMPDIR)/usr/libexec
	mkdir -p $(TEMPDIR)/private/var/lib/dpkg/info
	cp dirhelper $(TEMPDIR)/usr/libexec/dirhelper
	cp com.evad3rs.evasi0n.list $(TEMPDIR)/private/var/lib/dpkg/info/com.evad3rs.evasi0n.list
	cp com.evad3rs.evasi0n.prerm $(TEMPDIR)/private/var/lib/dpkg/info/com.evad3rs.evasi0n.prerm
	cp com.evad3rs.evasi0n.postrm $(TEMPDIR)/private/var/lib/dpkg/info/com.evad3rs.evasi0n.postrm
	cp com.evad3rs.evasi0n.extrainst_ $(TEMPDIR)/private/var/lib/dpkg/info/com.evad3rs.evasi0n.extrainst_
	sudo chmod a+rx $(TEMPDIR)
	sudo chown -R 0:0 $(TEMPDIR)
	cd $(TEMPDIR); COPYFILE_DISABLE=1 COPY_EXTENDED_ATTRIBUTES_DISABLE=1 tar cvf $(CURDIR)/$@ .
	sudo rm -rf $(TEMPDIR)

clean:
	rm -f extras.tar
