#!/bin/bash

((0)) && {
	echo "start"
}

:<<!
#http://linuxbrew.sh/
sh -c "$(curl -fsSL https://raw.githubusercontent.com/Linuxbrew/install/master/install.sh)"
test -d ~/.linuxbrew && PATH="$HOME/.linuxbrew/bin:$HOME/.linuxbrew/sbin:$PATH"
test -d /home/linuxbrew/.linuxbrew && PATH="/home/linuxbrew/.linuxbrew/bin:/home/linuxbrew/.linuxbrew/sbin:$PATH"
test -r ~/.bash_profile && echo "export PATH='$(brew --prefix)/bin:$(brew --prefix)/sbin'":'"$PATH"' >>~/.bash_profile
echo "export PATH='$(brew --prefix)/bin:$(brew --prefix)/sbin'":'"$PATH"' >>~/.profile
!

function yum_check_pkg_installed()
{
	echo "checking "$1" ..."
	local count=`yum list installed | grep $1 | wc -l`
	if [ $count -ge 1 ]; then
		echo "ok!"
		return 1;
	else
		echo "no!"
		return 0;
	fi
}
function yum_check_and_install()
{
	yum_check_pkg_installed $1
	if [ $? -eq 0 ]; then
		sudo yum install $1
	fi
}

function deb_check_pkg_installed()
{
	echo "checking "$1" ..."
	local count=`dpkg -l | grep $1 | wc -l`
	if [ $count -ge 1 ]; then
		echo "ok!"
		return 1;
	else
		echo "no!"
		return 0;
	fi
}
function deb_check_and_install()
{
	deb_check_pkg_installed $1
	if [ $? -eq 0 ]; then
		sudo apt-get install $1
	fi
}

function pip_check_pkg_installed()
{
	echo "checking "$1" ..."
	local count=`pip list | grep $1 | wc -l`
	if [ $count -ge 1 ]; then
		echo "ok!"
		return 1;
	else
		echo "no!"
		return 0;
	fi
}
function pip_check_and_install()
{
	pip_check_pkg_installed $1
	if [ $? -eq 0 ]; then
		sudo pip install $1
	fi
}

function rpm_develop_depend()
{
	yum_check_and_install "ruby"
	yum_check_and_install "rubygem-json"


	yum_check_and_install "python3-pip"
	pip_check_and_install "simplejson"


	yum_check_and_install "snappy-devel"
	yum_check_and_install "lz4-devel"
	yum_check_and_install "bzip2-devel"
}

function deb_develop_depend()
{
	deb_check_and_install "ruby"

	deb_check_and_install "python-pip"
	pip_check_and_install "simplejson"

	deb_check_and_install "openssl"
	deb_check_and_install "libssl-dev"
	deb_check_and_install "libsnappy-dev"
	deb_check_and_install "liblz4-dev"
	deb_check_and_install "libbz2-dev"
}

OS_DID=`lsb_release -a | grep "Distributor ID" | cut -d ":" -f 2 | awk '$1=$1'`
case $OS_DID in
        "Fedora")
		rpm_develop_depend
                ;;
        "CentOS")
		rpm_develop_depend
                ;;
        "Debian")
		deb_develop_depend
                ;;
        "Ubuntu")
		deb_develop_depend
                ;;
        *)
                ;;
esac


make libuv
make libev
make libevcoro
make libcomm
make libh2o
make librocksdb
make
