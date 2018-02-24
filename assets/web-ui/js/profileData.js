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

    static M_START() { return 0; }
    static M_END()   { return 1; }
    static M_EXC()   { return 2; }

    constructor(list, idx) {
        if (idx < 0 || idx >= this.size) {
            throw new Error('Out of bound index: ' + idx);
        }

        this.list = list;
        this.elemOffset = idx * this.list.elemSize;
    }

    getFunctionId() {
        return this.list.buffer[this.elemOffset];
    }

    getFunctionName() {
        return this.list.functionNames[this.getFunctionId()];
    }

    getMetrics() {
        return this.list.metrics;
    }

    getMetricValue(type, metric) {
        return this.list.buffer[this.elemOffset + 2 + (3 * this.list.metricOffsets[metric]) + type];
    }

    getMetricValues(type) {
        let values = {};
        for (var metric of this.list.metrics) {
            values[metric] = this.getMetricValue(type, metric);
        }

        return new MetricValues(values);
    }

    getStart(metric) {
        return this.getMetricValue(this.constructor.M_START(), metric);
    }

    getEnd(metric) {
        return this.getMetricValue(this.constructor.M_END(), metric);
    }

    getInc(metric) {
        return this.getEnd(metric) - this.getStart(metric);
    }

    getExc(metric) {
        return this.getMetricValue(this.constructor.M_EXC(), metric);
    }

    getTimeRange() {
        return new math.Range(this.getStart('wt'), this.getEnd('wt'));
    }

    getParent() {
        if (this.list.buffer[this.elemOffset + 1] < 0) {
            return null;
        }

        return this.list.getCall(this.list.buffer[this.elemOffset + 1]);
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
        let parentOffset = this.list.buffer[this.elemOffset + 1] * this.list.elemSize;
        let depth = 0;
        while (parentOffset >= 0) {
            depth++;
            parentOffset = this.list.buffer[parentOffset + 1] * this.list.elemSize;
        }

        return depth;
    }

    getCycleDepth() {
        let parentOffset = this.list.buffer[this.elemOffset + 1] * this.list.elemSize;
        let cycleDepth = 0;
        while (parentOffset >= 0) {
            if (this.list.buffer[parentOffset] == this.list.buffer[this.elemOffset]) {
                cycleDepth++;
            }

            parentOffset = this.list.buffer[parentOffset + 1] * this.list.elemSize;
        }

        return cycleDepth;
    }
}

class CallList {

    constructor(size, functionCount, metrics) {
        this.size = size;
        this.metrics = metrics;
        this.functionNames = Array(functionCount).fill("n/a");

        this.metricOffsets = {};
        for (let i = 0; i < this.metrics.length; i++) {
            this.metricOffsets[this.metrics[i]] = i;
        }

        this.elemSize =
            1                     // function idx
            + 1                   // parent idx
            + 3 * metrics.length  // (start, end, exc) for each metrics
        ;

        // FIXME layout could be optimized with Int32 as type for function & parent idx
        this.buffer = new Float64Array(this.size * this.elemSize);
    }

    getSize() {
        return this.size;
    }

    getMetrics() {
        return this.metrics;
    }

    setRawCallData(idx, functionNameIdx, parentIdx, start, end, exc) {
        let elemOffset = idx * this.elemSize;

        this.buffer[elemOffset + 0] = functionNameIdx;
        this.buffer[elemOffset + 1] = parentIdx;
        
        for (let i = 0; i < this.metrics.length; i++) {
            this.buffer[elemOffset + 2 + 3 * i + 0] = start[i];
            this.buffer[elemOffset + 2 + 3 * i + 1] = end[i];
            this.buffer[elemOffset + 2 + 3 * i + 2] = exc[i];
        }

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

class Stats {

    constructor(metrics) {
        this.min = MetricValues.createFromMetricsAndValue(metrics, Number.MAX_VALUE);
        this.max = MetricValues.createFromMetricsAndValue(metrics, Number.MIN_VALUE);
        this.callMin = MetricValues.createFromMetricsAndValue(metrics, Number.MAX_VALUE);
        this.callMax = MetricValues.createFromMetricsAndValue(metrics, Number.MIN_VALUE);
    }

    getMin(metric) {
        return this.min.getValue(metric);
    }

    getMax(metric) {
        return this.max.getValue(metric);
    }

    getCallMin(metric) {
        return this.callMin.getValue(metric);
    }

    getCallMax(metric) {
        return this.callMax.getValue(metric);
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
            let stats = this.functionsStats.get(call.getFunctionId());
            if (!stats) {
                stats = {
                    functionName: call.getFunctionName(),
                    maxCycleDepth: 0,
                    called: 0,
                    inc: MetricValues.createFromMetricsAndValue(call.getMetrics(), 0),
                    exc: MetricValues.createFromMetricsAndValue(call.getMetrics(), 0),
                };

                this.functionsStats.set(call.getFunctionId(), stats);
            }

            stats.called++;
            let cycleDepth = call.getCycleDepth();
            stats.maxCycleDepth = Math.max(stats.maxCycleDepth, cycleDepth);
            if (cycleDepth > 0) {
                continue;
            }

            let truncated = false;

            let start = call.getMetricValues(CallListEntry.M_START());
            if (lowerBound && lowerBound.getValue('wt') > start.getValue('wt')) {
                start = lowerBound;
                truncated = true;
            }

            let end = call.getMetricValues(CallListEntry.M_END());
            if (upperBound && upperBound.getValue('wt') < end.getValue('wt')) {
                end = upperBound;
                truncated = true;
            }

            stats.inc.add(end.copy().sub(start));
            if (!truncated) {
                stats.exc.add(call.getMetricValues(CallListEntry.M_EXC()));
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
            maxCumInc = this.inc.copy().set(Number.MIN_VALUE);
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

            let start = call.getMetricValues(CallListEntry.M_START());
            if (lowerBound && lowerBound.getValue('wt') > start.getValue('wt')) {
                start = lowerBound;
            }

            let end = call.getMetricValues(CallListEntry.M_END());
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

    constructor(range, callList) {
        this.range = range;
        this.callList = callList;
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

    getMetricValues(time) {
        if (!this.range.contains(new math.Range(time, time))) {
            return null;
        }

        const low = this.getNearestMetricValues(time, true);
        const up = this.getNearestMetricValues(time, false);

        if (low == null) {
            return up;
        }

        if (up == null) {
            return low;
        }

        return MetricValues.lerpByTime(low, up, time);
    }

    getNearestMetricValues(time, lower) {
        if (!this.range.contains(new math.Range(time, time))) {
            return null;
        }

        let metricValuesSet = [];
        for (let callRef of this.callRefs) {
            const call = this.callList.getCall(callRef);

            const startMetricValues = call.getMetricValues(CallListEntry.M_START());
            const endMetricValues = call.getMetricValues(CallListEntry.M_END());

            for (let metricValues of [startMetricValues, endMetricValues]) {
                if (
                    (lower && metricValues.getValue('wt') <= time)
                    || (!lower && metricValues.getValue('wt') >= time)
                ) {
                    metricValuesSet.push(metricValues);
                }
            }
        }

        for (let child of this.children) {
            const metricValues = child.getNearestMetricValues(time, lower);
            if (metricValues != null) {
                metricValuesSet.push(metricValues);
            }
        }

        let nearest = null;
        for (let metricValues of metricValuesSet) {
            if (nearest == null) {
                nearest = metricValues.copy();

                continue;
            }

            if (lower) {
                nearest.max(metricValues);
            } else {
                nearest.min(metricValues);
            }
        }

        return nearest;
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

        if (lowerBound == null && this.range.a < range.a) {
            lowerBound = this.getMetricValues(range.a);
        }

        if (upperBound == null && this.range.b > range.b) {
            upperBound = this.getMetricValues(range.b);
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

        if (lowerBound == null && this.range.a < range.a) {
            lowerBound = this.getMetricValues(range.a);
        }

        if (upperBound == null && this.range.b > range.b) {
            upperBound = this.getMetricValues(range.b);
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

        if (typeof callRefs === 'undefined') {
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

    static buildAsync(range, callRefs, callList, progress, done) {
        let tree = new CallRangeTree(range, callList);

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
                    progress,
                    child => {
                        tree.functionsStats.merge(child.functionsStats);
                        tree.callGraphStats.merge(child.callGraphStats);
                        next();
                    }
                ));
            },
            () => {
                // prune calls < 1/80th as memory / accuracy trade-off
                // /!\ this should be tunable, pruning on time basis only could broke accuracy on other metrics
                // /!\ pruning appears to cause popping noise in flamegraph view
                tree.callGraphStats.prune(range.length() / 80);
                done(tree);
            }
        ], callRefs.length >= 5000, 0);

        return tree;
    }
}

class ProfileData {

    constructor(metadata, stats, callList, callRangeTree) {
        console.log('tree', callRangeTree.getMaxDepth(), callRangeTree.getNodeCount(), callList.getSize());
        this.metadata = metadata;
        this.stats = stats;
        this.callList = callList;
        this.callRangeTree = callRangeTree;
    }

    getMetadata() {
        return this.metadata;
    }

    getStats() {
        return this.stats;
    }

    getMetricFormater(metric) {
        // FIXME remove hard-code
        switch (metric) {
            case 'wt':
            case 'ct':
            case 'it':
                return fmt.time;

            case 'zm':
            case 'io':
            case 'ior':
            case 'iow':
                return fmt.memory;

            default:
                return fmt.quantity;
        }
    }

    isReleasableMetric(metric) {
        // FIXME remove hard-code
        switch (metric) {
            case 'zm':
            case 'zr':
            case 'zo':
                return true;

            default:
                return false;
        }
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
}

export class ProfileDataBuilder {

    constructor(metadata) {
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

        this.stack = [];
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
                fnId: event[0],
                parent: this.stack.length > 0 ? this.stack[this.stack.length - 1] : null,
                start: Array(this.metrics.length).fill(0),
                end: Array(this.metrics.length).fill(0),
                exc: Array(this.metrics.length).fill(0),
                children: Array(this.metrics.length).fill(0),
            });

            return;
        }

        const frame = this.stack.pop();

        for (let j = 0; j < this.metrics.length; j++) {
            const m = this.metrics[j];
            frame.start[j] = frame.startEvent[2 + j];
            frame.end[j] = event[2 + j];

            this.stats.mergeMetricValue(m, frame.start[j]);
            this.stats.mergeMetricValue(m, frame.end[j]);
            this.stats.mergeCallMetricValue(m, frame.end[j] - frame.start[j]);
        }

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
                if (this.stack[k].fnId == frame.fnId) {
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
            frame.fnId,
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
            this.metadata,
            this.stats,
            this.callList,
            this.callRangeTree
        );
    }
}
