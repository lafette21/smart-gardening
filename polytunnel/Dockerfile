FROM registry.suse.com/bci/bci-base:15.5

RUN zypper update

RUN zypper install --no-confirm \
    cmake \
    gcc12 \
    gcc12-c++ \
    git \
    libopenssl-devel \
    libxslt-tools \
    wget

ENV CC=/usr/bin/gcc-12
ENV CXX=/usr/bin/g++-12

RUN git clone https://github.com/DaveGamble/cJSON.git \
    && cd cJSON \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make -j6 \
    && make install

RUN wget https://mosquitto.org/files/source/mosquitto-2.0.18.tar.gz \
    && tar -xvf mosquitto-2.0.18.tar.gz \
    && cd mosquitto-2.0.18 \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make -j6 \
    && make install

ENV LD_LIBRARY_PATH=/usr/local/lib64

RUN mkdir -p /opt/polytunnel

COPY polytunnel.c /opt/polytunnel
COPY measure_simulation.h /opt/polytunnel
COPY measure_simulation_test.h /opt/polytunnel

RUN cd /opt/polytunnel \
    && gcc-12 polytunnel.c -o polytunnel -lmosquitto -lrt

ENTRYPOINT ["/opt/polytunnel/polytunnel"]
