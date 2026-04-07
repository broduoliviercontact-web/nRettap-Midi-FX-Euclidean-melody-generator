FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-aarch64-linux-gnu \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .
RUN make aarch64
