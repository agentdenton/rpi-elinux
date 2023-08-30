#!/bin/bash

set -eu

IMG_NAME="rpi-elinux"
CONTAINER_NAME="rpi-elinux-dev"

USERNAME="rpi"
CURR_DIR="$PWD"

if [[ ! -d "$CURR_DIR/br/buildroot" ]]; then
    git submodule add git://git.buildroot.net/buildroot br/buildroot
    git submodule update --init
fi

if [[ ! -d "$CURR_DIR/yocto/sources/poky" ]]; then
    git submodule add https://github.com/yoctoproject/poky.git yocto/sources/poky
    git submodule update --init
fi

# remove previously created images
if [[ -n $(docker ps -a | grep "$CONTAINER_NAME") ]]; then
    docker rm "$CONTAINER_NAME" || true
    docker rmi "$IMG_NAME" || true
fi

# create the image
docker build -t $IMG_NAME --build-arg USERNAME=$USERNAME .

# create the container
docker create -it --privileged \
    --mount type=bind,source="$CURR_DIR",target=/home/$USERNAME \
    --name $CONTAINER_NAME $IMG_NAME
