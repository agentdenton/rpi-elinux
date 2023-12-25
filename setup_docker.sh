#!/bin/bash -eu

USERNAME="rpi"

CONTAINER_NAME="rpi-elinux"
CONTAINER_IMG_NAME="rpi-elinux-img"
CONTAINER_NET_NAME="rpi-elinux-macvlan"

SUBNET="192.168.50.0/24"
GATEWAY="192.168.50.1"
INTERFACE="enp4s0f4u1u1"
HOST_MACVLAN="rpi-macvlan"

CONTAINER_IP_ADDR="192.168.50.102"
HOST_MACVLAN_IP_ADDR="192.168.50.103"

MNT_DIR="/home/$USERNAME/rpi-dev"

remove_container() {
    # remove previously created images
    if docker ps -a | grep $CONTAINER_NAME; then
        # Check if container is running before stopping
        if docker ps -q -f name=$CONTAINER_NAME; then
            docker stop $CONTAINER_NAME
        fi
        docker rm $CONTAINER_NAME || true
        docker rmi $CONTAINER_IMG_NAME || true
    fi

    # Remove network if already exists
    if docker network ls | grep -q $CONTAINER_NET_NAME; then
        docker network rm $CONTAINER_NET_NAME
    fi
}

trap_handler() {
    remove_container
}
trap "echo 'Stopping...'; trap_handler" INT ERR

remove_container && docker network create \
    --driver=macvlan \
    --subnet=$SUBNET \
    --gateway=$GATEWAY \
    -o parent=$INTERFACE \
    $CONTAINER_NET_NAME

# create the image
docker build \
    -t $CONTAINER_IMG_NAME \
    --build-arg USERNAME=$USERNAME \
    --build-arg WORKDIR=$MNT_DIR .

# Access the container from any device on the the local network using a custom
# macvlan bridge interface
# NOTE: How to access docker container from the host and vise versa
# https://www.linuxtechi.com/create-use-macvlan-network-in-docker/
# NOTE: The macvlan interface name shouldn't be too long:
# https://github.com/JonathanCasey/mullvad-ubuntu/issues/3
if ! ip link | grep -q $HOST_MACVLAN; then
    sudo ip link add $HOST_MACVLAN link $INTERFACE type macvlan mode bridge
    sudo ip addr add $HOST_MACVLAN_IP_ADDR dev $HOST_MACVLAN
    sudo ip link set $HOST_MACVLAN up
    sudo ip route add $SUBNET dev $HOST_MACVLAN
fi

docker run \
    -dit \
    --privileged \
    --mount type=bind,source=$PWD,target=$MNT_DIR \
    --network $CONTAINER_NET_NAME \
    --ip $CONTAINER_IP_ADDR \
    --name $CONTAINER_NAME $CONTAINER_IMG_NAME

# Copy github host ssh keys to the container
docker cp $HOME/.ssh/github $CONTAINER_NAME:/home/$USERNAME/.ssh
docker cp $HOME/.ssh/github.pub $CONTAINER_NAME:/home/$USERNAME/.ssh
docker cp $HOME/.ssh/known_hosts $CONTAINER_NAME:/home/$USERNAME/.ssh
