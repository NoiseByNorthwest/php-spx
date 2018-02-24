import * as math from './math.js';

export function getCookieVar(name) {
    let m = document.cookie.match(new RegExp('(^|\\b)' + name + '=([^;]+)'));

    return m ? m[2] : null;
}

export function setCookieVar(name, value) {
    document.cookie = name + '=' + value + '; expires=Thu, 31 Dec 2037 23:59:59 UTC; path=/';
}

export function truncateFunctionName(str, max) {
    if (str.length < max) {
        return str;
    }

    return str.slice(0, max / 2 - 1) + '…' + str.slice(-max / 2);
}

function process(func, async, delay) {
    if (async || false) {
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

        process(() => {
            call(makeNextCall);
        }, async, delay || 0);
    }

    makeNextCall();
}

export function processChunksAsync(from, to, chunkSize, chunkProcessor, done, delay) {
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
