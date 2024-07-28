# build for ubuntu 20.04
DOCKER_BUILDKIT=1 docker build --file Dockerfile-ubuntu --build-arg "VERSION=20.04" --output type=local,dest=lib .

# build for ubuntu 18.04
DOCKER_BUILDKIT=1 docker build --file Dockerfile-ubuntu --build-arg "VERSION=18.04" --output type=local,dest=lib .

# build for alpine 3.14.0
DOCKER_BUILDKIT=1 docker build --file Dockerfile-alpine --build-arg "VERSION=3.14.0" --output type=local,dest=lib .
