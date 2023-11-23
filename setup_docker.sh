#!/bin/bash -eu

USERNAME="rpi"
CONTAINER_NAME="rpi-elinux"
CONTAINER_IMG_NAME="rpi-elinux-img"
MNT_DIR="/home/$USERNAME/rpi-dev"

create_container() {
    # create the image
    docker build -t $CONTAINER_IMG_NAME \
    --build-arg USERNAME=$USERNAME \
    --build-arg WORKDIR=$MNT_DIR .

    # create the container
    docker create -it --privileged \
        --mount type=bind,source="$PWD",target=$MNT_DIR \
        --name $CONTAINER_NAME $CONTAINER_IMG_NAME
}

remove_container() {
    # remove previously created images
    if [[ -n $(docker ps -a | grep "$CONTAINER_NAME") ]]; then
        # Check if container is running before stopping
        if [[ -n $(docker ps -q -f name=$CONTAINER_NAME) ]]; then
            docker stop "$CONTAINER_NAME"
        fi
        docker rm "$CONTAINER_NAME" || true
        docker rmi "$CONTAINER_IMG_NAME" || true
    fi
}

# NOTE: Can't copy keys inside the Dockerfile from $HOME, so do it here
cp_ssh_keys() {
    docker start $CONTAINER_NAME

    docker cp $HOME/.ssh/github $CONTAINER_NAME:/home/$USERNAME/.ssh
    docker cp $HOME/.ssh/github.pub $CONTAINER_NAME:/home/$USERNAME/.ssh
    docker cp $HOME/.ssh/known_hosts $CONTAINER_NAME:/home/$USERNAME/.ssh

    docker stop $CONTAINER_NAME
}

trap_handler() {
    remove_container
}
trap "echo 'Stopping...'; trap_handler" INT ERR

# Remove the previous container before creating a new one.
remove_container

create_container

# Copy github host ssh keys to the container
cp_ssh_keys
