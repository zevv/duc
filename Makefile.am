SUBDIRS = lib duc
man1_MANS = doc/duc.1

EXTRA_DIST = doc/duc.1 doc/duc.1.html doc/duc.md

.PHONY: test

test:
	fakeroot ./test-duc

install-exec-hook:
	-/sbin/ldconfig || :;
