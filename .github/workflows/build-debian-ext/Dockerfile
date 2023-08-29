FROM debian:11.5-slim as builder

LABEL Description="Base image with custom PHP version based on Debian"
LABEL name="spx-profiler-debian11"

ENV DEBIAN_FRONTEND="noninteractive"
ARG PHP_VERSION

RUN apt update && apt upgrade -y && \
    apt install --no-install-recommends -y \
        lsb-release \
        ca-certificates \
        apt-transport-https \
        software-properties-common \
        gnupg2 \
        wget \
        curl \
        git \
        make\
        zlib1g-dev \
        && \
    echo "deb https://packages.sury.org/php/ $(lsb_release -sc) main" | tee /etc/apt/sources.list.d/sury-php.list && \
    wget -qO - https://packages.sury.org/php/apt.gpg | apt-key add - && \
    apt update && \
    apt install -y php${PHP_VERSION}-dev
