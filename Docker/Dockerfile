FROM vankiaio/builder as builder
ARG branch=master
ARG symbol=VKT

RUN apt-get update && apt-get install -y \
    clang-6.0 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-6.0 1000 \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-6.0 1000 \
    && update-alternatives --config clang \
    && update-alternatives --config clang++ 

RUN git clone -b $branch https://github.com/vankiaio/vkt.git --recursive \
    && cd vkt && echo "$branch:$(git rev-parse HEAD)" > /etc/eosio-version \
    && cmake -H. -B"/tmp/build" -GNinja -DCMAKE_BUILD_TYPE=Release -DWASM_ROOT=/opt/wasm -DCMAKE_CXX_COMPILER=clang++ \
       -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX=/tmp/build -DBUILD_MONGO_DB_PLUGIN=true -DCORE_SYMBOL_NAME=$symbol \
       -DEOSIO_ROOT_KEY=VKT8J1Nu3f6NCWbPPZ8DwVM4XSLTfDhFdwPrgvyVzKrH85KwKh365 \
    && cmake --build /tmp/build --target install && rm /tmp/build/bin/eosiocpp


FROM ubuntu:18.04
LABEL author="LiYi@vankia.cn" \
  maintainer="vankia.io" \
  version="1.3.2" \
  description="This is an vankiaio/vkt image"

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install openssl ca-certificates && rm -rf /var/lib/apt/lists/*
COPY --from=builder /usr/local/lib/* /usr/local/lib/
COPY --from=builder /tmp/build/bin /opt/eosio/bin
COPY --from=builder /tmp/build/contracts /contracts
COPY --from=builder /vkt/Docker/config.ini /
COPY --from=builder /etc/eosio-version /etc
COPY --from=builder /vkt/Docker/nodeosd.sh /opt/eosio/bin/nodeosd.sh
ENV EOSIO_ROOT=/opt/eosio
RUN chmod +x /opt/eosio/bin/nodeosd.sh
ENV LD_LIBRARY_PATH /usr/local/lib
ENV PATH /opt/eosio/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
