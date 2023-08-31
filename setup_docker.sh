#!/bin/bash -eu

IMG_NAME="rpi-elinux-img"
CONTAINER_NAME="rpi-elinux"

USERNAME="rpi"
PROJECT_DIR="/home/$USERNAME/rpi-elinux"

git submodule update --init --recursive

# remove previously created images
if [[ -n $(docker ps -a | grep "$CONTAINER_NAME") ]]; then
    docker rm "$CONTAINER_NAME" || true
    docker rmi "$IMG_NAME" || true
fi

# create the image
docker build -t $IMG_NAME --build-arg USERNAME=$USERNAME .

# create the container
docker create -it --privileged \
    --mount type=bind,source="$PWD",target=$PROJECT_DIR \
    --name $CONTAINER_NAME $IMG_NAME
