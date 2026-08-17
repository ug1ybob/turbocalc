FROM ubuntu:26.04 AS builder
WORKDIR /build
COPY . .
RUN apt-get update \
 && apt-get install build-essential upx -y \
 && ls -la \
 && make

FROM scratch
LABEL org.opencontainers.image.authors="https://github.com/ug1ybob"
COPY --link --from=builder --chmod=755 /build/turbocalc /
ENTRYPOINT ["/turbocalc"]
