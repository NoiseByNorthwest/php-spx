#!/bin/bash
set -ex
cd $(dirname $0)
esbuild main.js --bundle --format=esm --define:DEBUG=true --external:node_modules --outfile=spx.js --watch
