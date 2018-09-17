#!/bin/bash
shopt -s expand_aliases
set -x

# This file is intended to gather everything in or used in synApps.
# The version numbers in this file are not guaranteed to be up to date,
# and the modules are not guaranteed to work or even build together.

shallow_repo()
{
	PROJECT=$1
	MODULE_NAME=$2
	RELEASE_NAME=$3
	TAG=$4
	
	FOLDER_NAME=$MODULE_NAME-${TAG//./-}
	
	echo
	echo "Grabbing $MODULE_NAME at tag: $TAG"
	echo
	
	if [ ! -d "$FOLDER_NAME" ] 
	then
		git clone --branch $TAG --depth 1 git://github.com/$PROJECT/$MODULE_NAME.git $FOLDER_NAME
	fi
	
	echo "$RELEASE_NAME=\$(SUPPORT)/$FOLDER_NAME" >> ./configure/RELEASE
	
	echo
}

shallow_support()
{
	if [ ! -d "$1" ]
	then
		git clone --branch $2 --depth 1 git://github.com/EPICS-synApps/$1.git
	fi
}

cd $HOME/.cache

EPICS_BASE=$HOME/.cache/base-$BASE

if [ ! -d "$EPICS_BASE" ] 
then
	git clone --branch $BASE --depth 1 git://github.com/epics-base/epics-base.git base-$BASE

	EPICS_HOST_ARCH=`sh $EPICS_BASE/startup/EpicsHostArch`
	
	case "$STATIC" in
	static)
		cat << EOF >> "$EPICS_BASE/configure/CONFIG_SITE"
SHARED_LIBRARIES=NO
STATIC_BUILD=YES
EOF
		;;
	*) ;;
	esac
	
	case "$CMPLR" in
	clang)
		echo "Host compiler is clang"
		
		cat << EOF >> "$EPICS_BASE/configure/os/CONFIG_SITE.Common.$EPICS_HOST_ARCH"
GNU         = NO
CMPLR_CLASS = clang
CC          = clang
CCC         = clang++
EOF
		;;
	*) echo "Host compiler is default";;
	esac
	
	# requires wine and g++-mingw-w64-i686
	if [ "$WINE" = "32" ]
	then
		echo "Cross mingw32"
		sed -i -e '/CMPLR_PREFIX/d' $EPICS_BASE/configure/os/CONFIG_SITE.linux-x86.win32-x86-mingw
		cat << EOF >> "$EPICS_BASE/configure/os/CONFIG_SITE.linux-x86.win32-x86-mingw"
CMPLR_PREFIX=i686-w64-mingw32-
EOF
		cat << EOF >> "$EPICS_BASE/configure/CONFIG_SITE"
CROSS_COMPILER_TARGET_ARCHS+=win32-x86-mingw
EOF
	fi
	
	# set RTEMS to eg. "4.9" or "4.10"
	if [ -n "$RTEMS" ]
	then
		echo "Cross RTEMS${RTEMS} for pc386"
		install -d /home/travis/.cache
		curl -L "https://github.com/mdavidsaver/rsb/releases/download/travis-20160306-2/rtems${RTEMS}-i386-trusty-20190306-2.tar.gz" \
		| tar -C /home/travis/.cache -xj
	
		sed -i -e '/^RTEMS_VERSION/d' -e '/^RTEMS_BASE/d' $EPICS_BASE/configure/os/CONFIG_SITE.Common.RTEMS
		cat << EOF >> "$EPICS_BASE/configure/os/CONFIG_SITE.Common.RTEMS"
RTEMS_VERSION=$RTEMS
RTEMS_BASE=/home/travis/.cache/rtems${RTEMS}-i386
EOF
		cat << EOF >> $EPICS_BASE/configure/CONFIG_SITE
CROSS_COMPILER_TARGET_ARCHS+=RTEMS-pc386
EOF
	
	fi
	
	make -C "$EPICS_BASE" -j2

	
	# get MSI for 3.14
	case "$BASE" in
	3.14*)
		if [ ! -d "$HOME/msi/extensions/src" ]
		then
			echo "Build MSI"
			install -d "$HOME/msi/extensions/src"
			curl https://epics.anl.gov/download/extensions/extensionsTop_20120904.tar.gz | tar -C "$HOME/msi" -xvz
			curl https://epics.anl.gov/download/extensions/msi1-7.tar.gz | tar -C "$HOME/msi/extensions/src" -xvz
			mv "$HOME/msi/extensions/src/msi1-7" "$HOME/msi/extensions/src/msi"
		
			cat << EOF > "$HOME/msi/extensions/configure/RELEASE"
EPICS_BASE=$EPICS_BASE
EPICS_EXTENSIONS=\$(TOP)
EOF
	
		fi
		
		make -C "$HOME/msi/extensions"
		cp "$HOME/msi/extensions/bin/$EPICS_HOST_ARCH/msi" "$EPICS_BASE/bin/$EPICS_HOST_ARCH/"
		echo 'MSI:=$(EPICS_BASE)/bin/$(EPICS_HOST_ARCH)/msi' >> "$EPICS_BASE/configure/CONFIG_SITE"
	
		cat <<EOF >> configure/CONFIG_SITE
MSI = \$(EPICS_BASE)/bin/\$(EPICS_HOST_ARCH)/msi
EOF
	
		;;
	*) echo "Use MSI from Base"
		;;
	esac
fi

alias get_support='shallow_support'
alias get_repo='shallow_repo'

get_support support synApps_5_8
cd support

get_support configure        synApps_5_8

echo "SUPPORT=$HOME/.cache/support" > configure/RELEASE
echo "EPICS_BASE=$EPICS_BASE" >> configure/RELEASE

echo "EPICS_BASE=$EPICS_BASE" > $TRAVIS_BUILD_DIR/configure/RELEASE 
