SUBDIRS = lib duc
man1_MANS = doc/duc.1

.PHONY: test

test:
	fakeroot ./test-duc

install-exec-hook:
	-/sbin/ldconfig || :;
