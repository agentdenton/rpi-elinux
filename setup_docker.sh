#!/bin/bash -eu

CONTAINER_NAME="rpi-elinux"
CONTAINER_USERNAME="rpi"
IMG_NAME="$CONTAINER_USERNAME-img"
MNT_DIR="/home/$CONTAINER_USERNAME/$CONTAINER_USERNAME-dev"

create_container() {
    # create the image
    docker build -t $IMG_NAME \
    --build-arg CONTAINER_USERNAME=$CONTAINER_USERNAME \
    --build-arg WORKDIR_PATH=$MNT_DIR .

    # create the container
    docker create -it --privileged \
        --mount type=bind,source="$PWD",target=$MNT_DIR \
        --name $CONTAINER_NAME $IMG_NAME
}

remove_container() {
    # remove previously created images
    if [[ -n $(docker ps -a | grep "$CONTAINER_NAME") ]]; then
        # Check if container is running before stopping
        if [[ -n $(docker ps -q -f name=$CONTAINER_NAME) ]]; then
            docker stop "$CONTAINER_NAME"
        fi
        docker rm "$CONTAINER_NAME" || true
        docker rmi "$IMG_NAME" || true
    fi
}

trap_handler() {
    remove_container
}
trap "echo 'Stopping...'; trap_handler" INT ERR

# Remove the previous container before creating a new one.
remove_container

create_container
