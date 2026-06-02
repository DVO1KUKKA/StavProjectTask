#!/usr/bin/env bash

BUILD_DIR="package_build"

# Client Package
mkdir -p ${BUILD_DIR}/myrpc-cli/usr/bin
mkdir -p ${BUILD_DIR}/myrpc-cli/DEBIAN
cp myRPC-client ${BUILD_DIR}/myrpc-cli/usr/bin/

echo -e "Package: myrpc-client\nVersion: 1.0\nArchitecture: amd64\nMaintainer: Student\nDescription: RPC Client for AstraLinux" > ${BUILD_DIR}/myrpc-cli/DEBIAN/control

dpkg-deb --build ${BUILD_DIR}/myrpc-cli

# Server Package
mkdir -p ${BUILD_DIR}/myrpc-srv/usr/bin
mkdir -p ${BUILD_DIR}/myrpc-srv/etc/myRPC
mkdir -p ${BUILD_DIR}/myrpc-srv/DEBIAN

cp myRPC-server ${BUILD_DIR}/myrpc-srv/usr/bin/
echo -e "port = 1234\nsocket_type = stream" > ${BUILD_DIR}/myrpc-srv/etc/myRPC/myRPC.conf
echo -e "root\n$USER" > ${BUILD_DIR}/myrpc-srv/etc/myRPC/users.conf

echo -e "Package: myrpc-server\nVersion: 1.0\nArchitecture: amd64\nMaintainer: Student\nDescription: RPC Server Daemon" > ${BUILD_DIR}/myrpc-srv/DEBIAN/control

dpkg-deb --build ${BUILD_DIR}/myrpc-srv