FROM ubuntu:14.04
MAINTAINER Ray Wang <raywang945@gmail.com>
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    gdb \
    valgrind \
    libpolarssl5 \
    libpolarssl-dev \
    iperf \
    tcpdump
RUN mkdir -p /workspace/MP0/
RUN mkdir -p /workspace/MP1/
COPY MP0/Makefile /workspace/MP0/
COPY MP0/mp0client.c /workspace/MP0/
WORKDIR /workspace/
