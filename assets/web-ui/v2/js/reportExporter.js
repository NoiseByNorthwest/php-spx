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

import { readReport } from './reportReader.js';
import { ProgressBar } from './progressBar.js';

function rewritePath(path, prefixFrom, prefixTo) {
    if (prefixFrom && path.startsWith(prefixFrom)) {
        return prefixTo + path.slice(prefixFrom.length);
    }
    return path;
}

async function gzipBytes(bytes) {
    const cs = new CompressionStream('gzip');
    const writer = cs.writable.getWriter();
    writer.write(bytes);
    writer.close();

    const chunks = [];
    const reader = cs.readable.getReader();
    for (;;) {
        const { done, value } = await reader.read();
        if (done) {
            break;
        }
        chunks.push(value);
    }

    const total = chunks.reduce((s, c) => s + c.length, 0);
    const out = new Uint8Array(total);
    let offset = 0;
    for (const chunk of chunks) {
        out.set(chunk, offset);
        offset += chunk.length;
    }
    return out;
}

class CallgrindBuilder {
    constructor() {
        this.pathPrefixFrom = null;
        this.pathPrefixTo = null;
        this.stack = [];
        this.selfCosts = new Map();
        // Aggregating per edge keeps the output small regardless of call count
        this.edgeCosts = new Map();
    }

    setMetricsInfo(metricsInfo) {
        this.metricsInfo = metricsInfo;
    }

    setMetadata(metadata) {
        this.metadata = metadata;
        this.enabledMetrics = metadata.enabled_metrics;
        this.functionInfos = Array(metadata.called_function_count);
        for (let i = 0; i < this.functionInfos.length; i++) {
            this.functionInfos[i] = {
                name: 'n/a',
                file: 'unknown',
                line: 0,
            };
        }
    }

    setPathPrefixRewrite(from, to) {
        this.pathPrefixFrom = from;
        this.pathPrefixTo = to;
    }

    setFunctionInfo(idx, name, file, lineNumber) {
        this.functionInfos[idx] = {
            name,
            file: file || 'unknown',
            line: lineNumber || 0,
        };
    }

    addEvent(event) {
        const fnIdx = event[0];
        const metricCount = this.enabledMetrics.length;

        if (event[1]) {
            const start = Array(metricCount);
            for (let i = 0; i < metricCount; i++) {
                start[i] = event[2 + i];
            }

            this.stack.push({
                fnIdx,
                start,
                children: Array(metricCount).fill(0),
                callSiteLine: event[2 + metricCount] || 0,
            });

            return;
        }

        const frame = this.stack.pop();

        const inc = Array(metricCount);
        for (let i = 0; i < metricCount; i++) {
            inc[i] = event[2 + i] - frame.start[i];
        }

        let selfCost = this.selfCosts.get(frame.fnIdx);
        if (!selfCost) {
            selfCost = Array(metricCount).fill(0);
            this.selfCosts.set(frame.fnIdx, selfCost);
        }
        for (let i = 0; i < metricCount; i++) {
            selfCost[i] += inc[i] - frame.children[i];
        }

        if (this.stack.length === 0) {
            return;
        }

        const parent = this.stack[this.stack.length - 1];

        for (let i = 0; i < metricCount; i++) {
            parent.children[i] += inc[i];
        }

        // Distinct call sites for the same caller→callee pair are kept as
        // separate edges, matching callgrind's per-line cost attribution.
        const edgeKey =
            parent.fnIdx + ':' + frame.fnIdx + ':' + frame.callSiteLine;
        let edge = this.edgeCosts.get(edgeKey);
        if (!edge) {
            edge = {
                callerIdx: parent.fnIdx,
                calleeIdx: frame.fnIdx,
                callSiteLine: frame.callSiteLine,
                calls: 0,
                costs: Array(metricCount).fill(0),
            };
            this.edgeCosts.set(edgeKey, edge);
        }
        edge.calls++;
        for (let i = 0; i < metricCount; i++) {
            edge.costs[i] += inc[i];
        }
    }

    getContent() {
        if (!this.metadata) {
            throw new Error('getContent() called before setMetadata()');
        }

        const lines = [];
        this._renderHeader(lines);
        this._renderSummary(lines);
        this._renderBody(lines);

        return lines.join('\n');
    }

    _renderHeader(lines) {
        const cmd = this.metadata.cli
            ? this.metadata.cli_command_line
            : (this.metadata.http_method || '') +
              ' ' +
              (this.metadata.http_request_uri || '');

        lines.push('# callgrind format');
        lines.push('version: 1');
        lines.push('creator: php-spx');
        lines.push('pid: ' + this.metadata.process_pid);
        lines.push('cmd: ' + cmd.trim());
        lines.push('part: 1');
        lines.push('');
        lines.push('positions: line');
        lines.push(
            'events: ' +
                this.enabledMetrics.map((k) => this._metricLabel(k)).join(' ')
        );
    }

    _renderSummary(lines) {
        const metricCount = this.enabledMetrics.length;
        const totals = Array(metricCount).fill(0);
        for (const selfCost of this.selfCosts.values()) {
            for (let i = 0; i < metricCount; i++) {
                // Callgrind event values must be non-negative integers; some
                // metrics (e.g. memory) can decrease, so clamp at zero.
                totals[i] += Math.max(0, Math.round(selfCost[i]));
            }
        }
        lines.push('summary: ' + totals.join(' '));
        lines.push('');
    }

    _renderBody(lines) {
        const edgesByCallerIdx = new Map();
        for (const edge of this.edgeCosts.values()) {
            let callees = edgesByCallerIdx.get(edge.callerIdx);
            if (!callees) {
                callees = [];
                edgesByCallerIdx.set(edge.callerIdx, callees);
            }
            callees.push(edge);
        }

        const fileIds = new Map();
        const fnIds = new Map();

        const makeRef = (prefix, map) => (value) => {
            value = rewritePath(value, this.pathPrefixFrom, this.pathPrefixTo);
            if (map.has(value)) {
                return prefix + '=(' + map.get(value) + ')';
            }
            const id = map.size + 1;
            map.set(value, id);
            return prefix + '=(' + id + ') ' + value;
        };

        const flRef = makeRef('fl', fileIds);
        const fnRef = makeRef('fn', fnIds);
        const cflRef = makeRef('cfl', fileIds);
        const cfnRef = makeRef('cfn', fnIds);

        for (const [fnIdx, selfCost] of this.selfCosts) {
            const info = this.functionInfos[fnIdx];

            lines.push(flRef(info.file));
            lines.push(fnRef(info.name));
            lines.push(
                info.line +
                    ' ' +
                    selfCost.map((v) => Math.max(0, Math.round(v))).join(' ')
            );

            for (const edge of edgesByCallerIdx.get(fnIdx) || []) {
                const calleeInfo = this.functionInfos[edge.calleeIdx];
                lines.push(cfnRef(calleeInfo.name));
                lines.push(cflRef(calleeInfo.file));
                lines.push('calls=' + edge.calls + ' ' + calleeInfo.line);
                lines.push(
                    edge.callSiteLine +
                        ' ' +
                        edge.costs
                            .map((v) => Math.max(0, Math.round(v)))
                            .join(' ')
                );
            }

            lines.push('');
        }
    }

    _metricLabel(key) {
        const info =
            this.metricsInfo && this.metricsInfo.find((m) => m.key === key);
        return info ? info.name.replace(/ /g, '_') : key;
    }
}

class ExporterDialogBase {
    #dialog = null;
    #statusElement = null;
    #progressBar = null;
    #pathFromInput = null;
    #pathToInput = null;
    #exportButton = null;
    #cancelButton = null;
    #reportKey = null;
    #abortController = null;

    open(reportKey) {
        this.#init();

        this.#reportKey = reportKey;
        this.#setExporting(false);
        this.#progressBar.reset();
        this.#statusElement.textContent = '';
        this.#statusElement.classList.remove('report-exporter-error');

        this.#pathFromInput.value =
            localStorage.getItem('spx-exporter-path-from') || '';
        this.#pathToInput.value =
            localStorage.getItem('spx-exporter-path-to') || '';

        this.#dialog.showModal();
    }

    #init() {
        if (this.#dialog) {
            return;
        }

        this.#progressBar = new ProgressBar();

        this.#dialog = document.createElement('dialog');
        this.#dialog.className = 'report-exporter-dialog';
        this.#dialog.innerHTML = `
            <div class="report-exporter-title">Export to ${this._title}</div>
            <div class="report-exporter-path-rewrite">
                <div class="report-exporter-path-rewrite-label">Path prefix rewrite <span>(optional)</span></div>
                <label class="report-exporter-path-row">
                    <span>From</span
                    ><input type="text" class="report-exporter-path-from" placeholder="/var/www/project">
                </label>
                <label class="report-exporter-path-row">
                    <span>To</span
                    ><input type="text" class="report-exporter-path-to" placeholder="/home/user/project">
                </label>
            </div>
            <div class="report-exporter-status"></div>
            <div class="report-exporter-actions">
                <button class="report-exporter-cancel">Cancel</button><!--
                --><button class="report-exporter-export">Export</button>
            </div>
        `;

        this.#dialog
            .querySelector('.report-exporter-status')
            .after(this.#progressBar.element, this.#progressBar.infoElement);

        document.body.appendChild(this.#dialog);

        this.#statusElement = this.#dialog.querySelector(
            '.report-exporter-status'
        );
        this.#pathFromInput = this.#dialog.querySelector(
            '.report-exporter-path-from'
        );
        this.#pathToInput = this.#dialog.querySelector(
            '.report-exporter-path-to'
        );
        this.#exportButton = this.#dialog.querySelector(
            '.report-exporter-export'
        );
        this.#cancelButton = this.#dialog.querySelector(
            '.report-exporter-cancel'
        );

        this.#exportButton.addEventListener('click', () => this.#startExport());

        const onClose = (e) => {
            e.preventDefault();
            if (this.#abortController) {
                this.#abortController.abort();
                this.#abortController = null;
            }
            this.#dialog.close();
        };

        this.#cancelButton.addEventListener('click', onClose);
        this.#dialog.addEventListener('cancel', onClose);
    }

    #setExporting(active) {
        this.#pathFromInput.disabled = active;
        this.#pathToInput.disabled = active;
        this.#exportButton.disabled = active;
    }

    #startExport() {
        this.#statusElement.textContent = '';
        this.#statusElement.classList.remove('report-exporter-error');
        this.#progressBar.reset();
        this.#setExporting(true);

        const pathFrom = this.#pathFromInput.value.trim();
        const pathTo = this.#pathToInput.value.trim();

        localStorage.setItem('spx-exporter-path-from', pathFrom);
        localStorage.setItem('spx-exporter-path-to', pathTo);

        const abortController = new AbortController();
        this.#abortController = abortController;

        const builder = this._makeBuilder();
        if (pathFrom) {
            builder.setPathPrefixRewrite(pathFrom, pathTo);
        }

        readReport(
            this.#reportKey,
            {
                metrics: (metricsInfo) => builder.setMetricsInfo(metricsInfo),
                metadata: (metadata) => builder.setMetadata(metadata),
                function: (idx, name, file, lineNumber) =>
                    builder.setFunctionInfo(idx, name, file, lineNumber),
                event: (event) => builder.addEvent(event),
                progress: (current, total) =>
                    this.#progressBar.update(current, total),
            },
            abortController.signal
        )
            // Guard against a newer export resolving over this one's state
            // (cancel + reopen + re-export within the same event loop tick).
            .then(() => builder.getContent())
            .then((content) => {
                if (this.#abortController !== abortController) {
                    return;
                }
                this.#abortController = null;
                this.#downloadFile(this._filename(this.#reportKey), content);
                this.#dialog.close();
            })
            .catch((e) => {
                if (this.#abortController !== abortController) {
                    return;
                }
                this.#abortController = null;
                if (e.name === 'AbortError') {
                    return;
                }

                this.#setExporting(false);
                this.#statusElement.textContent = 'Error: ' + e.message;
                this.#statusElement.classList.add('report-exporter-error');
            });
    }

    #downloadFile(filename, content) {
        const blob = new Blob([content], { type: this._fileType });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }

    // Subclass hooks
    get _title() {
        throw new Error('abstract: _title');
    }

    get _fileType() {
        return 'application/octet-stream';
    }

    _makeBuilder() {
        throw new Error('abstract: _makeBuilder');
    }

    _filename(/* reportKey */) {
        throw new Error('abstract: _filename');
    }
}

class CallgrindExporterDialog extends ExporterDialogBase {
    get _title() {
        return 'Callgrind';
    }

    get _fileType() {
        return 'text/plain';
    }

    _makeBuilder() {
        return new CallgrindBuilder();
    }

    _filename(reportKey) {
        return 'callgrind.out.' + reportKey;
    }
}

let callgrindExporterDialog = null;

export function exportCallgrind(reportKey) {
    if (!callgrindExporterDialog) {
        callgrindExporterDialog = new CallgrindExporterDialog();
    }
    callgrindExporterDialog.open(reportKey);
}

// --- Minimal protobuf encoder for the pprof profile.proto schema ---

class ByteBuffer {
    constructor() {
        this._buf = new Uint8Array(4096);
        this._pos = 0;
    }

    _grow(needed) {
        if (this._buf.length - this._pos >= needed) {
            return;
        }
        let size = this._buf.length * 2;
        while (size - this._pos < needed) {
            size *= 2;
        }
        const next = new Uint8Array(size);
        next.set(this._buf);
        this._buf = next;
    }

    byte(b) {
        this._grow(1);
        this._buf[this._pos++] = b;
    }

    bytes(src) {
        this._grow(src.length);
        this._buf.set(src, this._pos);
        this._pos += src.length;
    }

    // Encode a non-negative integer as a varint (wire type 0).
    // Uses Math.floor(v/128) instead of v>>>7 to handle values above 2^31.
    varint(v) {
        while (v > 127) {
            this.byte((v & 0x7f) | 0x80);
            v = Math.floor(v / 128);
        }
        this.byte(v & 0x7f);
    }

    // field tag: (fieldNumber << 3) | wireType
    tag(field, wire) {
        this.varint((field << 3) | wire);
    }

    // int64/uint64 field (wire type 0), omitted when value is 0 (proto default)
    f_int(field, v) {
        if (v === 0) {
            return;
        }
        this.tag(field, 0);
        this.varint(v);
    }

    // length-delimited field (wire type 2): embedded message or non-empty bytes,
    // omitted when empty (proto default for messages/bytes/strings)
    f_len(field, src) {
        if (src.length === 0) {
            return;
        }
        this.tag(field, 2);
        this.varint(src.length);
        this.bytes(src);
    }

    // length-delimited field that is always emitted, even when empty.
    // Required for repeated string fields where empty strings are meaningful
    // (e.g. pprof's string_table[0] which MUST be the empty string).
    f_len_always(field, src) {
        this.tag(field, 2);
        this.varint(src.length);
        this.bytes(src);
    }

    // packed repeated varints — packed is the default for repeated scalar
    // fields in proto3, and is also accepted by proto2 readers ([packed=true]).
    f_packed(field, values) {
        if (!values.length) {
            return;
        }
        const tmp = new ByteBuffer();
        for (const v of values) {
            tmp.varint(v);
        }
        this.f_len(field, tmp.done());
    }

    // Returns a view backed by the internal buffer — callers MUST stop writing
    // to this ByteBuffer (or read the view) before any further mutation, since
    // a subsequent _grow() would reallocate and leave the view pointing at the
    // old (still-valid but stale) buffer.
    done() {
        return this._buf.subarray(0, this._pos);
    }
}

const _pprof_te = new TextEncoder();

function _pprofValueType(typeIdx, unitIdx) {
    const b = new ByteBuffer();
    b.f_int(1, typeIdx);
    b.f_int(2, unitIdx);
    return b.done();
}

function _pprofSample(locationIds, values) {
    const b = new ByteBuffer();
    b.f_packed(1, locationIds);
    b.f_packed(2, values);
    return b.done();
}

function _pprofLine(functionId, lineNumber) {
    const b = new ByteBuffer();
    b.f_int(1, functionId);
    b.f_int(2, lineNumber);
    return b.done();
}

function _pprofLocation(id, functionId, lineNumber) {
    const b = new ByteBuffer();
    b.f_int(1, id);
    b.f_len(4, _pprofLine(functionId, lineNumber));
    return b.done();
}

function _pprofFunction(id, nameIdx, filenameIdx, startLine) {
    const b = new ByteBuffer();
    b.f_int(1, id);
    b.f_int(2, nameIdx);
    // system_name is meant for mangled/linker-level symbols (C++/Rust);
    // PHP has no name mangling, so it matches the user-visible name.
    b.f_int(3, nameIdx);
    b.f_int(4, filenameIdx);
    b.f_int(5, startLine);
    return b.done();
}

// SPX metric key → pprof unit string
const _PPROF_METRIC_UNITS = {
    wt: 'nanoseconds',
    ct: 'nanoseconds',
    it: 'nanoseconds',
    zm: 'bytes',
    zmab: 'bytes',
    zmfb: 'bytes',
    mor: 'bytes',
    io: 'bytes',
    ior: 'bytes',
    iow: 'bytes',
};

// --- PprofBuilder ---

class PprofBuilder {
    constructor() {
        this.pathPrefixFrom = null;
        this.pathPrefixTo = null;
        this.stack = [];
        this._stringTable = new Map([['', 0]]);
        this._functions = new Map(); // spxFnIdx → {pprofId, nameIdx, filenameIdx, startLine}
        this._nextFnId = 1;
        this._samples = []; // {locationIds: number[], values: number[]}
    }

    setMetricsInfo(metricsInfo) {
        this.metricsInfo = metricsInfo;
    }

    setMetadata(metadata) {
        this.metadata = metadata;
        this.enabledMetrics = metadata.enabled_metrics;
        this.functionInfos = Array(metadata.called_function_count);
        for (let i = 0; i < this.functionInfos.length; i++) {
            this.functionInfos[i] = {
                name: 'n/a',
                file: 'unknown',
                line: 0,
            };
        }
    }

    setPathPrefixRewrite(from, to) {
        this.pathPrefixFrom = from;
        this.pathPrefixTo = to;
    }

    setFunctionInfo(idx, name, file, lineNumber) {
        this.functionInfos[idx] = {
            name,
            file: file || 'unknown',
            line: lineNumber || 0,
        };
    }

    addEvent(event) {
        const fnIdx = event[0];
        const metricCount = this.enabledMetrics.length;

        if (event[1]) {
            const start = Array(metricCount);
            for (let i = 0; i < metricCount; i++) {
                start[i] = event[2 + i];
            }
            this.stack.push({
                fnIdx,
                start,
                childrenCost: Array(metricCount).fill(0),
            });
            return;
        }

        const frame = this.stack.pop();
        const selfCost = Array(metricCount);
        let nonZero = false;

        for (let i = 0; i < metricCount; i++) {
            const inc = event[2 + i] - frame.start[i];
            selfCost[i] = Math.max(0, Math.round(inc - frame.childrenCost[i]));
            if (selfCost[i] > 0) {
                nonZero = true;
            }
            if (this.stack.length > 0) {
                this.stack[this.stack.length - 1].childrenCost[i] += inc;
            }
        }

        if (!nonZero) {
            return;
        }

        // pprof location_ids: innermost (this frame) first, then up the stack
        const locationIds = [frame.fnIdx];
        for (let i = this.stack.length - 1; i >= 0; i--) {
            locationIds.push(this.stack[i].fnIdx);
        }

        this._samples.push({ locationIds, values: selfCost });
    }

    async getContent() {
        if (!this.metadata) {
            throw new Error('getContent() called before setMetadata()');
        }

        // Intern metric type/unit strings first so they land early in the table
        // Reversed so wt (wall time, always first in SPX) ends up last — speedscope
        // defaults to the last sample_type, making it the default view.
        const sampleTypeData = [...this.enabledMetrics].reverse().map((key) => {
            const info =
                this.metricsInfo && this.metricsInfo.find((m) => m.key === key);
            return {
                typeIdx: this._str(info ? info.name : key),
                unitIdx: this._str(_PPROF_METRIC_UNITS[key] || 'count'),
            };
        });

        // Build location map: one location per unique spxFnIdx across all samples
        const locMap = new Map(); // spxFnIdx → pprofLocationId
        let nextLocId = 1;
        for (const { locationIds } of this._samples) {
            for (const fnIdx of locationIds) {
                if (!locMap.has(fnIdx)) {
                    locMap.set(fnIdx, nextLocId++);
                    this._fn(fnIdx); // intern name/file strings
                }
            }
        }

        // Snapshot string table (all strings now interned)
        const strings = Array(this._stringTable.size);
        for (const [s, idx] of this._stringTable) {
            strings[idx] = s;
        }

        // Encode the profile message
        const profile = new ByteBuffer();

        for (const { typeIdx, unitIdx } of sampleTypeData) {
            profile.f_len(1, _pprofValueType(typeIdx, unitIdx));
        }

        for (const { locationIds, values } of this._samples) {
            profile.f_len(
                2,
                _pprofSample(
                    locationIds.map((fnIdx) => locMap.get(fnIdx)),
                    [...values].reverse()
                )
            );
        }

        for (const [fnIdx, locId] of locMap) {
            const fn = this._functions.get(fnIdx);
            profile.f_len(4, _pprofLocation(locId, fn.pprofId, fn.startLine));
        }

        for (const fn of this._functions.values()) {
            profile.f_len(
                5,
                _pprofFunction(
                    fn.pprofId,
                    fn.nameIdx,
                    fn.filenameIdx,
                    fn.startLine
                )
            );
        }

        for (const s of strings) {
            profile.f_len_always(6, _pprof_te.encode(s));
        }

        const durationNanos = Math.round(
            (this.metadata.wall_time_ms || 0) * 1e6
        );
        profile.f_int(10, durationNanos);

        return gzipBytes(profile.done());
    }

    // Intern a string and return its index in the string table
    _str(s) {
        let idx = this._stringTable.get(s);
        if (idx === undefined) {
            idx = this._stringTable.size;
            this._stringTable.set(s, idx);
        }
        return idx;
    }

    // Resolve (and lazily create) the pprof Function record for an SPX function index
    _fn(spxIdx) {
        let fn = this._functions.get(spxIdx);
        if (!fn) {
            const info = this.functionInfos[spxIdx];
            fn = {
                pprofId: this._nextFnId++,
                nameIdx: this._str(info.name),
                filenameIdx: this._str(
                    rewritePath(
                        info.file,
                        this.pathPrefixFrom,
                        this.pathPrefixTo
                    )
                ),
                startLine: info.line,
            };
            this._functions.set(spxIdx, fn);
        }
        return fn;
    }
}

class PprofExporterDialog extends ExporterDialogBase {
    get _title() {
        return 'pprof';
    }

    _makeBuilder() {
        return new PprofBuilder();
    }

    _filename(reportKey) {
        return reportKey + '.pprof.pb.gz';
    }
}

let pprofExporterDialog = null;

export function exportPprof(reportKey) {
    if (!pprofExporterDialog) {
        pprofExporterDialog = new PprofExporterDialog();
    }
    pprofExporterDialog.open(reportKey);
}

// --- TraceEventFormatBuilder ---
// Produces a Trace Event Format JSON file (Chrome DevTools / Perfetto compatible).
//
// Memory note: output is O(events) — every call boundary becomes a B/E object.
// Callgrind and pprof both aggregate, TEF cannot. Validate on large reports
// (~10M+ events) before release.

class TraceEventFormatBuilder {
    constructor() {
        this.pathPrefixFrom = null;
        this.pathPrefixTo = null;
        // [functions] comes after [events] in the report stream, so raw events
        // store fnIdx references and are resolved in getContent().
        this._rawEvents = [];
        this._tsMetricIndex = -1;
    }

    setMetricsInfo(/* metricsInfo */) {
        // unused: TEF has a single time axis
    }

    setMetadata(metadata) {
        this.metadata = metadata;
        this.enabledMetrics = metadata.enabled_metrics;
        this.functionInfos = Array(metadata.called_function_count);
        for (let i = 0; i < this.functionInfos.length; i++) {
            this.functionInfos[i] = { name: 'n/a', file: 'unknown', line: 0 };
        }
        // Prefer wall time for timestamps, then CPU time, then idle time.
        // No fallback to enabledMetrics[0]: a non-time metric (e.g. memory)
        // would produce non-monotonic timestamps. getContent() throws instead.
        for (const key of ['wt', 'ct', 'it']) {
            const idx = this.enabledMetrics.indexOf(key);
            if (idx >= 0) {
                this._tsMetricIndex = idx;
                break;
            }
        }
    }

    setPathPrefixRewrite(from, to) {
        this.pathPrefixFrom = from;
        this.pathPrefixTo = to;
    }

    setFunctionInfo(idx, name, file, lineNumber) {
        this.functionInfos[idx] = {
            name,
            file: file || 'unknown',
            line: lineNumber || 0,
        };
    }

    addEvent(event) {
        if (this._tsMetricIndex < 0) {
            return;
        }
        // SPX metric values are in nanoseconds; TEF ts is in microseconds.
        this._rawEvents.push([
            event[0],
            event[1],
            event[2 + this._tsMetricIndex] / 1000,
        ]);
    }

    async getContent() {
        if (!this.metadata) {
            throw new Error('getContent() called before setMetadata()');
        }
        if (this._tsMetricIndex < 0) {
            throw new Error(
                'Trace Event Format export requires one of wt, ct or it ' +
                    'in enabled metrics'
            );
        }

        const pid = this.metadata.process_pid || 1;

        const cmd =
            (this.metadata.cli
                ? this.metadata.cli_command_line
                : (
                      this.metadata.http_method +
                      ' ' +
                      this.metadata.http_request_uri
                  ).trim()) || '';

        const labels = [
            this.metadata.cli ? 'cli' : 'http',
            this.metadata.host_name,
        ]
            .filter(Boolean)
            .join(',');

        const traceEvents = [
            { name: 'process_name', ph: 'M', pid, args: { name: cmd } },
            { name: 'process_labels', ph: 'M', pid, args: { labels } },
            {
                name: 'thread_name',
                ph: 'M',
                pid,
                tid: 1,
                args: { name: 'Main PHP thread' },
            },
            ...this._rawEvents.map(([fnIdx, isStart, ts]) => {
                const info = this.functionInfos[fnIdx];
                if (isStart) {
                    return {
                        name: info.name,
                        cat: 'php',
                        ph: 'B',
                        ts,
                        pid,
                        tid: 1,
                        args: {
                            file: rewritePath(
                                info.file,
                                this.pathPrefixFrom,
                                this.pathPrefixTo
                            ),
                            line: info.line,
                        },
                    };
                }
                return {
                    name: info.name,
                    cat: 'php',
                    ph: 'E',
                    ts,
                    pid,
                    tid: 1,
                };
            }),
        ];

        const te = new TextEncoder();
        return gzipBytes(te.encode(JSON.stringify({ traceEvents })));
    }
}

class TraceEventFormatExporterDialog extends ExporterDialogBase {
    get _title() {
        return 'Trace Event Format';
    }

    _makeBuilder() {
        return new TraceEventFormatBuilder();
    }

    _filename(reportKey) {
        return reportKey + '.tef.json.gz';
    }
}

let traceEventFormatExporterDialog = null;

export function exportTraceEventFormat(reportKey) {
    if (!traceEventFormatExporterDialog) {
        traceEventFormatExporterDialog = new TraceEventFormatExporterDialog();
    }
    traceEventFormatExporterDialog.open(reportKey);
}
