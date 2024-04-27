#!/bin/sh
set -ev

add-apt-repository --remove "deb http://apt.postgresql.org/pub/repos/apt/ bionic-pgdg main"

apt-get update
apt-get -y install \
	cmake \
	pkg-config \
	libgmp-dev \
	nettle-dev \
	libudev-dev \
	\
	qtbase5-dev \
	qttools5-dev \
	qttools5-dev-tools
