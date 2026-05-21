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

import * as utils from './utils.js';
import * as fmt from './fmt.js';
import * as math from './math.js';

class PackedRecordArray {
    constructor(fields, size) {
        this.typesInfo = {
            int8: {
                arrayType: Int8Array,
            },
            int16: {
                arrayType: Int16Array,
            },
            int32: {
                arrayType: Int32Array,
            },
            float32: {
                arrayType: Float32Array,
            },
            float64: {
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
            (a, b) =>
                b.typeInfo.arrayType.BYTES_PER_ELEMENT -
                a.typeInfo.arrayType.BYTES_PER_ELEMENT
        );

        const alignment = this.fields[0].typeInfo.arrayType.BYTES_PER_ELEMENT;

        this.elemSize = Math.ceil(this.elemSize / alignment) * alignment;
        this.size = size;

        if (DEBUG) {
            console.log(
                `Allocating packed array: ${this.size} x ${this.elemSize}B -> ${parseInt((this.size * this.elemSize) / (1024 * 1024))}MB`
            );
        }

        this.buffer = new ArrayBuffer(this.size * this.elemSize);

        this.fieldIndexes = {};
        let currentIdx = 0;
        let currentOffset = 0;
        for (let field of this.fields) {
            this.fieldIndexes[field.name] = currentIdx++;

            field.typeElemSize =
                this.elemSize / field.typeInfo.arrayType.BYTES_PER_ELEMENT;
            field.typeOffset =
                currentOffset / field.typeInfo.arrayType.BYTES_PER_ELEMENT;
            field.array = new field.typeInfo.arrayType(this.buffer);

            if (DEBUG) {
                console.log(
                    `Packed array field: name = ${field.name}, offset = ${currentOffset}, type BPE = ${field.typeInfo.arrayType.BYTES_PER_ELEMENT}, type offset = ${field.typeOffset}`
                );
            }

            currentOffset += field.typeInfo.arrayType.BYTES_PER_ELEMENT;
        }
    }

    getSize() {
        return this.size;
    }

    getField(fieldName) {
        if (DEBUG) {
            if (!(fieldName in this.fieldIndexes)) {
                throw new Error('Unknown field: ' + fieldName);
            }
        }

        return this.fields[this.fieldIndexes[fieldName]];
    }

    setElement(idx, obj) {
        for (let field of this.fields) {
            field.array[idx * field.typeElemSize + field.typeOffset] =
                obj[field.name] ?? 0;
        }
    }

    setElementFieldValue(idx, fieldName, fieldValue) {
        if (DEBUG) {
            if (!(fieldName in this.fieldIndexes)) {
                throw new Error('Unknown field: ' + fieldName);
            }
        }

        const field = this.fields[this.fieldIndexes[fieldName]];

        field.array[idx * field.typeElemSize + field.typeOffset] =
            fieldValue ?? 0;
    }

    getElement(idx) {
        const obj = {};
        for (const field of this.fields) {
            obj[field.name] =
                field.array[idx * field.typeElemSize + field.typeOffset];
        }

        return obj;
    }

    getElementFieldValue(idx, fieldName) {
        if (DEBUG) {
            if (!(fieldName in this.fieldIndexes)) {
                throw new Error('Unknown field: ' + fieldName);
            }
        }

        const field = this.fields[this.fieldIndexes[fieldName]];

        return field.array[idx * field.typeElemSize + field.typeOffset];
    }

    getElementFieldValueOptimized(idx, field) {
        return field.array[idx * field.typeElemSize + field.typeOffset];
    }
}

// "Structure of arrays" version of PackedRecordArray
class PackedRecordArraySoA {
    constructor(fields, size) {
        this.typesInfo = {
            int8: {
                arrayType: Int8Array,
            },
            int16: {
                arrayType: Int16Array,
            },
            int32: {
                arrayType: Int32Array,
            },
            float32: {
                arrayType: Float32Array,
            },
            float64: {
                arrayType: Float64Array,
            },
        };

        this.size = size;
        this.fields = [];
        this.fieldIndexes = {};

        let i = 0;
        for (let fieldName in fields) {
            const type = fields[fieldName];
            if (!(type in this.typesInfo)) {
                throw new Error('Unsupported type: ' + type);
            }

            const typeInfo = this.typesInfo[type];

            if (DEBUG) {
                console.log(
                    `Allocating typed array for "${fieldName}" field: ${this.size} x ${typeInfo.arrayType.BYTES_PER_ELEMENT}B` +
                        ` -> ${parseInt((this.size * typeInfo.arrayType.BYTES_PER_ELEMENT) / (1024 * 1024))}MB`
                );
            }

            this.fields.push({
                name: fieldName,
                array: new typeInfo.arrayType(this.size),
            });

            this.fieldIndexes[fieldName] = i++;
        }
    }

    getSize() {
        return this.size;
    }

    getField(fieldName) {
        if (DEBUG) {
            if (!(fieldName in this.fieldIndexes)) {
                throw new Error('Unknown field: ' + fieldName);
            }
        }

        return this.fields[this.fieldIndexes[fieldName]];
    }

    setElement(idx, obj) {
        for (const field of this.fields) {
            field.array[idx] = obj[field.name] ?? 0;
        }
    }

    setElementFieldValue(idx, fieldName, fieldValue) {
        if (DEBUG) {
            if (!(fieldName in this.fieldIndexes)) {
                throw new Error('Unknown field: ' + fieldName);
            }
        }

        const field = this.fields[this.fieldIndexes[fieldName]];

        field.array[idx] = fieldValue ?? 0;
    }

    getElement(idx) {
        let obj = {};
        for (const field of this.fields) {
            obj[field.name] = field.array[idx];
        }

        return obj;
    }

    getElementFieldValue(idx, fieldName) {
        if (DEBUG) {
            if (!(fieldName in this.fieldIndexes)) {
                throw new Error('Unknown field: ' + fieldName);
            }
        }

        const field = this.fields[this.fieldIndexes[fieldName]];

        return field.array[idx];
    }

    getElementFieldValueOptimized(idx, field) {
        return field.array[idx];
    }
}

// This pooling system is currently not used, it seems to be useless right now.
class Poolable {
    static _pool = [];
    static _region = {};
    static _currentRegionName = null;

    static acquire(...initArgs) {
        let instance;

        if (this._pool.length > 0) {
            instance = this._pool.pop();

            if (DEBUG) {
                if (typeof instance.reset !== 'function') {
                    throw new Error('Missing reset() method');
                }
            }

            instance.reset(...initArgs);
        } else {
            instance = new this();

            if (DEBUG) {
                if (typeof instance.init !== 'function') {
                    throw new Error('Missing init() method');
                }
            }

            instance.init(...initArgs);
        }

        if (this._currentRegionName !== null) {
            this._region[this._currentRegionName].push(instance);
        }

        return instance;
    }

    static withPoolRegion(regionName, func) {
        if (this._currentRegionName !== null) {
            throw new Error('There is already a current pool region');
        }

        this._currentRegionName = regionName;
        if (!(regionName in this._region)) {
            this._region[regionName] = [];
        }

        if (this._region[regionName].length > 0) {
            throw new Error('The current pool region is not empty');
        }

        try {
            return func();
        } finally {
            this._currentRegionName = null;
        }
    }

    static releaseRegion(name) {
        if (!(name in this._region)) {
            return;
        }

        this._pool.push(...this._region[name]);
        this._region[name].length = 0;
    }

    static release(instance) {
        this._pool.push(instance);
    }
}

class MetricValueSet {
    static createFromMetricsAndValue(metrics, value) {
        let values = {};
        for (let m of metrics) {
            values[m] = value;
        }

        return new MetricValueSet(values);
    }

    static lerpByTime(a, b, time) {
        if (a.values['wt'] == b.values['wt']) {
            return a.copy();
        }

        const dist =
            (time - a.values['wt']) / (b.values['wt'] - a.values['wt']);

        let values = {};
        for (let m in a.values) {
            values[m] = math.lerp(a.values[m], b.values[m], dist);
        }

        return new MetricValueSet(values);
    }

    constructor(values) {
        this.values = values;
    }

    copy() {
        let copy = {};
        for (let i in this.values) {
            copy[i] = this.values[i];
        }

        return new MetricValueSet(copy);
    }

    getMetrics() {
        return Object.keys(this.values);
    }

    getValue(metric) {
        return this.values[metric];
    }

    setValue(metric, value) {
        this.values[metric] = value;
    }

    set(value) {
        for (let i in this.values) {
            this.values[i] = value;
        }

        return this;
    }

    add(other) {
        for (let i in this.values) {
            this.values[i] += other.values[i];
        }

        return this;
    }

    sub(other) {
        for (let i in this.values) {
            this.values[i] -= other.values[i];
        }

        return this;
    }

    addPos(other) {
        for (let i in this.values) {
            if (other.values[i] > 0) {
                this.values[i] += other.values[i];
            }
        }

        return this;
    }

    addNeg(other) {
        for (let i in this.values) {
            if (other.values[i] < 0) {
                this.values[i] += other.values[i];
            }
        }

        return this;
    }

    min(other) {
        for (let i in this.values) {
            this.values[i] = Math.min(this.values[i], other.values[i]);
        }

        return this;
    }

    max(other) {
        for (let i in this.values) {
            this.values[i] = Math.max(this.values[i], other.values[i]);
        }

        return this;
    }
}

class CallListEntry {
    constructor(list, idx) {
        if (DEBUG) {
            if (idx < 0 || idx >= list.getSize()) {
                throw new Error('Out of bound index: ' + idx);
            }
        }

        this.list = list;
        this.idx = idx;
    }

    getIdx() {
        return this.idx;
    }

    getFunctionIdx() {
        if (this.functionIdx === undefined) {
            this.functionIdx = this.list.array.getElementFieldValueOptimized(
                this.idx,
                this.list.fields.functionIdx
            );
        }

        return this.functionIdx;
    }

    getFunctionName() {
        return this.list.functionNames[this.getFunctionIdx()];
    }

    getMetrics() {
        return this.list.metrics;
    }

    getMetricValue(type, metric) {
        return this.list.array.getElementFieldValueOptimized(
            this.idx,
            this.list.fields[type + '_' + metric]
        );
    }

    getMetricValues(type) {
        let values = {};
        for (let metric of this.list.metrics) {
            values[metric] = this.getMetricValue(type, metric);
        }

        return new MetricValueSet(values);
    }

    getMetricValuesDiff(statNameA, statNameB) {
        let values = {};
        for (let metric of this.list.metrics) {
            values[metric] =
                this.getMetricValue(statNameA, metric) -
                this.getMetricValue(statNameB, metric);
        }

        return new MetricValueSet(values);
    }

    getStartMetricValues() {
        return this.getMetricValues('start');
    }

    getEndMetricValues() {
        return this.getMetricValues('end');
    }

    getIncMetricValues() {
        if (this.incMetricValues === undefined) {
            this.incMetricValues = this.getMetricValuesDiff('end', 'start');
        }

        return this.incMetricValues;
    }

    getExcMetricValues() {
        return this.getMetricValues('exc');
    }

    getStart(metric) {
        return this.getMetricValue('start', metric);
    }

    getEnd(metric) {
        return this.getMetricValue('end', metric);
    }

    getInc(metric) {
        return this.getEnd(metric) - this.getStart(metric);
    }

    getExc(metric) {
        return this.getMetricValue('exc', metric);
    }

    getTimeRange() {
        if (this.timeRange === undefined) {
            this.timeRange = new math.Range(
                this.list.array.getElementFieldValueOptimized(
                    this.idx,
                    this.list.fields.start_wt
                ),
                this.list.array.getElementFieldValueOptimized(
                    this.idx,
                    this.list.fields.end_wt
                )
            );
        }

        return this.timeRange;
    }

    getStartWt() {
        return this.list.array.getElementFieldValueOptimized(
            this.idx,
            this.list.fields.start_wt
        );
    }

    getEndWt() {
        return this.list.array.getElementFieldValueOptimized(
            this.idx,
            this.list.fields.end_wt
        );
    }

    getIncWt() {
        return this.list.array.getElementFieldValueOptimized(
            this.idx,
            this.list.fields.incWt
        );
    }

    getParent() {
        if (this.parent === undefined) {
            const parentIdx = this.list.array.getElementFieldValueOptimized(
                this.idx,
                this.list.fields.parentIdx
            );

            if (parentIdx < 0) {
                this.parent = null;
            } else {
                this.parent = this.list.getCall(parentIdx);
            }
        }

        return this.parent;
    }

    getAncestors() {
        if (this.ancestors === undefined) {
            this.ancestors = [];

            let parent = this.getParent();
            while (parent != null) {
                this.ancestors.push(parent);
                parent = parent.getParent();
            }
        }

        return this.ancestors;
    }

    getFunctionNameStack() {
        if (this.functionNameStack === undefined) {
            this.functionNameStack = [];

            let currentIdx = this.idx;
            while (currentIdx >= 0) {
                this.functionNameStack.push(
                    this.list.functionNames[
                        this.list.array.getElementFieldValueOptimized(
                            currentIdx,
                            this.list.fields.functionIdx
                        )
                    ]
                );

                currentIdx = this.list.array.getElementFieldValueOptimized(
                    currentIdx,
                    this.list.fields.parentIdx
                );
            }
        }

        return this.functionNameStack;
    }

    getDepth() {
        if (this.depth === undefined) {
            this.depth = this.list.array.getElementFieldValueOptimized(
                this.idx,
                this.list.fields.depth
            );
        }

        return this.depth;
    }

    getCycleDepth() {
        if (this.cycleDepth === undefined) {
            this.cycleDepth = this.list.array.getElementFieldValueOptimized(
                this.idx,
                this.list.fields.cycleDepth
            );
        }

        return this.cycleDepth;
    }
}

class TruncatedCallListEntry extends CallListEntry {
    constructor(call, lowerBound, upperBound) {
        super(call.list, call.idx);

        this.customMetricValues = {};

        let truncated = false;

        if (lowerBound && lowerBound.getValue('wt') > this.getStart('wt')) {
            truncated = true;
            this.customMetricValues['start'] = lowerBound;
        }

        if (upperBound && upperBound.getValue('wt') < this.getEnd('wt')) {
            truncated = true;
            this.customMetricValues['end'] = upperBound;
        }

        if (truncated) {
            this.customMetricValues['exc'] =
                MetricValueSet.createFromMetricsAndValue(this.getMetrics(), 0);
        }
    }

    getMetricValue(type, metric) {
        if (type in this.customMetricValues) {
            return this.customMetricValues[type].getValue(metric);
        }

        return super.getMetricValue(type, metric);
    }
}

class CallList {
    constructor(functionCount, metrics, callCount) {
        this.metrics = metrics;
        this.functionNames = Array(functionCount).fill('n/a');

        this.metricOffsets = {};
        for (let i = 0; i < this.metrics.length; i++) {
            this.metricOffsets[this.metrics[i]] = i;
        }

        const structure = {
            functionIdx: 'int32',
            parentIdx: 'int32',
            depth: 'int16',
            cycleDepth: 'int8',
            incWt: 'float64',
        };

        // FIXME use float32 to save space ?
        // FIXME or add/compute some stats somewhere to find the best type (e.g. compiled
        //       file count metric could be stored as uint16)
        for (let metric of this.metrics) {
            const storageType = 'float64';
            structure['start_' + metric] = storageType;
            structure['end_' + metric] = storageType;
            structure['exc_' + metric] = storageType;
        }

        this.array = new PackedRecordArraySoA(structure, callCount);

        this.fields = {
            functionIdx: this.array.getField('functionIdx'),
            parentIdx: this.array.getField('parentIdx'),
            depth: this.array.getField('depth'),
            cycleDepth: this.array.getField('cycleDepth'),
            incWt: this.array.getField('incWt'),
        };

        for (const metric of this.metrics) {
            for (const statName of ['start', 'end', 'exc']) {
                const fieldName = statName + '_' + metric;
                this.fields[fieldName] = this.array.getField(fieldName);
            }
        }
    }

    getSize() {
        return this.array.size;
    }

    getMetrics() {
        return this.metrics;
    }

    setRawCallData(
        idx,
        functionNameIdx,
        parentIdx,
        depth,
        cycleDepth,
        start,
        end,
        exc
    ) {
        const elt = {
            functionIdx: functionNameIdx,
            parentIdx: parentIdx,
            depth: depth,
            cycleDepth: cycleDepth,
        };

        for (let i = 0; i < this.metrics.length; i++) {
            const metric = this.metrics[i];

            elt['start_' + metric] = start[i];
            elt['end_' + metric] = end[i];
            elt['exc_' + metric] = exc[i];

            if (metric === 'wt') {
                elt.incWt = end[i] - start[i];
            }
        }

        this.array.setElement(idx, elt);

        return this;
    }

    static #callCacheEnabled = false;
    static #callCache = new Map();

    static startCallCacheSession() {
        CallList.#callCacheEnabled = true;
        CallList.#callCache.clear();
    }

    static setCallCacheEnabled(enabled) {
        CallList.#callCacheEnabled = !!enabled;
    }

    getCall(idx) {
        if (!CallList.#callCacheEnabled) {
            return new CallListEntry(this, idx);
        }

        let call = CallList.#callCache.get(idx);
        if (call === undefined) {
            call = new CallListEntry(this, idx);
            CallList.#callCache.set(idx, call);
        }

        return call;
    }

    getCallNoCache(idx) {
        return new CallListEntry(this, idx);
    }

    setFunctionName(idx, functionName) {
        this.functionNames[idx] = functionName;

        return this;
    }
}

class CumCostStats {
    constructor(metrics) {
        this.min = MetricValueSet.createFromMetricsAndValue(metrics, 0);
        this.max = MetricValueSet.createFromMetricsAndValue(metrics, 0);
    }

    merge(other) {
        this.min.addNeg(other.min);
        this.max.addPos(other.max);
    }

    mergeMetricValues(metricValues) {
        this.min.addNeg(metricValues);
        this.max.addPos(metricValues);
    }

    getMin(metric) {
        return this.min.getValue(metric);
    }

    getMax(metric) {
        return this.max.getValue(metric);
    }

    getRange(metric) {
        return new math.Range(this.getMin(metric), this.getMax(metric));
    }

    getPosRange(metric) {
        return new math.Range(
            Math.max(0, this.getMin(metric)),
            Math.max(0, this.getMax(metric))
        );
    }

    getNegRange(metric) {
        return new math.Range(
            Math.min(0, this.getMin(metric)),
            Math.min(0, this.getMax(metric))
        );
    }
}

// fixme rename MetricValueSet -> Sample & MetricValuesList -> SampleList ?
//       keep in mind that Sample might be to concrete since MetricValueSet can also represent a cost
class MetricValuesList {
    constructor(metrics, size) {
        this.metrics = metrics;

        const structure = {};
        for (let metric of this.metrics) {
            structure[metric] = 'float64';
        }

        this.array = new PackedRecordArray(structure, size);
        this.fields = {
            wt: this.array.getField('wt'),
        };
    }

    setRawMetricValuesData(idx, rawMetricValuesData) {
        const elt = {};

        for (let i = 0; i < this.metrics.length; i++) {
            const metric = this.metrics[i];

            elt[metric] = rawMetricValuesData[i];
        }

        this.array.setElement(idx, elt);
    }

    getCumCostStats(range) {
        if (range.length() == 0) {
            return new CumCostStats(this.metrics);
        }

        let firstIdx = this.#findNearestIdx(range.start);
        if (
            firstIdx < this.array.getSize() - 1 &&
            this.array.getElement(firstIdx)['wt'] < range.start
        ) {
            firstIdx++;
        }

        let lastIdx = this.#findNearestIdx(range.end);
        if (lastIdx > 0 && this.array.getElement(lastIdx)['wt'] > range.end) {
            lastIdx--;
        }

        const first = this.getMetricValues(range.start);
        const last = this.getMetricValues(range.end);

        let previous = first;
        const cumCostStats = new CumCostStats(previous.getMetrics());
        for (let i = firstIdx; i <= lastIdx; i++) {
            const current = new MetricValueSet(this.array.getElement(i));
            cumCostStats.mergeMetricValues(current.copy().sub(previous));
            previous = current;
        }

        cumCostStats.mergeMetricValues(last.copy().sub(previous));

        return cumCostStats;
    }

    getMetricValues(time) {
        const nearestIdx = this.#findNearestIdx(time);
        const nearestRawMetricValues = this.array.getElement(nearestIdx);

        if (nearestRawMetricValues['wt'] == time) {
            return new MetricValueSet(nearestRawMetricValues);
        }

        let lowerRawMetricValues = null;
        let upperRawMetricValues = null;

        if (nearestRawMetricValues['wt'] < time) {
            lowerRawMetricValues = nearestRawMetricValues;
            upperRawMetricValues = this.array.getElement(nearestIdx + 1);
        } else {
            lowerRawMetricValues = this.array.getElement(nearestIdx - 1);
            upperRawMetricValues = nearestRawMetricValues;
        }

        return MetricValueSet.lerpByTime(
            new MetricValueSet(lowerRawMetricValues),
            new MetricValueSet(upperRawMetricValues),
            time
        );
    }

    #findNearestIdx(time, range) {
        range = range ?? new math.Range(0, this.array.getSize());

        if (range.length() == 1) {
            return range.start;
        }

        const center = Math.floor(range.center());
        const centerTime = this.array.getElementFieldValueOptimized(
            center,
            this.fields.wt
        );

        if (time < centerTime) {
            return this.#findNearestIdx(
                time,
                new math.Range(range.start, center)
            );
        }

        if (time > centerTime) {
            return this.#findNearestIdx(
                time,
                new math.Range(center, range.end)
            );
        }

        return center;
    }
}

/*
 FIXME remove and do a dead code removal pass
*/
class Stats {
    constructor(metrics) {
        this.min = MetricValueSet.createFromMetricsAndValue(
            metrics,
            Number.MAX_VALUE
        );
        this.max = MetricValueSet.createFromMetricsAndValue(
            metrics,
            -Number.MAX_VALUE
        );
        this.callMin = MetricValueSet.createFromMetricsAndValue(
            metrics,
            Number.MAX_VALUE
        );
        this.callMax = MetricValueSet.createFromMetricsAndValue(
            metrics,
            -Number.MAX_VALUE
        );
    }

    getMin(metric) {
        return this.min.getValue(metric);
    }

    getMax(metric) {
        return this.max.getValue(metric);
    }

    getRange(metric) {
        return new math.Range(this.getMin(metric), this.getMax(metric));
    }

    getCallMin(metric) {
        return this.callMin.getValue(metric);
    }

    getCallMax(metric) {
        return this.callMax.getValue(metric);
    }

    getCallRange(metric) {
        return new math.Range(this.getCallMin(metric), this.getCallMax(metric));
    }

    merge(other) {
        this.min.min(other.min);
        this.max.max(other.max);
        this.callMin.min(other.callMin);
        this.callMax.max(other.callMax);

        return this;
    }

    mergeMetricValue(metric, value) {
        this.min.setValue(metric, Math.min(this.min.getValue(metric), value));

        this.max.setValue(metric, Math.max(this.max.getValue(metric), value));
    }

    mergeCallMetricValue(metric, value) {
        this.callMin.setValue(
            metric,
            Math.min(this.callMin.getValue(metric), value)
        );

        this.callMax.setValue(
            metric,
            Math.max(this.callMax.getValue(metric), value)
        );
    }
}

class FunctionsStats {
    constructor(calls, moveable) {
        this.functionsStats = new Map();
        this.moveable = !!moveable;

        calls = calls ?? [];

        if (calls.length === 0) {
            return;
        }

        const metrics = calls[0].getMetrics();

        const initStats = (functionName) => ({
            functionName: functionName,
            maxCycleDepth: 0,
            called: 0,
            inc: MetricValueSet.createFromMetricsAndValue(metrics, 0),
            exc: MetricValueSet.createFromMetricsAndValue(metrics, 0),
        });

        for (let call of calls) {
            let stats = this.functionsStats.get(call.getFunctionIdx());
            if (!stats) {
                stats = initStats(call.getFunctionName());
                this.functionsStats.set(call.getFunctionIdx(), stats);
            }

            stats.called++;
            let cycleDepth = call.getCycleDepth();
            stats.maxCycleDepth = Math.max(stats.maxCycleDepth, cycleDepth);

            if (cycleDepth === 0) {
                stats.inc.add(call.getIncMetricValues());
            }

            stats.exc.add(call.getIncMetricValues());

            const parentCall = call.getParent();
            if (parentCall) {
                let parentStats = this.functionsStats.get(
                    parentCall.getFunctionIdx()
                );

                if (!parentStats) {
                    parentStats = initStats(parentCall.getFunctionName());
                    this.functionsStats.set(
                        parentCall.getFunctionIdx(),
                        parentStats
                    );
                }

                parentStats.exc.sub(call.getIncMetricValues());
            }
        }
    }

    getValues() {
        return Array.from(this.functionsStats.values());
    }

    merge(other) {
        for (let key of other.functionsStats.keys()) {
            let a = this.functionsStats.get(key);
            let b = other.functionsStats.get(key);

            if (!a) {
                if (other.moveable) {
                    this.functionsStats.set(key, b);
                } else {
                    this.functionsStats.set(key, {
                        functionName: b.functionName,
                        maxCycleDepth: b.maxCycleDepth,
                        called: b.called,
                        inc: b.inc.copy(),
                        exc: b.exc.copy(),
                    });
                }

                continue;
            }

            a.called += b.called;
            a.maxCycleDepth = Math.max(a.maxCycleDepth, b.maxCycleDepth);

            a.inc.add(b.inc);
            a.exc.add(b.exc);
        }
    }
}

class CallTreeStatsNode extends Poolable {
    init(functionName, metrics, moveable) {
        this.children = new Map();
        this.inc = MetricValueSet.createFromMetricsAndValue(metrics, 0);

        this.reset(functionName, metrics, moveable);
    }

    reset(functionName, metrics, moveable) {
        this.moveable = !!moveable;
        this.functionName = functionName;
        this.parent = null;
        this.children.clear();
        this.minTime = Number.MAX_VALUE;
        this.called = 0;

        const thisIncValues = this.inc.values;
        for (let i in thisIncValues) {
            thisIncValues[i] = 0;
        }

        this.childrenTime = 0;
    }

    getFunctionName() {
        return this.functionName;
    }

    getCalled() {
        return this.called;
    }

    getInc() {
        return this.inc;
    }

    getParent() {
        return this.parent;
    }

    getChildren() {
        return Array.from(this.children.values()).sort(
            (a, b) => a.minTime - b.minTime
        );
    }

    getDepth() {
        let depth = 0;
        let parent = this.getParent();
        while (parent != null) {
            depth++;
            parent = parent.getParent();
        }

        return depth;
    }

    addChild(node) {
        node.parent = this;
        this.children.set(node.functionName, node);

        return this;
    }

    addCallStats(call) {
        this.minTime = Math.min(this.minTime, call.getStart('wt'));
        this.called++;
        this.inc.add(call.getIncMetricValues());

        return this;
    }

    merge(other) {
        this.called += other.called;
        this.inc.add(other.inc);
        this.childrenTime += other.childrenTime;
        this.minTime = Math.min(this.minTime, other.minTime);

        for (const [i, otherChild] of other.children) {
            let child = this.children.get(i);
            if (child === undefined) {
                if (otherChild.moveable) {
                    this.children.set(i, otherChild);
                    otherChild.parent = this;

                    continue;
                }

                child = CallTreeStatsNode.acquire(
                    otherChild.getFunctionName(),
                    otherChild.getInc().getMetrics(),
                    this.moveable
                );

                this.addChild(child);
            }

            child.merge(otherChild);
        }

        return this;
    }

    static fastMergeStackA = new Array(4096);
    static fastMergeStackB = new Array(4096);

    fastMerge(other, minDurationThreshold) {
        const metrics = this.inc.getMetrics();

        const stackA = CallTreeStatsNode.fastMergeStackA;
        const stackB = CallTreeStatsNode.fastMergeStackB;
        let topIdx = 0;

        stackA[topIdx] = this;
        stackB[topIdx] = other;

        while (topIdx >= 0) {
            const thisNode = stackA[topIdx];
            const otherNode = stackB[topIdx];
            topIdx--;

            thisNode.called += otherNode.called;

            const thisNodeIncValues = thisNode.inc.values;
            const otherNodeIncValues = otherNode.inc.values;
            for (const i in thisNodeIncValues) {
                thisNodeIncValues[i] += otherNodeIncValues[i];
            }

            thisNode.childrenTime += otherNode.childrenTime;

            const otherMinTime = otherNode.minTime;
            if (otherMinTime < thisNode.minTime) {
                thisNode.minTime = otherMinTime;
            }

            const thisNodeChildren = thisNode.children;
            const otherChildren = otherNode.children;

            for (const entry of otherChildren) {
                const i = entry[0];
                const otherChild = entry[1];

                if (
                    minDurationThreshold !== undefined &&
                    otherChild.called > 0 &&
                    otherChild.inc.getValue('wt') < minDurationThreshold &&
                    otherChild.childrenTime < minDurationThreshold
                ) {
                    continue;
                }

                let child = thisNodeChildren.get(i);

                if (child === undefined) {
                    if (otherChild.moveable) {
                        thisNodeChildren.set(i, otherChild);
                        otherChild.parent = thisNode;

                        continue;
                    }

                    child = CallTreeStatsNode.acquire(
                        otherChild.functionName,
                        metrics,
                        thisNode.moveable
                    );

                    thisNodeChildren.set(i, child);
                    child.parent = thisNode;
                }

                topIdx++;

                if (DEBUG) {
                    if (topIdx >= stackA.length) {
                        throw new Error('fastMergeStack overflow');
                    }
                }

                stackA[topIdx] = child;
                stackB[topIdx] = otherChild;
            }
        }

        return this;
    }
}

class CallTreeStats {
    constructor(metrics, calls, moveable) {
        this.moveable = moveable === true;
        this.root = CallTreeStatsNode.acquire(null, metrics, this.moveable);
        this.root.called = 1;

        calls = calls ?? [];
        for (let call of calls) {
            const stack = call.getFunctionNameStack();
            const incTime = call.getIncMetricValues().values.wt;

            let node = this.root;
            for (let i = stack.length - 1; i >= 0; i--) {
                const functionName = stack[i];
                let child = node.children.get(functionName);
                if (!child) {
                    child = CallTreeStatsNode.acquire(
                        functionName,
                        metrics,
                        this.moveable
                    );
                    node.addChild(child);
                }

                if (i !== 0) {
                    child.childrenTime += incTime;
                }

                node = child;
            }

            node.addCallStats(call);
            if (node.getDepth() == 1) {
                node.getParent().getInc().add(call.getIncMetricValues());
            }
        }
    }

    getRoot() {
        return this.root;
    }

    merge(other, minDurationThreshold) {
        this.root.fastMerge(other.root, minDurationThreshold);

        return this;
    }
}

class TimeRangeStats {
    constructor(timeRange, functionsStats, callTreeStats, cumCostStats) {
        this.timeRange = timeRange;
        this.functionsStats = functionsStats;
        this.callTreeStats = callTreeStats;
        this.cumCostStats = cumCostStats;
    }

    merge(other, callTreeStatsMinDurationThreshold) {
        this.functionsStats.merge(other.functionsStats);
        this.callTreeStats.merge(
            other.callTreeStats,
            callTreeStatsMinDurationThreshold
        );
        this.cumCostStats.merge(other.cumCostStats);
    }

    getTimeRange() {
        return this.timeRange;
    }

    getFunctionsStats() {
        return this.functionsStats;
    }

    getCallTreeStats() {
        return this.callTreeStats;
    }

    getCumCostStats() {
        return this.cumCostStats;
    }
}

class CallRangeTree {
    constructor(range, callList, metricValuesList) {
        this.range = range;
        this.callList = callList;
        this.metricValuesList = metricValuesList;
        this.callRefs = [];
        this.children = [];
        this.maxDepth = 0;
        this.functionsStats = null;
        this.callTreeStats = null;
        this.cumCostStats = null;
    }

    computeInternalStats() {
        const callCount = this.callRefs.length;
        const childrenStats = this.children.map((e) =>
            e.computeInternalStats()
        );

        return {
            callCount: callCount,
            children: childrenStats,
        };
    }

    computeTimeRangeMaxDepth(range) {
        range = range ?? this.range;

        if (!this.range.overlaps(range)) {
            return 0;
        }

        if (this.range.isContainedBy(range)) {
            return this.maxDepth;
        }

        let maxDepth = 0;
        const rangeOverlap = range.copy().intersect(this.range);

        for (const child of this.children) {
            const childMaxDepth = child.computeTimeRangeMaxDepth(range);

            if (rangeOverlap.isContainedBy(child.range)) {
                return childMaxDepth;
            }

            if (childMaxDepth > maxDepth) {
                maxDepth = childMaxDepth;
            }
        }

        for (const callRef of this.callRefs) {
            const call = this.callList.getCallNoCache(callRef);

            const callDepth = call.getDepth();
            if (callDepth <= maxDepth) {
                continue;
            }

            if (range.end < call.getStartWt()) {
                continue;
            }

            if (call.getEndWt() < range.start) {
                continue;
            }

            maxDepth = callDepth;
        }

        return maxDepth;
    }

    computeTimeRangeStats(
        range,
        callTreeStatsMinDurationThreshold,
        lowerBound,
        upperBound
    ) {
        range = range ?? this.range;

        if (!this.range.overlaps(range)) {
            return new TimeRangeStats(
                range,
                new FunctionsStats([], true),
                new CallTreeStats(this.callList.getMetrics(), [], true),
                new CumCostStats(this.callList.getMetrics())
            );
        }

        if (this.range.isContainedBy(range)) {
            return new TimeRangeStats(
                range,
                this.functionsStats,
                this.callTreeStats,
                this.cumCostStats
            );
        }

        if (lowerBound === undefined && this.range.start < range.start) {
            lowerBound = this.metricValuesList.getMetricValues(range.start);
        }

        if (upperBound === undefined && this.range.end > range.end) {
            upperBound = this.metricValuesList.getMetricValues(range.end);
        }

        const calls = [];
        for (const callRef of this.callRefs) {
            const call = this.callList.getCall(callRef);

            const callStartWt = call.getStartWt();
            if (range.end < callStartWt) {
                continue;
            }

            const callEndWt = call.getEndWt();
            if (callEndWt < range.start) {
                continue;
            }

            const callTimeRange = new math.Range(callStartWt, callEndWt);
            if (range.contains(callTimeRange)) {
                calls.push(call);
            } else {
                calls.push(
                    new TruncatedCallListEntry(call, lowerBound, upperBound)
                );
            }
        }

        const timeRangeStats = new TimeRangeStats(
            range,
            new FunctionsStats(calls, true),
            new CallTreeStats(this.callList.getMetrics(), calls, true),
            new CumCostStats(this.callList.getMetrics())
        );

        const clampedRange = this.range.copy().intersect(range);
        let remainingStart = clampedRange.start;

        // Children must be sorted by range.start: the gap-filling logic below
        // advances remainingStart left-to-right and would double-count a child's
        // range if an earlier child in the array had a larger range.start.
        if (DEBUG) {
            for (let i = 1; i < this.children.length; i++) {
                if (
                    this.children[i].range.start <=
                    this.children[i - 1].range.start
                ) {
                    throw new Error('Children are not sorted by range.start');
                }
            }
        }

        for (const child of this.children) {
            timeRangeStats.merge(
                child.computeTimeRangeStats(
                    range,
                    callTreeStatsMinDurationThreshold,
                    lowerBound,
                    upperBound
                ),
                callTreeStatsMinDurationThreshold
            );

            if (child.range.overlaps(clampedRange)) {
                const childClampedStart = Math.max(
                    child.range.start,
                    clampedRange.start
                );
                const childClampedEnd = Math.min(
                    child.range.end,
                    clampedRange.end
                );

                if (remainingStart < childClampedStart) {
                    timeRangeStats
                        .getCumCostStats()
                        .merge(
                            this.metricValuesList.getCumCostStats(
                                new math.Range(
                                    remainingStart,
                                    childClampedStart
                                )
                            )
                        );
                }

                remainingStart = Math.max(remainingStart, childClampedEnd);
            }
        }

        if (remainingStart < clampedRange.end) {
            timeRangeStats
                .getCumCostStats()
                .merge(
                    this.metricValuesList.getCumCostStats(
                        new math.Range(remainingStart, clampedRange.end)
                    )
                );
        }

        return timeRangeStats;
    }

    computeCallTreeStats(range, minDurationThreshold, lowerBound, upperBound) {
        range = range ?? this.range;

        if (!this.range.overlaps(range)) {
            return new CallTreeStats(this.callList.getMetrics(), [], true);
        }

        if (this.range.isContainedBy(range)) {
            return this.callTreeStats;
        }

        if (lowerBound === undefined && this.range.start < range.start) {
            lowerBound = this.metricValuesList.getMetricValues(range.start);
        }

        if (upperBound === undefined && this.range.end > range.end) {
            upperBound = this.metricValuesList.getMetricValues(range.end);
        }

        const calls = [];
        for (const callRef of this.callRefs) {
            const call = this.callList.getCall(callRef);

            const callStartWt = call.getStartWt();
            if (range.end < callStartWt) {
                continue;
            }

            const callEndWt = call.getEndWt();
            if (callEndWt < range.start) {
                continue;
            }

            const callTimeRange = new math.Range(callStartWt, callEndWt);
            if (range.contains(callTimeRange)) {
                calls.push(call);
            } else {
                calls.push(
                    new TruncatedCallListEntry(call, lowerBound, upperBound)
                );
            }
        }

        const callTreeStats = new CallTreeStats(
            this.callList.getMetrics(),
            calls,
            true
        );

        for (const child of this.children) {
            callTreeStats.merge(
                child.computeCallTreeStats(
                    range,
                    minDurationThreshold,
                    lowerBound,
                    upperBound
                ),
                minDurationThreshold
            );
        }

        return callTreeStats;
    }

    getCallRefs(range, minDurationThreshold, callRefs) {
        if (this.range.length() < minDurationThreshold) {
            return [];
        }

        if (!this.range.overlaps(range)) {
            return [];
        }

        if (callRefs === undefined) {
            callRefs = [];
        }

        for (const callRef of this.callRefs) {
            const call = this.callList.getCall(callRef);

            if (call.getIncWt() < minDurationThreshold) {
                // since calls are sorted
                break;
            }

            if (range.end < call.getStartWt()) {
                continue;
            }

            if (call.getEndWt() < range.start) {
                continue;
            }

            callRefs.push(callRef);
        }

        for (const child of this.children) {
            child.getCallRefs(range, minDurationThreshold, callRefs);
        }

        return callRefs;
    }

    getCalls(range, minDurationThreshold) {
        const callRefs = this.getCallRefs(range, minDurationThreshold);

        let calls = [];
        for (let callRef of callRefs) {
            calls.push(this.callList.getCall(callRef));
        }

        return calls;
    }

    static buildAsync(
        range,
        callRefs,
        callList,
        metricValuesList,
        progress,
        done
    ) {
        const callRangeTree = new CallRangeTree(
            range,
            callList,
            metricValuesList
        );

        const childCount = 4;
        const childrenParameters = callRangeTree.range
            .subRanges(1 / childCount)
            .map((e) => ({
                range: e,
                callRefs: [],
            }));

        if (!callRefs) {
            callRefs = Array(callList.getSize());
            for (let i = 0; i < callRefs.length; i++) {
                callRefs[i] = i;
            }
        }

        for (const callRef of callRefs) {
            const call = callList.getCallNoCache(callRef);
            const callTimeRange = call.getTimeRange();
            const callDepth = call.getDepth();

            if (DEBUG) {
                if (!callRangeTree.range.contains(callTimeRange)) {
                    throw new Error('Unexpected uncontained call');
                }
            }

            if (callDepth > callRangeTree.maxDepth) {
                callRangeTree.maxDepth = callDepth;
            }

            let containedByChild = false;
            for (const childParameters of childrenParameters) {
                if (childParameters.range.contains(callTimeRange)) {
                    childParameters.callRefs.push(callRef);
                    containedByChild = true;

                    break;
                }
            }

            if (!containedByChild) {
                callRangeTree.callRefs.push(callRef);
            }
        }

        const minCallsPerNode = 2000;

        for (const childParameters of childrenParameters) {
            if (childParameters.callRefs.length < minCallsPerNode) {
                callRangeTree.callRefs = callRangeTree.callRefs.concat(
                    childParameters.callRefs
                );
                childParameters.callRefs.length = 0;
            }
        }

        callRangeTree.callRefs.sort((a, b) => {
            a = callList.getCallNoCache(a).getIncWt();
            b = callList.getCallNoCache(b).getIncWt();

            // N.B. "b - a" does not work on Chromium 62.0.3202.94 !!!

            if (a == b) {
                return 0;
            }

            return a > b ? -1 : 1;
        });

        const treeCalls = [];
        for (const callRef of callRangeTree.callRefs) {
            treeCalls.push(callList.getCallNoCache(callRef));
        }

        callRangeTree.functionsStats = new FunctionsStats(treeCalls);
        callRangeTree.callTreeStats = new CallTreeStats(
            callList.getMetrics(),
            treeCalls
        );
        callRangeTree.cumCostStats = new CumCostStats(callList.getMetrics());

        const callChain = [
            (next) => {
                progress(callRangeTree.callRefs.length);
                next();
            },
        ];

        for (const childParameters of childrenParameters) {
            callChain.push((next) => {
                if (childParameters.callRefs.length === 0) {
                    callRangeTree.cumCostStats.merge(
                        metricValuesList.getCumCostStats(childParameters.range)
                    );
                    next();

                    return;
                }

                callRangeTree.children.push(
                    CallRangeTree.buildAsync(
                        childParameters.range,
                        childParameters.callRefs,
                        callList,
                        metricValuesList,
                        progress,
                        (child) => {
                            callRangeTree.functionsStats.merge(
                                child.functionsStats
                            );
                            callRangeTree.callTreeStats.merge(
                                child.callTreeStats
                            );
                            callRangeTree.cumCostStats.merge(
                                child.cumCostStats
                            );
                            next();
                        }
                    )
                );
            });
        }

        callChain.push(() => {
            done(callRangeTree);
        });

        utils.processCallChain(callChain, callRefs.length >= 5000, 0);

        return callRangeTree;
    }
}

class TimeRangeAnalyzer {
    constructor(callRangeTree, timeRange) {
        this.callRangeTree = callRangeTree;
        this.timeRange = timeRange;
        this.levelOfDetails = 1;
    }

    getTimeRange() {
        return this.timeRange;
    }

    setTimeRange(timeRange, levelOfDetails, minRelativeDurationThreshold) {
        const timerName =
            'TimeRangeAnalyzer.setTimeRange ' + levelOfDetails.toFixed(2);
        console.time(timerName);

        this.#doSetTimeRange(
            timeRange,
            levelOfDetails,
            minRelativeDurationThreshold
        );

        console.timeEnd(timerName);
    }

    #doSetTimeRange(timeRange, levelOfDetails, minRelativeDurationThreshold) {
        levelOfDetails = levelOfDetails ?? this.levelOfDetails ?? 1;
        minRelativeDurationThreshold =
            minRelativeDurationThreshold ?? this.minRelativeDurationThreshold;

        const timeRangeChange = !(
            this.timeRange !== undefined && this.timeRange.equals(timeRange)
        );

        if (
            minRelativeDurationThreshold ===
                this.minRelativeDurationThreshold &&
            levelOfDetails === this.levelOfDetails &&
            !timeRangeChange
        ) {
            return;
        }

        this.timeRange = timeRange.copy();
        this.minRelativeDurationThreshold = minRelativeDurationThreshold;
        this.levelOfDetails = levelOfDetails;

        const minDurationThreshold =
            this.timeRange.length() * this.minRelativeDurationThreshold;

        const lodMinDurationThreshold =
            minDurationThreshold * (1 / this.levelOfDetails);

        const updateSubRangesInfo = () => {
            this.subRangesInfo = [];
            for (const subRange of timeRange.subRanges(
                1 / (150 * this.levelOfDetails)
            )) {
                this.subRangesInfo.push({
                    range: subRange,
                    maxDepth:
                        this.callRangeTree.computeTimeRangeMaxDepth(subRange),
                });
            }
        };

        if (this.significantCalls === undefined || timeRangeChange) {
            // TODO measure the performance gain
            CallList.startCallCacheSession();

            const timeRangeStats = this.callRangeTree.computeTimeRangeStats(
                timeRange,
                lodMinDurationThreshold
            );

            this.subRangesInfo = [];
            if (this.levelOfDetails === 1) {
                updateSubRangesInfo();
            }

            this.significantCalls = this.callRangeTree.getCalls(
                this.timeRange,
                minDurationThreshold
            );

            this.functionsStats = timeRangeStats.getFunctionsStats();
            this.callTreeStats = timeRangeStats.getCallTreeStats();
            this.cumCostStats = timeRangeStats.getCumCostStats();
        } else {
            // level of detail change only

            updateSubRangesInfo();

            this.callTreeStats = this.callRangeTree.computeCallTreeStats(
                timeRange,
                lodMinDurationThreshold
            );
        }
    }

    setMinRelativeDurationThreshold(minRelativeDurationThreshold) {
        this.setTimeRange(
            this.timeRange,
            this.levelOfDetails,
            minRelativeDurationThreshold
        );
    }

    setLevelOfDetails(levelOfDetails) {
        this.setTimeRange(
            this.timeRange,
            levelOfDetails,
            this.minRelativeDurationThreshold
        );
    }

    getLevelOfDetails() {
        return this.levelOfDetails;
    }

    getSubRangesInfo() {
        return this.subRangesInfo;
    }

    getSignificantCalls() {
        return this.significantCalls;
    }

    getFunctionsStats() {
        return this.functionsStats;
    }

    getCallTreeStats() {
        return this.callTreeStats;
    }

    getCumCostStats() {
        return this.cumCostStats;
    }
}

class ProfileDataAnalyzer {
    constructor(
        metricsInfo,
        metadata,
        stats,
        callList,
        metricValuesList,
        callRangeTree
    ) {
        this.metricsInfo = metricsInfo;
        this.metadata = metadata;
        this.stats = stats;
        this.callList = callList;
        this.metricValuesList = metricValuesList;
        this.callRangeTree = callRangeTree;

        this.timeRangeAnalyzer = new TimeRangeAnalyzer(
            this.callRangeTree,
            this.getTimeRange()
        );
    }

    getMetricKeys() {
        return Object.keys(this.metricsInfo);
    }

    getMetricInfo(metric) {
        for (let info of this.metricsInfo) {
            if (info.key == metric) {
                return info;
            }
        }

        throw new Error('Unknown metric: ' + key);
    }

    getMetricFormatter(metric) {
        switch (this.getMetricInfo(metric).type) {
            case 'time':
                return fmt.time;

            case 'memory':
                return fmt.memory;

            default:
                return fmt.quantity;
        }
    }

    isReleasableMetric(metric) {
        return this.getMetricInfo(metric).releasable;
    }

    getMetadata() {
        return this.metadata;
    }

    getStats() {
        return this.stats;
    }

    getWallTime() {
        return this.stats.getMax('wt');
    }

    getTimeRange() {
        return new math.Range(0, this.getWallTime());
    }

    getTimeRangeAnalyzer() {
        return this.timeRangeAnalyzer;
    }

    getCall(idx) {
        return this.callList.getCall(idx);
    }

    getMetricValues(time) {
        return this.metricValuesList.getMetricValues(time);
    }
}

export class ProfileDataAnalyzerBuilder {
    constructor(metricsInfo) {
        this.metricsInfo = metricsInfo;
    }

    setMetadata(metadata) {
        this.metadata = metadata;
        this.metrics = metadata.enabled_metrics;
        this.stats = new Stats(this.metrics);

        this.totalCallCount = metadata.recorded_call_count;
        this.currentCallCount = 0;

        this.callList = new CallList(
            metadata.called_function_count,
            this.metrics,
            metadata['call_count']
        );

        this.metricValuesList = new MetricValuesList(
            this.metrics,
            metadata['call_count'] * 2
        );

        this.stack = [];
        this.eventCount = 0;
        this.callCount = 0;
    }

    getTotalCallCount() {
        return this.totalCallCount;
    }

    getCurrentCallCount() {
        return this.currentCallCount;
    }

    addEvent(event) {
        if (event[1]) {
            this.stack.push({
                idx: this.callCount++,
                startEvent: event,
                startEventIdx: this.eventCount++,
                fnIdx: event[0],
                parent:
                    this.stack.length > 0
                        ? this.stack[this.stack.length - 1]
                        : null,
                start: Array(this.metrics.length),
                end: Array(this.metrics.length),
                exc: Array(this.metrics.length),
                children: Array(this.metrics.length).fill(0),
            });

            return;
        }

        const frame = this.stack.pop();

        frame.endEventIdx = this.eventCount++;

        for (let j = 0; j < this.metrics.length; j++) {
            const m = this.metrics[j];
            const startValue = (frame.start[j] = frame.startEvent[2 + j]);
            const endValue = (frame.end[j] = event[2 + j]);

            const incValue = endValue - startValue;

            this.stats.mergeMetricValue(m, startValue);
            this.stats.mergeMetricValue(m, endValue);
            this.stats.mergeCallMetricValue(m, incValue);

            frame.exc[j] = incValue - frame.children[j];
        }

        this.metricValuesList.setRawMetricValuesData(
            frame.startEventIdx,
            frame.start
        );

        this.metricValuesList.setRawMetricValuesData(
            frame.endEventIdx,
            frame.end
        );

        let cycleDepth = 0;
        if (this.stack.length > 0) {
            let parent = this.stack[this.stack.length - 1];
            for (let j = 0; j < this.metrics.length; j++) {
                parent.children[j] += frame.end[j] - frame.start[j];
            }

            for (let k = this.stack.length - 1; k >= 0; k--) {
                if (this.stack[k].fnIdx == frame.fnIdx) {
                    cycleDepth++;
                    if (cycleDepth === 1) {
                        for (let j = 0; j < this.metrics.length; j++) {
                            this.stack[k].children[j] -= frame.exc[j];
                        }
                    }
                }
            }
        }

        this.currentCallCount++;

        this.callList.setRawCallData(
            frame.idx,
            frame.fnIdx,
            frame.parent != null ? frame.parent.idx : -1,
            this.stack.length,
            cycleDepth,
            frame.start,
            frame.end,
            frame.exc
        );
    }

    setFunctionName(idx, name) {
        this.callList.setFunctionName(idx, name);
    }

    buildCallRangeTree(setProgress) {
        return new Promise((resolve) => {
            let totalInserted = 0;
            console.time('Call range tree building');
            CallRangeTree.buildAsync(
                new math.Range(0, this.stats.getMax('wt')),
                null,
                this.callList,
                this.metricValuesList,
                (inserted) => {
                    totalInserted += inserted;
                    setProgress(totalInserted, this.callList.getSize());
                },
                (callRangeTree) => {
                    console.timeEnd('Call range tree building');
                    this.callRangeTree = callRangeTree;

                    if (DEBUG) {
                        console.log(
                            'CallRangeTree stats',
                            callRangeTree.computeInternalStats()
                        );

                        console.time('CallRangeTree.computeTimeRangeMaxDepth');

                        for (let i = 0; i < 500; i++) {
                            for (const rangeGrowFactor of [
                                0.9, 0.8, 0.7, 0.6, 0.5,
                            ]) {
                                callRangeTree.computeTimeRangeMaxDepth(
                                    callRangeTree.range
                                        .copy()
                                        .grow(rangeGrowFactor)
                                );
                            }
                        }

                        console.timeEnd(
                            'CallRangeTree.computeTimeRangeMaxDepth'
                        );

                        console.time('CallRangeTree.computeTimeRangeStats');

                        for (let i = 0; i < 10; i++) {
                            for (const rangeGrowFactor of [
                                0.9, 0.8, 0.7, 0.6, 0.5,
                            ]) {
                                callRangeTree.computeTimeRangeStats(
                                    callRangeTree.range
                                        .copy()
                                        .grow(rangeGrowFactor)
                                );
                            }
                        }

                        console.timeEnd('CallRangeTree.computeTimeRangeStats');

                        console.time('CallRangeTree.getCallRefs');

                        for (let i = 0; i < 100; i++) {
                            for (const rangeGrowFactor of [
                                0.9, 0.8, 0.7, 0.6, 0.5,
                            ]) {
                                callRangeTree.getCallRefs(
                                    callRangeTree.range
                                        .copy()
                                        .grow(rangeGrowFactor),
                                    callRangeTree.range.length() / 5000
                                );
                            }
                        }

                        console.timeEnd('CallRangeTree.getCallRefs');
                    }

                    resolve();
                }
            );
        });
    }

    getProfileDataAnalyzer() {
        return new ProfileDataAnalyzer(
            this.metricsInfo,
            this.metadata,
            this.stats,
            this.callList,
            this.metricValuesList,
            this.callRangeTree
        );
    }
}
