/* SPX - A seamless profiler for PHP
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
 */

import * as math from './math.js';

export function getCookieVar(name) {
    const escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    let m = document.cookie.match(new RegExp('(^|\\b)' + escaped + '=([^;]+)'));

    return m ? m[2] : null;
}

export function setCookieVar(name, value) {
    document.cookie =
        name + '=' + value + '; expires=Thu, 31 Dec 2037 23:59:59 UTC; path=/';
}

export function truncateFunctionName(str, max) {
    if (str.length < max) {
        return str;
    }

    return str.slice(0, max / 2 - 1) + '…' + str.slice(-max / 2);
}

function process(func, async, delay) {
    if (async || false) {
        setTimeout(func, delay || 0);
    } else {
        func();
    }
}

export function processPipeline(calls, async, delay) {
    calls = calls.slice(0);
    calls.reverse();
    let ret;

    function makeNextCall() {
        if (calls.length == 0) {
            return;
        }

        let call = calls.pop();
        ret = call(ret);

        if (calls.length > 0) {
            process(makeNextCall, async, delay);
        }
    }

    process(makeNextCall, async, 0);
}

export function processCallChain(calls, async, delay) {
    calls = calls.slice(0);
    calls.reverse();

    function makeNextCall() {
        if (calls.length == 0) {
            return;
        }

        let call = calls.pop();

        process(
            () => {
                call(makeNextCall);
            },
            async,
            delay || 0
        );
    }

    makeNextCall();
}

export function processChunksAsync(
    from,
    to,
    chunkSize,
    chunkProcessor,
    done,
    delay
) {
    if (chunkSize < 1) {
        chunkSize = chunkSize * (to - from);
    }

    chunkSize = math.bound(Math.round(chunkSize), 1, to - from);

    let chunks = [];

    while (1) {
        chunks.push([from, Math.min(to, from + chunkSize)]);
        if (from + chunkSize >= to) {
            break;
        }

        from += chunkSize;
    }

    chunks.reverse();

    function processChunk() {
        if (chunks.length == 0) {
            done();

            return;
        }

        let chunk = chunks.pop();
        chunkProcessor(chunk[0], chunk[1]);
        setTimeout(processChunk, delay || 0);
    }

    setTimeout(processChunk, 0);
}

// found here: https://stackoverflow.com/questions/75988682/debounce-in-javascript
export const debounce = (callback, wait) => {
    let timeoutId = null;
    return (...args) => {
        window.clearTimeout(timeoutId);
        timeoutId = window.setTimeout(() => {
            callback(...args);
        }, wait);
    };
};

export function createRegexFromSearchQuery(searchQuery) {
    try {
        if (searchQuery.startsWith('/') && searchQuery.endsWith('/')) {
            return new RegExp(searchQuery.slice(1, -1), 'i');
        }

        const pattern = searchQuery
            .replace(/[.+^${}()|[\]\\]/g, '\\$&')
            .replace(/\*/g, '.*')
            .replace(/\?/g, '.');

        return new RegExp(pattern, 'i');
    } catch (e) {
        console.warn('Invalid searchQuery:', searchQuery, e);

        return null;
    }
}
