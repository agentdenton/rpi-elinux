#!/bin/bash -eu

IMG_NAME="rpi-elinux-img"
CONTAINER_NAME="rpi-elinux"

USERNAME="rpi"
RPI_ELINUX_ROOT="/home/$USERNAME/rpi-elinux"

# remove previously created images
if [[ -n $(docker ps -a | grep "$CONTAINER_NAME") ]]; then
    docker rm "$CONTAINER_NAME" || true
    docker rmi "$IMG_NAME" || true
fi

# create the image
docker build \
    -t $IMG_NAME \
    --build-arg USERNAME=$USERNAME RPI_BOARD_NAME=$RPI_BOARD_NAME .

# create the container
docker create -it --privileged \
    --mount type=bind,source="$PWD",target=$RPI_ELINUX_ROOT \
    --name $CONTAINER_NAME $IMG_NAME
