#!/bin/sh

SOCLE=$(grep "^ID=" /etc/os-release | cut -f 2 -d '=')

if [ "$(whoami)" != "root" ]; then
  echo "Only user root can run this script (or sudo)."
  exit 1
fi

groupadd -f abls 2>/dev/null || true

if [ "$SOCLE" = "fedora" ]; then
  echo "Installing RPM-based dependencies"
  dnf install -y git cmake gcc pkg-config rpm-build rpm-sign
  dnf install -y glib2-devel json-glib-devel abls-libs-devel abls-agent-libs-devel
fi

if [ "$SOCLE" = "debian" ] || [ "$SOCLE" = "raspbian" ] || [ "$SOCLE" = "ubuntu" ]; then
  apt update -y
  apt install -y git cmake gcc pkg-config fakeroot dpkg-dev debhelper lintian
  apt install -y abls-libs-dev abls-agent-libs-dev libglib2.0-dev libjson-glib-dev
fi
