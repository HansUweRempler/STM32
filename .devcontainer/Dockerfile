FROM mcr.microsoft.com/vscode/devcontainers/base:ubuntu

# use this Dockerfile to install additional tools you might need, e.g.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends build-essential cmake git stlink-tools \
    && apt-get -y install gcc-arm-none-eabi
    # temp deactivated to analyze Coder devcontainer
    #\
    #&& git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git /usr/local/src/FreeRTOS-Kernel
