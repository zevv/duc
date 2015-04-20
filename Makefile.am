SUBDIRS = lib duc

.PHONY: test

test:
	fakeroot ./test-duc

install-exec-hook:
	-/sbin/ldconfig || :;
