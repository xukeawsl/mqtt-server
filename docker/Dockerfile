FROM ubuntu:22.04

ENV MQTT_ROOT=/root/mqtt-server

ENV GIT_SSL_NO_VERIFY=1

RUN sed -i s@/archive.ubuntu.com/@/mirrors.aliyun.com/@g /etc/apt/sources.list

# Install Required Packages and Clean up APT when done.
RUN apt-get update \
    && apt-get install -y --no-install-recommends  g++ cmake make \
    git openssl libssl-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN git clone https://github.com/xukeawsl/mqtt-server.git $MQTT_ROOT

WORKDIR $MQTT_ROOT

RUN mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make

CMD cd build && /bin/bash -c "./mqtt-server"