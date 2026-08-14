#!/bin/sh

SOCLE=$(grep "^ID=" /etc/os-release | cut -f 2 -d '=')

if [ "$(whoami)" != "root" ]; then
  echo "Only user root can run this script (or sudo)."
  exit 1
fi

groupadd -f abls 2>/dev/null || true

if [ "$SOCLE" = "fedora" ]; then
  echo "Configuring ABLS-PKGS repository"
  curl -fsSL https://pkgs.abls-habitat.fr/abls-rpms.repo -o /etc/yum.repos.d/abls-rpms.repo

  echo "Installing RPM-based dependencies"
  dnf install -y git cmake gcc pkg-config rpm-build rpm-sign
  dnf install -y glib2-devel json-glib-devel abls-libs-devel abls-agent-libs-devel
fi

if [ "$SOCLE" = "debian" ] || [ "$SOCLE" = "raspbian" ] || [ "$SOCLE" = "ubuntu" ]; then
  echo "Configuring ABLS APT repository"
  mkdir -p /etc/apt/keyrings
  curl -fsSL https://pkgs.abls-habitat.fr/rpms/keys/RPM-GPG-KEY-ABLS | gpg --dearmor --yes -o /etc/apt/keyrings/abls-archive-keyring.gpg
  curl -fsSL https://pkgs.abls-habitat.fr/abls-deb.sources -o /etc/apt/sources.list.d/abls-pkgs.sources

  apt update -y
  apt install -y git cmake gcc pkg-config fakeroot dpkg-dev debhelper lintian
  apt install -y abls-libs-dev abls-agent-libs-dev libglib2.0-dev libjson-glib-dev
fi
