### Description

The repo contains examples on configuring and using an embedded Linux system
on a Raspberry Pi.

* The `lfs` directory contains an example of how to set up embedded Linux from
    scratch.
* The `br2-external` directory contains an example of developing a buildroot-
    based system out-of-tree.

### Setup

The repo provides a `Dockerfile` that mounts the container in a way that makes
it available to the host and provides a development environment.

To set up the development environment, run the script below:

`./setup_docker.sh`
