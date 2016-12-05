FROM ubuntu

ENV http_proxy http://proxy-us.intel.com:911
ENV https_proxy http://proxy-us.intel.com:912
ENV no_proxy intel.com,*.intel.com,127.0.0.1,localhost

RUN apt-get update && \
    apt-get install -y git build-essential ocaml automake autoconf libtool libcurl4-openssl-dev curl make g++ unzip

RUN groupadd sean && useradd -m -s /bin/bash -g sean sean
USER sean

RUN cd ~ && \
    git clone https://github.com/google/protobuf.git && \
    cd protobuf && \
    ./autogen.sh && \
    ./configure && \
    make && \
    make check

USER root
RUN cd /home/sean/protobuf && \
    make install && \
    ldconfig

RUN apt-get install -y wget libssl-dev python
USER sean
RUN cd ~ && \
    mkdir -p ~/STREAM && \
    git clone https://github.com/sean-jc/linux-sgx.git && \
    cd linux-sgx && \
    git checkout -b docker origin/docker && \
    ./download_prebuilt.sh && \
    make && \
    make psw_install_pkg && \
    make sdk_install_pkg
USER root
RUN mkdir -p /opt/intel && \
    cd /opt/intel && \
    sh -c 'echo yes | /home/sean/linux-sgx/linux/installer/bin/sgx_linux_x64_sdk_1.6.100.34922.bin'

USER sean
COPY Makefile main.c stream.c stream_enclave.edl stream_enclave.lds stream_key.pem /home/sean/STREAM/

RUN cd ~/STREAM && make

USER root
#CMD  cd /opt/intel && /home/sean/linux-sgx/linux/installer/bin/sgx_linux_x64_psw_1.6.100.34922.bin && ~/STREAM/stream_sgx
CMD  cd /opt/intel && /home/sean/linux-sgx/linux/installer/bin/sgx_linux_x64_psw_1.6.100.34922.bin && /bin/bash
  