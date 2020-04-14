/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2020 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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


import * as utils from './?SPX_UI_URI=/js/utils.js';
import * as fmt from './?SPX_UI_URI=/js/fmt.js';
import * as math from './?SPX_UI_URI=/js/math.js';

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

        const dist = (time - a.values['wt']) / (b.values['wt'] - a.values['wt']);

        let values = {};
        for (let m in a.values) {
            values[m] = math.lerp(
                a.values[m],
                b.values[m],
                dist
            );
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
        if (idx < 0 || idx >= list.getSize()) {
            throw new Error('Out of bound index: ' + idx);
        }

        this.list = list;
        this.idx = idx;
        this.elemOffset = idx * this.list.elemSize;
    }

    getIdx() {
        return this.idx;
    }

    getFunctionIdx() {
        return this.list.array.getElementFieldValue(this.idx, 'functionIdx');
    }

    getFunctionName() {
        return this.list.functionNames[this.getFunctionIdx()];
    }

    getMetrics() {
        return this.list.metrics;
    }

    getMetricValue(type, metric) {
        return this.list.array.getElementFieldValue(this.idx, type + '_' + metric);
    }

    getMetricValues(type) {
        let values = {};
        for (let metric of this.list.metrics) {
            values[metric] = this.getMetricValue(type, metric);
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
        return this.getEndMetricValues().copy().sub(this.getStartMetricValues());
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
        return new math.Range(this.getStart('wt'), this.getEnd('wt'));
    }

    getParent() {
        const parentIdx = this.list.array.getElementFieldValue(this.idx, 'parentIdx');
        if (parentIdx < 0) {
            return null;
        }

        return this.list.getCall(parentIdx);
    }

    getAncestors() {
        const ancestors = [];

        let parent = this.getParent();
        while (parent != null) {
            ancestors.push(parent);
            parent = parent.getParent();
        }

        return ancestors;
    }

    getStack() {
        const stack = this.getAncestors().reverse();
        stack.push(this);

        return stack;
    }

    getDepth() {
        let parentIdx = this.list.array.getElementFieldValue(this.idx, 'parentIdx');
        let depth = 0;
        while (parentIdx >= 0) {
            parentIdx = this.list.array.getElementFieldValue(parentIdx, 'parentIdx');
            depth++;
        }

        return depth;
    }

    getCycleDepth() {
        const functionIdx = this.getFunctionIdx();
        let parentIdx = this.list.array.getElementFieldValue(this.idx, 'parentIdx');

        let cycleDepth = 0;
        while (parentIdx >= 0) {
            const parentFunctionIdx = this.list.array.getElementFieldValue(parentIdx, 'functionIdx');
            if (functionIdx == parentFunctionIdx) {
                cycleDepth++;
            }

            parentIdx = this.list.array.getElementFieldValue(parentIdx, 'parentIdx');
        }

        return cycleDepth;
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
            this.customMetricValues['exc'] = MetricValueSet.createFromMetricsAndValue(
                this.getMetrics(),
                0
            );
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

    constructor(functionCount, metrics) {
        this.metrics = metrics;
        this.functionNames = Array(functionCount).fill("n/a");

        this.metricOffsets = {};
        for (let i = 0; i < this.metrics.length; i++) {
            this.metricOffsets[this.metrics[i]] = i;
        }

        const structure = {
            functionIdx: 'int32',
            parentIdx: 'int32',
        };

        // FIXME use float32 to save space ?
        // FIXME or add/compute some stats somewhere to find the best type (e.g. compiled
        //       file count metric could be stored as uint16)
        for (let metric of this.metrics) {
            structure['start_' + metric] = 'float64';
            structure['end_'   + metric] = 'float64';
            structure['exc_'   + metric] = 'float64';
        }

        this.array = new utils.ChunkedRecordArray(structure, 1024 * 1024);
    }

    getSize() {
        return this.array.size;
    }

    getMetrics() {
        return this.metrics;
    }

    setRawCallData(idx, functionNameIdx, parentIdx, start, end, exc) {
        const elt = {
            functionIdx: functionNameIdx,
            parentIdx: parentIdx,
        };

        for (let i = 0; i < this.metrics.length; i++) {
            const metric = this.metrics[i];

            elt['start_' + metric] = start[i];
            elt['end_'   + metric] = end[i];
            elt['exc_'   + metric] = exc[i];
        }

        this.array.setElement(idx, elt);

        return this;
    }

    getCall(idx) {
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
        return new math.Range(
            this.getMin(metric),
            this.getMax(metric)
        );
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

    constructor(metrics) {
        this.metrics = metrics;

        const structure = {};
        for (let metric of this.metrics) {
            structure[metric] = 'float64';
        }

        this.array = new utils.ChunkedRecordArray(structure, 1024 * 1024);
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

        let firstIdx = this._findNearestIdx(range.begin);
        if (firstIdx < this.array.getSize() - 1 && this.array.getElement(firstIdx)['wt'] < range.begin) {
            firstIdx++;
        }

        let lastIdx = this._findNearestIdx(range.end);
        if (lastIdx > 0 && this.array.getElement(lastIdx)['wt'] > range.end) {
            lastIdx--;
        }

        const first = this.getMetricValues(range.begin);
        const last = this.getMetricValues(range.end);

        let previous = first;
        const cumCost = new CumCostStats(previous.getMetrics());
        for (let i = firstIdx; i <= lastIdx; i++) {
            const current = new MetricValueSet(this.array.getElement(i));
            cumCost.mergeMetricValues(current.copy().sub(previous));
            previous = current;
        }

        cumCost.mergeMetricValues(last.copy().sub(previous));

        return cumCost;
    }

    getMetricValues(time) {
        const nearestIdx = this._findNearestIdx(time);
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

    _findNearestIdx(time, range) {
        range = range || new math.Range(0, this.array.getSize());

        if (range.length() == 1) {
            return range.begin;
        }

        const center = Math.floor(range.center());
        const centerTime = this.array.getElementFieldValue(center, 'wt');

        if (time < centerTime) {
            return this._findNearestIdx(time, new math.Range(range.begin, center));
        }

        if (time > centerTime) {
            return this._findNearestIdx(time, new math.Range(center, range.end));
        }

        return center;
    }
}

/*
 FIXME remove and do a dead code removal pass
*/
class Stats {

    constructor(metrics) {
        this.min = MetricValueSet.createFromMetricsAndValue(metrics, Number.MAX_VALUE);
        this.max = MetricValueSet.createFromMetricsAndValue(metrics, -Number.MAX_VALUE);
        this.callMin = MetricValueSet.createFromMetricsAndValue(metrics, Number.MAX_VALUE);
        this.callMax = MetricValueSet.createFromMetricsAndValue(metrics, -Number.MAX_VALUE);
    }

    getMin(metric) {
        return this.min.getValue(metric);
    }

    getMax(metric) {
        return this.max.getValue(metric);
    }

    getRange(metric) {
        return new math.Range(
            this.getMin(metric),
            this.getMax(metric)
        );
    }

    getCallMin(metric) {
        return this.callMin.getValue(metric);
    }

    getCallMax(metric) {
        return this.callMax.getValue(metric);
    }

    getCallRange(metric) {
        return new math.Range(
            this.getCallMin(metric),
            this.getCallMax(metric)
        );
    }

    merge(other) {
        this.min.min(other.min);
        this.max.max(other.max);
        this.callMin.min(other.callMin);
        this.callMax.max(other.callMax);

        return this;
    }

    mergeMetricValue(metric, value) {
        this.min.setValue(metric, Math.min(
            this.min.getValue(metric),
            value
        ));

        this.max.setValue(metric, Math.max(
            this.max.getValue(metric),
            value
        ));
    }

    mergeCallMetricValue(metric, value) {
        this.callMin.setValue(metric, Math.min(
            this.callMin.getValue(metric),
            value
        ));

        this.callMax.setValue(metric, Math.max(
            this.callMax.getValue(metric),
            value
        ));
    }
}

class FunctionsStats {

    constructor(calls) {
        this.functionsStats = new Map();

        calls = calls || [];
        for (let call of calls) {
            let stats = this.functionsStats.get(call.getFunctionIdx());
            if (!stats) {
                stats = {
                    functionName: call.getFunctionName(),
                    maxCycleDepth: 0,
                    called: 0,
                    inc: MetricValueSet.createFromMetricsAndValue(call.getMetrics(), 0),
                    exc: MetricValueSet.createFromMetricsAndValue(call.getMetrics(), 0),
                };

                this.functionsStats.set(call.getFunctionIdx(), stats);
            }

            stats.called++;
            let cycleDepth = call.getCycleDepth();
            stats.maxCycleDepth = Math.max(stats.maxCycleDepth, cycleDepth);
            if (cycleDepth > 0) {
                continue;
            }

            stats.inc.add(call.getIncMetricValues());
            stats.exc.add(call.getExcMetricValues());
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
                this.functionsStats.set(key, {
                    functionName: b.functionName,
                    maxCycleDepth: b.maxCycleDepth,
                    called: b.called,
                    inc: b.inc.copy(),
                    exc: b.exc.copy(),
                });

                continue;
            }

            a.called += b.called;
            a.maxCycleDepth = Math.max(a.maxCycleDepth, b.maxCycleDepth);

            a.inc.add(b.inc);
            a.exc.add(b.exc);
        }
    }
}

class CallTreeStatsNode {

    constructor(functionName, metrics) {
        this.functionName = functionName;
        this.parent = null;
        this.children = {};
        this.minTime = Number.MAX_VALUE;
        this.called = 0;
        this.inc = MetricValueSet.createFromMetricsAndValue(metrics, 0);
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
        return Object
            .keys(this.children)
            .map(k => this.children[k])
            .sort((a, b) => a.minTime - b.minTime)
        ;
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

    getMinInc() {
        const minInc = this.inc.copy();
        for (let i in this.children) {
            minInc.min(this.children[i].getMinInc());
        }

        return minInc;
    }

    getMaxCumInc() {
        const maxCumInc = this.inc.copy().set(0);
        for (const i in this.children) {
            maxCumInc.add(this.children[i].getMaxCumInc());
        }

        if (this.getChildren().length == 0) {
            maxCumInc.set(-Number.MAX_VALUE);
        }

        return maxCumInc.max(this.inc.copy());
    }

    addChild(node) {
        node.parent = this;
        this.children[node.functionName] = node;

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
        this.minTime = Math.min(this.minTime, other.minTime);

        for (let i in other.children) {
            if (!(i in this.children)) {
                this.addChild(new CallTreeStatsNode(
                    other.children[i].getFunctionName(),
                    other.children[i].getInc().getMetrics()
                ));
            }

            this.children[i].merge(other.children[i]);
        }

        return this;
    }

    prune(minDuration) {
        for (let i in this.children) {
            const child = this.children[i];

            if (child.called > 0 && child.inc.getValue('wt') < minDuration) {
                delete this.children[i];

                continue;
            }

            child.prune(minDuration);
        }

        return this;
    }
}

class CallTreeStats {

    constructor(metrics, calls) {
        this.root = new CallTreeStatsNode(null, metrics);
        this.root.called = 1;

        calls = calls || [];
        for (let call of calls) {
            const stack = call.getStack();

            let node = this.root;
            for (let i = 0; i < stack.length; i++) {
                const functionName = stack[i].getFunctionName();
                let child = node.children[functionName];
                if (!child) {
                    child = new CallTreeStatsNode(functionName, metrics);
                    node.addChild(child);
                }

                node = child;
            }

            node.addCallStats(call);
            if (node.getDepth() == 1) {
                node.getParent().getInc().add(
                    call.getIncMetricValues()
                );
            }
        }
    }

    getRoot() {
        return this.root;
    }

    merge(other) {
        this.root.merge(other.root);

        return this;
    }

    prune(minDuration) {
        this.root.prune(minDuration);

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

    merge(other) {
        this.functionsStats.merge(other.functionsStats);
        this.callTreeStats.merge(other.callTreeStats);
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
        this.functionsStats = null;
        this.callTreeStats = null;
        this.cumCostStats = null;
    }

    getNodeCount() {
        let nodeCount = 0;
        for (const child of this.children) {
            nodeCount += child.getNodeCount();
        }

        return 1 + nodeCount;
    }

    getMaxDepth() {
        let maxDepth = 0;
        for (const child of this.children) {
            maxDepth = Math.max(maxDepth, child.getMaxDepth());
        }

        return maxDepth + 1;
    }

    getTimeRangeStats(range, lowerBound, upperBound) {
        range = range || this.range;

        if (!this.range.overlaps(range)) {
            return new TimeRangeStats(
                range,
                new FunctionsStats(),
                new CallTreeStats(this.callList.getMetrics()),
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

        if (lowerBound == null && this.range.begin < range.begin) {
            lowerBound = this.metricValuesList.getMetricValues(range.begin);
        }

        if (upperBound == null && this.range.end > range.end) {
            upperBound = this.metricValuesList.getMetricValues(range.end);
        }

        const calls = [];
        for (const callRef of this.callRefs) {
            const callTimeRange = this.callList.getCall(callRef).getTimeRange();
            if (!callTimeRange.overlaps(range)) {
                continue;
            }

            calls.push(new TruncatedCallListEntry(
                this.callList.getCall(callRef),
                lowerBound,
                upperBound
            ));
        }

        const timeRangeStats = new TimeRangeStats(
            range,
            new FunctionsStats(calls),
            new CallTreeStats(this.callList.getMetrics(), calls),
            new CumCostStats(this.callList.getMetrics())
        );

        const remainingRange = this.range.copy().intersect(range);
        for (const child of this.children) {
            timeRangeStats.merge(child.getTimeRangeStats(range, lowerBound, upperBound));
            remainingRange.sub(child.range);
        }

        timeRangeStats.getCumCostStats().merge(
            this.metricValuesList.getCumCostStats(remainingRange)
        );

        return timeRangeStats;
    }

    getCallRefs(range, minDuration, callRefs) {
        if (this.range.length() < minDuration) {
            return [];
        }

        if (!this.range.overlaps(range)) {
            return [];
        }

        if (callRefs === undefined) {
            callRefs = [];
        }

        for (const callRef of this.callRefs) {
            const callTimeRange = this.callList.getCall(callRef).getTimeRange();
            if (callTimeRange.length() < minDuration) {
                // since calls are sorted
                break;
            }

            if (!callTimeRange.overlaps(range)) {
                continue;
            }

            callRefs.push(callRef);
        }

        for (const child of this.children) {
            child.getCallRefs(range, minDuration, callRefs);
        }

        return callRefs;
    }

    static buildAsync(range, callRefs, callList, metricValuesList, progress, done) {
        const tree = new CallRangeTree(range, callList, metricValuesList);

        const lRange = tree.range.subRange(0.5, 0);
        const rRange = tree.range.subRange(0.5, 1);

        let lCallRefs = [];
        let rCallRefs = [];

        if (!callRefs) {
            callRefs = Array(callList.getSize());
            for (let i = 0; i < callRefs.length; i++) {
                callRefs[i] = i;
            }
        }

        for (const callRef of callRefs) {
            const callTimeRange = callList.getCall(callRef).getTimeRange();

            if (!tree.range.contains(callTimeRange)) {
                continue;
            }

            if (lRange.contains(callTimeRange)) {
                lCallRefs.push(callRef);

                continue;
            }

            if (rRange.contains(callTimeRange)) {
                rCallRefs.push(callRef);

                continue;
            }

            tree.callRefs.push(callRef);
        }

        const minCallsPerNode = 500;

        if (lCallRefs.length < minCallsPerNode) {
            tree.callRefs = tree.callRefs.concat(lCallRefs);
            lCallRefs = [];
        }

        if (rCallRefs.length < minCallsPerNode) {
            tree.callRefs = tree.callRefs.concat(rCallRefs);
            rCallRefs = [];
        }

        tree.callRefs.sort((a, b) => {
            a = callList.getCall(a).getTimeRange().length();
            b = callList.getCall(b).getTimeRange().length();
            
            // N.B. "b - a" does not work on Chromium 62.0.3202.94 !!!

            if (a == b) {
                return 0;
            }

            return a > b ? -1 : 1;
        });

        const treeCalls = [];
        for (const callRef of tree.callRefs) {
            treeCalls.push(callList.getCall(callRef));
        }

        tree.functionsStats = new FunctionsStats(treeCalls);
        tree.callTreeStats = new CallTreeStats(callList.getMetrics(), treeCalls);
        tree.cumCostStats = new CumCostStats(callList.getMetrics());

        utils.processCallChain([
            next => {
                progress(tree.callRefs.length);
                next();
            },
            next => {
                if (lCallRefs.length == 0) {
                    tree.cumCostStats.merge(metricValuesList.getCumCostStats(lRange));
                    next();

                    return;
                }

                tree.children.push(CallRangeTree.buildAsync(
                    lRange,
                    lCallRefs,
                    callList,
                    metricValuesList,
                    progress,
                    child => {
                        tree.functionsStats.merge(child.functionsStats);
                        tree.callTreeStats.merge(child.callTreeStats);
                        tree.cumCostStats.merge(child.cumCostStats);
                        next();
                    }
                ));
            },
            next => {
                if (rCallRefs.length == 0) {
                    tree.cumCostStats.merge(metricValuesList.getCumCostStats(rRange));
                    next();

                    return;
                }

                tree.children.push(CallRangeTree.buildAsync(
                    rRange,
                    rCallRefs,
                    callList,
                    metricValuesList,
                    progress,
                    child => {
                        tree.functionsStats.merge(child.functionsStats);
                        tree.callTreeStats.merge(child.callTreeStats);
                        tree.cumCostStats.merge(child.cumCostStats);
                        next();
                    }
                ));
            },
            () => {
                // prune calls < 1/150th of node range as memory / accuracy trade-off
                // FIXME /!\ this should be tunable, pruning on time basis only could broke accuracy on other metrics
                // FIXME /!\ pruning appears to cause popping noise in flamegraph view
                tree.callTreeStats.prune(range.length() / 150);
                done(tree);
            }
        ], callRefs.length >= 5000, 0);

        return tree;
    }
}

class ProfileData {

    constructor(metricsInfo, metadata, stats, callList, metricValuesList, callRangeTree) {
        console.log('tree', callRangeTree.getMaxDepth(), callRangeTree.getNodeCount(), callList.getSize());
        this.metricsInfo = metricsInfo;
        this.metadata = metadata;
        this.stats = stats;
        this.callList = callList;
        this.metricValuesList = metricValuesList;
        this.callRangeTree = callRangeTree;
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

    getStats() {
        return this.stats;
    }

    getWallTime() {
        return this.stats.getMax('wt');
    }

    getTimeRange() {
        return new math.Range(
            0,
            this.getWallTime()
        );
    }

    getTimeRangeStats(range) {
        console.time('getTimeRangeStats');

        const timeRangeStats = this
            .callRangeTree
            .getTimeRangeStats(range)
        ;

        console.timeEnd('getTimeRangeStats');

        return timeRangeStats;
    }

    getCall(idx) {
        return this.callList.getCall(idx);
    }

    getCalls(range, minDuration) {
        console.time('getCalls');
        const callRefs = this.callRangeTree.getCallRefs(
            range,
            minDuration
        );

        let calls = [];
        for (let callRef of callRefs) {
            calls.push(this.callList.getCall(callRef));
        }

        console.timeEnd('getCalls');

        return calls;
    }

    getMetricValues(time) {
        return this.metricValuesList.getMetricValues(time);
    }
}

export class ProfileDataBuilder {

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
            this.metrics
        );

        this.metricValuesList = new MetricValuesList(
            this.metrics
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
                parent: this.stack.length > 0 ? this.stack[this.stack.length - 1] : null,
                start: Array(this.metrics.length).fill(0),
                end: Array(this.metrics.length).fill(0),
                exc: Array(this.metrics.length).fill(0),
                children: Array(this.metrics.length).fill(0),
            });

            return;
        }

        const frame = this.stack.pop();

        frame.endEventIdx = this.eventCount++;

        for (let j = 0; j < this.metrics.length; j++) {
            const m = this.metrics[j];
            frame.start[j] = frame.startEvent[2 + j];
            frame.end[j] = event[2 + j];

            this.stats.mergeMetricValue(m, frame.start[j]);
            this.stats.mergeMetricValue(m, frame.end[j]);
            this.stats.mergeCallMetricValue(m, frame.end[j] - frame.start[j]);
        }

        this.metricValuesList.setRawMetricValuesData(frame.startEventIdx, frame.start);
        this.metricValuesList.setRawMetricValuesData(frame.endEventIdx, frame.end);

        for (let j = 0; j < this.metrics.length; j++) {
            frame.exc[j] = frame.end[j] - frame.start[j];
            if (j in frame.children) {
                frame.exc[j] -= frame.children[j];
            }
        }

        if (this.stack.length > 0) {
            let parent = this.stack[this.stack.length - 1];
            for (let j = 0; j < this.metrics.length; j++) {
                parent.children[j] += frame.end[j] - frame.start[j];
            }

            for (let k = this.stack.length - 1; k >= 0; k--) {
                if (this.stack[k].fnIdx == frame.fnIdx) {
                    for (let j = 0; j < this.metrics.length; j++) {
                        this.stack[k].children[j] -= frame.exc[j];
                    }

                    break;
                }
            }
        }

        this.currentCallCount++;

        this.callList.setRawCallData(
            frame.idx,
            frame.fnIdx,
            frame.parent != null ? frame.parent.idx : -1,
            frame.start,
            frame.end,
            frame.exc
        );
    }

    setFunctionName(idx, name) {
        this.callList.setFunctionName(idx, name);
    }

    buildCallRangeTree(setProgress) {
        return new Promise(resolve => {
            let totalInserted = 0;
            console.time('Call range tree building');
            CallRangeTree.buildAsync(
                new math.Range(0, this.stats.getMax('wt')),
                null,
                this.callList,
                this.metricValuesList,
                inserted => {
                    totalInserted += inserted;
                    setProgress(totalInserted, this.callList.getSize());
                },
                callRangeTree => {
                    console.timeEnd('Call range tree building');
                    this.callRangeTree = callRangeTree;
                    resolve();
                }
            );
        });
    }

    getProfileData() {
        return new ProfileData(
            this.metricsInfo,
            this.metadata,
            this.stats,
            this.callList,
            this.metricValuesList,
            this.callRangeTree
        );
    }
}
