Installation through RPM
========================

If you want you can install duc through a rpm package. Under `build/duc.spec` you find the spec file to build the rpm.

Tested on `CentOS 6.5`.

### Build

Using `mock` and `rpmdevtools` to create the rpm (`yum install mock rpm-build rpmdevtools` from `EPEL` ).

##### User

Add new mock user:

```sh
useradd <username>
passwd <username>
usermod -aG mock <username>
```

Or editing existing users:

```sh
usermod -aG mock <username> && newgrp mock
```

##### Get sources from repository

```sh
cd deploy/rpmbuild/
spectool -g duc.spec
```

##### Build the sources (src.rpm)

```sh
mock --resultdir=. --root epel-6-x86_64 --buildsrpm --spec duc.spec --sources .
```

##### Build rpm

```sh
mock --resultdir=. --root=epel-6-x86_64 --rebuild duc-1.0-1.el6.src.rpm
```

After this operations you have `duc-1.0-1.el6.x86_64.rpm`. To install it simply run:

```sh
yum localinstall duc-1.0-1.el6.x86_64.rpm
```
