#!/bin/sh

docker build -t xv64:latest .
docker run --rm -v ${PWD}/bin:/src/bin -it xv64:latest
