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
            value = this._rewritePath(value);
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

    _rewritePath(path) {
        if (this.pathPrefixFrom && path.startsWith(this.pathPrefixFrom)) {
            return this.pathPrefixTo + path.slice(this.pathPrefixFrom.length);
        }

        return path;
    }

    _metricLabel(key) {
        const info =
            this.metricsInfo && this.metricsInfo.find((m) => m.key === key);
        return info ? info.name.replace(/ /g, '_') : key;
    }
}

class ReportExporterDialog {
    static #dialog = null;
    static #statusElement = null;
    static #progressBar = null;
    static #pathFromInput = null;
    static #pathToInput = null;
    static #exportButton = null;
    static #cancelButton = null;
    static #reportKey = null;
    static #abortController = null;

    static open(reportKey) {
        this.#init();

        this.#reportKey = reportKey;
        this.#setExporting(false);
        this.#progressBar.reset();
        this.#statusElement.textContent = '';
        this.#statusElement.classList.remove('report-exporter-error');

        this.#dialog.showModal();
    }

    static #init() {
        if (this.#dialog) {
            return;
        }

        this.#progressBar = new ProgressBar();

        this.#dialog = document.createElement('dialog');
        this.#dialog.id = 'report-exporter-dialog';
        this.#dialog.innerHTML = `
            <div id="report-exporter-title">Export to Callgrind</div>
            <div id="report-exporter-path-rewrite">
                <div id="report-exporter-path-rewrite-label">Path prefix rewrite <span>(optional)</span></div>
                <div class="report-exporter-path-row">
                    <label for="report-exporter-path-from">From</label
                    ><input type="text" id="report-exporter-path-from" placeholder="/var/www/project">
                </div>
                <div class="report-exporter-path-row">
                    <label for="report-exporter-path-to">To</label
                    ><input type="text" id="report-exporter-path-to" placeholder="/home/user/project">
                </div>
            </div>
            <div id="report-exporter-status"></div>
            <div id="report-exporter-actions">
                <button id="report-exporter-cancel">Cancel</button><!--
                --><button id="report-exporter-export">Export</button>
            </div>
        `;

        this.#dialog
            .querySelector('#report-exporter-status')
            .after(this.#progressBar.element, this.#progressBar.infoElement);

        document.body.appendChild(this.#dialog);

        this.#statusElement = this.#dialog.querySelector(
            '#report-exporter-status'
        );
        this.#pathFromInput = this.#dialog.querySelector(
            '#report-exporter-path-from'
        );
        this.#pathToInput = this.#dialog.querySelector(
            '#report-exporter-path-to'
        );
        this.#exportButton = this.#dialog.querySelector(
            '#report-exporter-export'
        );
        this.#cancelButton = this.#dialog.querySelector(
            '#report-exporter-cancel'
        );

        this.#pathFromInput.value =
            localStorage.getItem('spx-callgrind-path-from') || '';
        this.#pathToInput.value =
            localStorage.getItem('spx-callgrind-path-to') || '';

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

    static #setExporting(active) {
        this.#pathFromInput.disabled = active;
        this.#pathToInput.disabled = active;
        this.#exportButton.disabled = active;
    }

    static #startExport() {
        this.#statusElement.textContent = '';
        this.#statusElement.classList.remove('report-exporter-error');
        this.#progressBar.reset();
        this.#setExporting(true);

        const pathFrom = this.#pathFromInput.value.trim();
        const pathTo = this.#pathToInput.value.trim();

        localStorage.setItem('spx-callgrind-path-from', pathFrom);
        localStorage.setItem('spx-callgrind-path-to', pathTo);

        const abortController = new AbortController();
        this.#abortController = abortController;

        const callgrindBuilder = new CallgrindBuilder();
        if (pathFrom) {
            callgrindBuilder.setPathPrefixRewrite(pathFrom, pathTo);
        }

        readReport(
            this.#reportKey,
            {
                metrics: (metricsInfo) =>
                    callgrindBuilder.setMetricsInfo(metricsInfo),
                metadata: (metadata) => callgrindBuilder.setMetadata(metadata),
                function: (idx, name, file, lineNumber) =>
                    callgrindBuilder.setFunctionInfo(
                        idx,
                        name,
                        file,
                        lineNumber
                    ),
                event: (event) => callgrindBuilder.addEvent(event),
                progress: (current, total) =>
                    this.#progressBar.update(current, total),
            },
            abortController.signal
        )
            // Guard against a newer export resolving over this one's state
            // (cancel + reopen + re-export within the same event loop tick).
            .then(() => {
                if (this.#abortController !== abortController) {
                    return;
                }
                this.#abortController = null;
                this.#downloadFile(
                    'callgrind.out.spx-' + this.#reportKey,
                    callgrindBuilder.getContent()
                );
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

    static #downloadFile(filename, content) {
        const blob = new Blob([content], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }
}

export function exportCallgrind(reportKey) {
    ReportExporterDialog.open(reportKey);
}
