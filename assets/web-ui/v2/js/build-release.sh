#!/bin/bash
set -ex
cd $(dirname $0)

npx esbuild main.js --bundle --format=esm \
    --sourcemap --sources-content=true \
    --banner:js='/*! SPX - A seamless profiler for PHP
 * Copyright (C) 2017-2026 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */' \
    --define:DEBUG=false --external:*.min.js --external:node_modules --outfile=spx.js.tmp

npx terser spx.js.tmp \
    --compress passes=3,drop_console=false,keep_classnames=true,keep_fnames=true --mangle \
    --source-map "content=spx.js.tmp.map,url=inline,filename=spx.js" \
    --output spx.js
