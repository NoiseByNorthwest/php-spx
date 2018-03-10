import * as utils from './utils.js';
import * as fmt from './fmt.js';
import * as math from './math.js';

class MetricValues {

    static createFromMetricsAndValue(metrics, value) {
        let values = {};
        for (let m of metrics) {
            values[m] = value;
        }

        return new MetricValues(values);
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

        return new MetricValues(values);
    }

    constructor(values) {
        this.values = values;
    }

    copy() {
        let copy = {};
        for (let i in this.values) {
            copy[i] = this.values[i];
        }

        return new MetricValues(copy);
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

        return new MetricValues(values);
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

class CallList {

    constructor(size, functionCount, metrics) {
        this.metrics = metrics;
        this.functionNames = Array(functionCount).fill("n/a");

        this.metricOffsets = {};
        for (let i = 0; i < this.metrics.length; i++) {
            this.metricOffsets[this.metrics[i]] = i;
        }

        // FIXME use float32 to save space ?
        const structure = {
            functionIdx: 'int32',
            parentIdx: 'int32',
        };

        for (let metric of this.metrics) {
            structure['start_' + metric] = 'float64';
            structure['end_'   + metric] = 'float64';
            structure['exc_'   + metric] = 'float64';
        }

        this.array = new utils.PackedRecordArray(structure, size);
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

class MetricValuesList {

    constructor(size, metrics) {
        this.metrics = metrics;

        const structure = {};
        for (let metric of this.metrics) {
            structure[metric] = 'float64';
        }

        this.array = new utils.PackedRecordArray(structure, size);
    }

    setRawMetricValuesData(idx, rawMetricValuesData) {
        const elt = {};

        for (let i = 0; i < this.metrics.length; i++) {
            const metric = this.metrics[i];

            elt[metric] = rawMetricValuesData[i];
        }

        this.array.setElement(idx, elt);
    }

    getMetricValues(time) {
        const nearestIdx = this._findNearestIdx(time);
        const nearestRawMetricValues = this.array.getElement(nearestIdx);

        if (nearestRawMetricValues['wt'] == time) {
            return new MetricValues(nearestRawMetricValues);
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

        return MetricValues.lerpByTime(
            new MetricValues(lowerRawMetricValues),
            new MetricValues(upperRawMetricValues),
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

class Stats {

    constructor(metrics) {
        this.min = MetricValues.createFromMetricsAndValue(metrics, Number.MAX_VALUE);
        this.max = MetricValues.createFromMetricsAndValue(metrics, -Number.MAX_VALUE);
        this.callMin = MetricValues.createFromMetricsAndValue(metrics, Number.MAX_VALUE);
        this.callMax = MetricValues.createFromMetricsAndValue(metrics, -Number.MAX_VALUE);
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

    constructor(callRefs, callList, lowerBound, upperBound) {
        this.functionsStats = new Map();

        callRefs = callRefs || [];
        for (let callRef of callRefs) {
            let call = callList.getCall(callRef);
            let stats = this.functionsStats.get(call.getFunctionIdx());
            if (!stats) {
                stats = {
                    functionName: call.getFunctionName(),
                    maxCycleDepth: 0,
                    called: 0,
                    inc: MetricValues.createFromMetricsAndValue(call.getMetrics(), 0),
                    exc: MetricValues.createFromMetricsAndValue(call.getMetrics(), 0),
                };

                this.functionsStats.set(call.getFunctionIdx(), stats);
            }

            stats.called++;
            let cycleDepth = call.getCycleDepth();
            stats.maxCycleDepth = Math.max(stats.maxCycleDepth, cycleDepth);
            if (cycleDepth > 0) {
                continue;
            }

            let truncated = false;

            let start = call.getMetricValues('start');
            if (lowerBound && lowerBound.getValue('wt') > start.getValue('wt')) {
                start = lowerBound;
                truncated = true;
            }

            let end = call.getMetricValues('end');
            if (upperBound && upperBound.getValue('wt') < end.getValue('wt')) {
                end = upperBound;
                truncated = true;
            }

            stats.inc.add(end.copy().sub(start));
            if (!truncated) {
                stats.exc.add(call.getMetricValues('exc'));
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

class CallGraphStatsNode {

    constructor(functionName, metrics) {
        this.functionName = functionName;
        this.parent = null;
        this.children = {};
        this.minTime = Number.MAX_VALUE;
        this.called = 0;
        this.inc = MetricValues.createFromMetricsAndValue(metrics, 0);
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

    getMaxCumInc(minInc) {
        minInc = minInc || this.getMinInc();
        let maxCumInc = null;
        for (let i in this.children) {
            if (maxCumInc == null) {
                maxCumInc = this.children[i].getMaxCumInc(minInc).copy();
            } else {
                maxCumInc.add(this.children[i].getMaxCumInc(minInc));
            }
        }

        if (maxCumInc == null) {
            maxCumInc = this.inc.copy().set(-Number.MAX_VALUE);
        }

        return maxCumInc.max(this.inc.copy().sub(minInc));
    }

    addChild(node) {
        node.parent = this;
        this.children[node.functionName] = node;

        return this;
    }

    addCallStats(start, end) {
        this.minTime = Math.min(this.minTime, start.getValue('wt'));
        this.called++;
        this.inc.add(end.copy().sub(start));

        return this;
    }

    merge(other) {
        this.called += other.called;
        this.inc.add(other.inc);
        this.minTime = Math.min(this.minTime, other.minTime);

        for (let i in other.children) {
            if (!(i in this.children)) {
                this.addChild(new CallGraphStatsNode(
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

class CallGraphStats {

    constructor(callRefs, callList, lowerBound, upperBound) {
        this.root = new CallGraphStatsNode(null, callList.getMetrics());
        this.root.called = 1;

        callRefs = callRefs || [];
        for (let callRef of callRefs) {
            const call = callList.getCall(callRef);
            const stack = call.getStack();

            let node = this.root;
            for (let i = 0; i < stack.length; i++) {
                const functionName = stack[i].getFunctionName();
                let child = node.children[functionName];
                if (!child) {
                    child = new CallGraphStatsNode(functionName, callList.getMetrics());
                    node.addChild(child);
                }

                node = child;
            }

            let start = call.getMetricValues('start');
            if (lowerBound && lowerBound.getValue('wt') > start.getValue('wt')) {
                start = lowerBound;
            }

            let end = call.getMetricValues('end');
            if (upperBound && upperBound.getValue('wt') < end.getValue('wt')) {
                end = upperBound;
            }

            node.addCallStats(start, end);
            if (node.getDepth() == 1) {
                node.getParent().getInc().add(
                    end.copy().sub(start)
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

class CallRangeTree {

    constructor(range, callList, metricValuesList) {
        this.range = range;
        this.callList = callList;
        this.metricValuesList = metricValuesList;
        this.callRefs = [];
        this.children = [];
        this.functionsStats = null;
        this.callGraphStats = null;
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

    getFunctionsStats(range, lowerBound, upperBound) {
        if (!this.range.overlaps(range)) {
            return new FunctionsStats([]);
        }

        if (this.range.isContainedBy(range)) {
            return this.functionsStats;
        }

        let callRefs = [];
        for (let callRef of this.callRefs) {
            let callTimeRange = this.callList.getCall(callRef).getTimeRange();
            if (callTimeRange.overlaps(range)) {
                callRefs.push(callRef);
            }
        }

        if (lowerBound == null && this.range.begin < range.begin) {
            lowerBound = this.metricValuesList.getMetricValues(range.begin);
        }

        if (upperBound == null && this.range.end > range.end) {
            upperBound = this.metricValuesList.getMetricValues(range.end);
        }

        let functionsStats = new FunctionsStats(
            callRefs,
            this.callList,
            lowerBound,
            upperBound
        );

        for (let child of this.children) {
            functionsStats.merge(child.getFunctionsStats(range, lowerBound, upperBound));
        }

        return functionsStats;
    }

    getCallGraphStats(range, lowerBound, upperBound) {
        if (!this.range.overlaps(range)) {
            return new CallGraphStats([], this.callList);
        }

        if (this.range.isContainedBy(range)) {
            return this.callGraphStats;
        }

        let callRefs = [];
        for (let callRef of this.callRefs) {
            let callTimeRange = this.callList.getCall(callRef).getTimeRange();
            if (callTimeRange.overlaps(range)) {
                callRefs.push(callRef);
            }
        }

        if (lowerBound == null && this.range.begin < range.begin) {
            lowerBound = this.metricValuesList.getMetricValues(range.begin);
        }

        if (upperBound == null && this.range.end > range.end) {
            upperBound = this.metricValuesList.getMetricValues(range.end);
        }

        let callGraphStats = new CallGraphStats(
            callRefs,
            this.callList,
            lowerBound,
            upperBound
        );

        for (let child of this.children) {
            callGraphStats.merge(child.getCallGraphStats(range, lowerBound, upperBound));
        }

        return callGraphStats;
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
        let tree = new CallRangeTree(range, callList, metricValuesList);

        let lRange = tree.range.subRange(0.5, 0);
        let rRange = tree.range.subRange(0.5, 1);

        let lCallRefs = [];
        let rCallRefs = [];

        if (!callRefs) {
            callRefs = Array(callList.getSize());
            for (let i = 0; i < callRefs.length; i++) {
                callRefs[i] = i;
            }
        }

        for (let callRef of callRefs) {
            let callTimeRange = callList.getCall(callRef).getTimeRange();

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

        let minCallsPerNode = 500;

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

        tree.functionsStats = new FunctionsStats(tree.callRefs, callList);
        tree.callGraphStats = new CallGraphStats(tree.callRefs, callList);

        utils.processCallChain([
            next => {
                progress(tree.callRefs.length);
                next();
            },
            next => {
                if (lCallRefs.length == 0) {
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
                        tree.callGraphStats.merge(child.callGraphStats);
                        next();
                    }
                ));
            },
            next => {
                if (rCallRefs.length == 0) {
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
                        tree.callGraphStats.merge(child.callGraphStats);
                        next();
                    }
                ));
            },
            () => {
                // prune calls < 1/150th of node range as memory / accuracy trade-off
                // FIXME /!\ this should be tunable, pruning on time basis only could broke accuracy on other metrics
                // FIXME /!\ pruning appears to cause popping noise in flamegraph view
                tree.callGraphStats.prune(range.length() / 150);
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

    getFunctionsStats(range) {
        console.time('getFunctionsStats');

        const functionsStats = this
            .callRangeTree
            .getFunctionsStats(range)
            .getValues()
        ;

        console.timeEnd('getFunctionsStats');

        return functionsStats;
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

    getCallGraphStats(range) {
        console.time('getCallGraphStats');

        const callGraphStats = this.callRangeTree.getCallGraphStats(range);

        console.timeEnd('getCallGraphStats');

        return callGraphStats;
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
            metadata.recorded_call_count,
            metadata.called_function_count,
            this.metrics
        );

        this.metricValuesList = new MetricValuesList(
            metadata.recorded_call_count * 2,
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
