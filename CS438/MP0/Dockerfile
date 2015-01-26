FROM ubuntu:14.04
MAINTAINER Ray Wang <raywang945@gmail.com>
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    gdb \
    valgrind \
    iperf \
    tcpdump
RUN mkdir -p /workspace/UIUC-sprint-2015/CS438/MP0/
COPY Makefile /workspace/UIUC-sprint-2015/CS438/MP0/
COPY mp0client.c /workspace/UIUC-sprint-2015/CS438/MP0/
WORKDIR /workspace/UIUC-sprint-2015/CS438/MP0/
