#!/bin/bash
set -ex
cd $(dirname $0)

npm run lint:fix -- --ignore-pattern spx.js
