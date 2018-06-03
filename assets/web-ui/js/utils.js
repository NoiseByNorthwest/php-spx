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

export class PackedRecordArray {

    constructor(fields, size) {
        this.typesInfo = {
            'int8': {
                arrayType: Int8Array,
            },
            'int32': {
                arrayType: Int32Array,
            },
            'float32': {
                arrayType: Float32Array,
            },
            'float64': {
                arrayType: Float64Array,
            },
        };

        this.fields = [];
        this.elemSize = 0;
        for (let fieldName in fields) {
            const type = fields[fieldName];
            if (!(type in this.typesInfo)) {
                throw new Error('Unsupported type: ' + type);
            }

            const typeInfo = this.typesInfo[type];
            this.fields.push({
                name: fieldName,
                typeInfo: typeInfo,
            });

            this.elemSize += typeInfo.arrayType.BYTES_PER_ELEMENT;
        }

        this.fields.sort(
            (a, b) => b.typeInfo.arrayType.BYTES_PER_ELEMENT - a.typeInfo.arrayType.BYTES_PER_ELEMENT
        );

        const alignment = this.fields[0].typeInfo.arrayType.BYTES_PER_ELEMENT;

        this.elemSize = Math.ceil(this.elemSize / alignment) * alignment;
        this.size = size;

        this.buffer = new ArrayBuffer(this.size * this.elemSize);

        this.fieldIndexes = {};
        let currentIdx = 0;
        let currentOffset = 0;
        for (let field of this.fields) {
            this.fieldIndexes[field.name] = currentIdx++;

            field.typeElemSize = this.elemSize / field.typeInfo.arrayType.BYTES_PER_ELEMENT;
            field.typeOffset = currentOffset / field.typeInfo.arrayType.BYTES_PER_ELEMENT;
            field.typeArray = new field.typeInfo.arrayType(this.buffer);

            currentOffset += field.typeInfo.arrayType.BYTES_PER_ELEMENT;
        }
    }

    getSize() {
        return this.size;
    }

    setElement(idx, obj) {
        const elemOffset = idx * this.elemSize;

        for (let field of this.fields) {
            field.typeArray[idx * field.typeElemSize + field.typeOffset] = obj[field.name] || 0;
        }
    }

    setElementFieldValue(idx, fieldName, fieldValue) {
        if (!(fieldName in this.fieldIndexes)) {
            throw new Error('Unknown field: ' + fieldName);
        }

        const field = this.fields[this.fieldIndexes[fieldName]];

        field.typeArray[idx * field.typeElemSize + field.typeOffset] = fieldValue || 0;
    }

    getElement(idx) {
        const elemOffset = idx * this.elemSize;

        let obj = {};
        for (let field of this.fields) {
            obj[field.name] = field.typeArray[idx * field.typeElemSize + field.typeOffset];
        }

        return obj;
    }

    getElementFieldValue(idx, fieldName) {
        if (!(fieldName in this.fieldIndexes)) {
            throw new Error('Unknown field: ' + fieldName);
        }

        const field = this.fields[this.fieldIndexes[fieldName]];

        return field.typeArray[idx * field.typeElemSize + field.typeOffset];
    }
}

/*
    FIXME move all categories related stuff elsewhere (e.g. in a dedicated module)
*/
let categCache = null;
const categStoreKey = 'spx-report-current-categories';

export function getCategories(includeUncategorized=false) {
    if (categCache === null) {
        let loaded = window.localStorage.getItem(categStoreKey);
        categCache = !!loaded ? JSON.parse(loaded): [];
        categCache.forEach(c => {
            c.patterns = c.patterns.map(p => new RegExp(p, 'gi'))
        });
    }

    if (includeUncategorized) {
        let all = categCache.slice();
        all.push({
            label: '<uncategorized>',
            color: [140,140,140],
            patterns: [/./],
            isDefault: true
        });
        return all;
    }
    return categCache;
}

export function setCategories(categories) {
    categCache = null;
    categories = categories.filter(c => !c.isDefault)
    categories.forEach(c => {
        c.patterns = c.patterns.map(p => p.source)
    });
    window.localStorage.setItem(categStoreKey, JSON.stringify(categories));
}
