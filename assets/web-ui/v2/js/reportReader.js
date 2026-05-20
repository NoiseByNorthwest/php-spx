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

const noop = () => {};

// Sliding window of recently parsed events. Back-references in the compressed
// format ("rN") can only point to one of the last BACK_REF_WINDOW events.
const BACK_REF_WINDOW = 20;

// Block size for the non-streaming fallback path (whole body read into an
// ArrayBuffer, then yielded to the event loop one block at a time so the UI
// stays responsive).
const FALLBACK_CHUNK_SIZE = 256 * 1024;

/**
 * Parses one line of the [events] section.
 *
 * Two on-wire formats coexist:
 *
 *   1. Legacy (space-separated): "<fnIdx> <start> <m1> <m2> ... [callSiteLine]"
 *      detected by the presence of a space — values map 1:1 to the event tuple.
 *
 *   2. Compressed (pipe-separated): "[callSiteLine|]<fnIdx>|<m1>|<m2>|..."
 *      with several layered tricks to shrink the payload:
 *
 *        - "-" prefix on the first character marks a call END; otherwise START.
 *        - For START events, parts[0] holds the call site line (only meaningful
 *          at call start, so it's omitted for END events — saves one field per
 *          end event).
 *        - "rN" on the fnIdx field is a back-reference: reuse the fnIdx of the
 *          N-th most recent event. Useful for tight loops calling the same fn.
 *        - "aN" on a metric value means "absolute N"; bare "N" means "delta N
 *          from the most recent event's same metric". Empty string = "0 delta".
 *
 * Side effect: pushes parsed compressed-format events onto `recentEvents` so
 * subsequent "rN" / delta lookups resolve correctly.
 */
function parseEventLine(line, recentEvents) {
    if (line.includes(' ')) {
        // legacy format
        return line.split(' ').map((e) => Number(e));
    }

    // compressed format
    const start = line[0] !== '-';
    const parts = line.split('|');
    const startOffset = start ? 1 : 0;

    // +1: slot for event[1] (start/end flag), absent from wire format.
    // For START events, parts[0] (callSiteLine) is not a direct field but gets
    // appended at the tail of the tuple — the extra head field and extra tail
    // slot cancel out, so parts.length + 1 gives the correct size in both cases.
    const event = Array(parts.length + 1);
    event[1] = start ? 1 : 0;

    for (let i = startOffset; i < parts.length; i++) {
        let v = parts[i];
        if (i === startOffset) {
            // fnIdx field
            if (!start) {
                v = v.slice(1); // strip the leading '-' marker
            }

            if (v[0] === 'r') {
                v = recentEvents[Number(v.slice(1)) - 1][i - startOffset];
            } else {
                v = Number(v);
            }

            event[0] = v;
        } else {
            // metric value
            if (v === '') {
                v = '0';
            }

            if (v[0] === 'a') {
                v = Number(v.slice(1));
            } else {
                v = Number(v) + recentEvents[0][i - startOffset + 1];
            }

            event[i - startOffset + 1] = v;
        }
    }

    if (start) {
        // call site line lives at parts[0] and tails the event tuple
        event[event.length - 1] = parseInt(parts[0], 16);
    }

    recentEvents.unshift(event);
    if (recentEvents.length > BACK_REF_WINDOW) {
        recentEvents.length = BACK_REF_WINDOW;
    }

    return event;
}

/**
 * Parses one line of the [functions] section: "file:lineNumber:functionName".
 * The function name may itself contain colons (e.g. "Foo::bar"), so we split,
 * take the first two fields, and rejoin the rest.
 */
function parseFunctionLine(line) {
    const parts = line.split(':');
    const file = parts.shift();
    const lineNumber = Number(parts.shift());

    let functionName = parts.join(':');
    if (functionName === '{closure}') {
        functionName = `{closure:${file}:${lineNumber}}`;
    }

    return { functionName, file, lineNumber };
}

/**
 * Streams a stored SPX report and dispatches its content piece by piece.
 *
 * Handlers (all optional):
 *   metrics(metricsInfo)         — array of {key, name, ...} for every
 *                                  metric known to the backend (catalog from
 *                                  /data/metrics), called once.
 *   metadata(metadata)           — report-level metadata, called once.
 *                                  Notable fields: enabled_metrics (array of
 *                                  metric keys recorded for this report),
 *                                  called_function_count, process_pid,
 *                                  cli / cli_command_line / http_*.
 *   function(idx, name, file, lineNumber)
 *                                — one call per recorded function.
 *   event(event)                 — one call per recorded call boundary.
 *                                  Tuple layout, positional:
 *                                    event[0]                = fnIdx
 *                                    event[1]                = 1 if call
 *                                                              start, 0 if end
 *                                    event[2 .. 2+M-1]       = metric values
 *                                                              (M = number of
 *                                                              enabled metrics)
 *                                    event[2+M]              = call site line
 *                                                              (start events
 *                                                              only)
 *   progress(current, total)     — streaming progress in events.
 */
export function readReport(key, handlers = {}, signal = null) {
    handlers = {
        metrics: noop,
        metadata: noop,
        event: noop,
        function: noop,
        progress: noop,
        ...handlers,
    };

    let totalEventCount = 0;
    let currentEventCount = 0;

    const fetchOptions = {
        credentials: 'same-origin',
        ...(signal ? { signal } : {}),
    };

    return fetch('?SPX_UI_URI=/data/metrics', fetchOptions)
        .then((response) => response.json())
        .then((response) => {
            handlers.metrics(response.results);

            return fetch(
                '?SPX_UI_URI=/data/reports/metadata/' + key,
                fetchOptions
            );
        })
        .then((response) => {
            if (!response.ok) {
                throw new Error('Cannot load report metadata');
            }

            return response.json();
        })
        .then((metadata) => {
            totalEventCount = metadata.recorded_call_count * 2;
            handlers.metadata(metadata);

            return fetch('?SPX_UI_URI=/data/reports/get/' + key, fetchOptions);
        })
        .then((response) => {
            if (!response.ok) {
                throw new Error('Cannot load report');
            }

            let type = null;
            let currentFunctionIdx = 0;
            let lastTruncatedLine = '';
            const recentEvents = [];

            const textDecoder = new TextDecoder('ascii');

            function readBuffer(buffer) {
                if (buffer.length == 0) {
                    return;
                }

                const lines = textDecoder.decode(buffer).split('\n');
                lines[0] = lastTruncatedLine + lines[0];
                lastTruncatedLine = lines.pop();

                for (const line of lines) {
                    if (line === '[events]') {
                        type = 'event';

                        continue;
                    }

                    if (line === '[functions]') {
                        type = 'function';

                        continue;
                    }

                    if (type === 'event') {
                        handlers.event(parseEventLine(line, recentEvents));
                        currentEventCount++;

                        continue;
                    }

                    if (type === 'function') {
                        const { functionName, file, lineNumber } =
                            parseFunctionLine(line);

                        handlers.function(
                            currentFunctionIdx++,
                            functionName,
                            file,
                            lineNumber
                        );

                        continue;
                    }
                }

                handlers.progress(currentEventCount, totalEventCount);
            }

            if (!response.body) {
                return response.arrayBuffer().then((buffer) => {
                    const readBlocks = (offset, resolve) => {
                        if (buffer.byteLength - offset <= FALLBACK_CHUNK_SIZE) {
                            readBuffer(new Uint8Array(buffer, offset));
                            resolve();

                            return;
                        }

                        readBuffer(
                            new Uint8Array(buffer, offset, FALLBACK_CHUNK_SIZE)
                        );

                        setTimeout(
                            () =>
                                readBlocks(
                                    offset + FALLBACK_CHUNK_SIZE,
                                    resolve
                                ),
                            0
                        );
                    };

                    return new Promise((resolve) => {
                        setTimeout(() => readBlocks(0, resolve), 0);
                    });
                });
            }

            const reader = response.body.getReader();
            const streamHandler = (status) => {
                if (status.value) {
                    readBuffer(status.value);
                }

                if (!status.done) {
                    return reader.read().then(streamHandler);
                }

                return Promise.resolve();
            };

            return reader.read().then(streamHandler);
        });
}
