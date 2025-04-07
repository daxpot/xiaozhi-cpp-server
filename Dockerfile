# syntax=docker/dockerfile:1-labs
FROM ubuntu:24.04 AS builder
WORKDIR /app

COPY --parents CMakeLists.txt src extern include apps scripts tests /app/

RUN apt-get update && apt-get install -y \
    cmake \
    g++ && \
    bash scripts/install_deps.sh && \
    mkdir -p build && \
    cd build && \
    cmake -DKALDI_NATIVE_FBANK_BUILD_TESTS=OFF -DKALDI_NATIVE_FBANK_BUILD_PYTHON=OFF -DCMAKE_BUILD_TYPE=Release .. && \
    make web_server

FROM ubuntu:24.04
WORKDIR /app
COPY extern/onnxruntime-linux-x64-1.21.0/lib /app/lib
ENV LD_LIBRARY_PATH=/app/lib:$LD_LIBRARY_PATH

COPY scripts/install_deps.sh /app/scripts/install_deps.sh
RUN bash scripts/install_deps.sh

COPY --from=builder /app/build/lib /app/lib
COPY --from=builder /app/build/apps/web_server /app/web_server

EXPOSE 8000
CMD [ "/app/web_server" ]