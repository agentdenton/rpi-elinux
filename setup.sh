#!/bin/bash

set -e
set -u

IMG_NAME="rpi0w_dev_img"
CONTAINER_NAME="rpi0w_dev"

USERNAME="rpi0w"
CURR_DIR="$(pwd)"

if [[ ! -d "$CURR_DIR/buildroot" ]]; then
    git clone git://git.buildroot.net/buildroot
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

docker start $CONTAINER_NAME

docker exec -u $USERNAME $CONTAINER_NAME \
    bash -c "cd buildroot && make rpi0w_defconfig"

docker exec -u $USERNAME $CONTAINER_NAME \
    bash -c "cd buildroot && make -j$(nproc)"

docker stop $CONTAINER_NAME
