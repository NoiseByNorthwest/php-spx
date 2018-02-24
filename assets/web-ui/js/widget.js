import * as utils from './utils.js';
import * as fmt from './fmt.js';
import * as math from './math.js';
import * as svg from './svg.js';

function renderSVGTimeGrid(viewPort, width, height, timeRangeUs, detailed) {
    if (detailed) {
        viewPort.appendChild(svg.createNode('rect', {
            x: 0,
            y: height - 70,
            width: width,
            height: 70,
            'fill-opacity': '0.5',
        }));
    }

    const timeRangeNs = timeRangeUs.copy().scale(1000);
    const delta = timeRangeNs.length();
    let step = Math.pow(10, parseInt(Math.log10(delta)));
    if (delta / step < 4) {
        step /= 5;
    }

    step = Math.max(step, 1000);

    const minorStep = step / 5;

    let tickTime = (parseInt(timeRangeNs.a / minorStep) + 1) * minorStep;
    while (1) {
        const majorTick = tickTime % step == 0;
        const x = width * (tickTime - timeRangeNs.a) / delta;
        viewPort.appendChild(svg.createNode('line', {
            x1: x,
            y1: 0,
            x2: x,
            y2: height,
            stroke: '#777',
            'stroke-width': majorTick ? 0.5 : 0.2
        }));

        if (majorTick) {
            if (detailed) {
                const units = ['s', 'ms', 'us'];
                let t = tickTime / 1000;
                let line = 0;
                while (t > 0) {
                    const unit = units.pop();
                    const m = t % 1000;
                    t = parseInt(t / 1000);
                    if (m == 0) {
                        continue;
                    }

                    viewPort.appendChild(svg.createNode('text', {
                        x: x + 2,
                        y: height - 10 - 20 * line++,
                        width: 100,
                        height: 15,
                        'font-size': 12,
                        fill: '#ccc',
                    }, node => {
                        node.textContent = m + unit;
                    }));
                }
            } else {
                viewPort.appendChild(svg.createNode('text', {
                    x: x + 2,
                    y: height - 10,
                    width: 100,
                    height: 15,
                    'font-size': 12,
                    fill: '#aaa',
                }, node => {
                    node.textContent = fmt.time(tickTime / 1000);
                }));
            }
        }

        tickTime += minorStep;
        if (tickTime > timeRangeNs.b) {
            break;
        }
    }
}

class ViewTimeRange {

    constructor(timeRange, wallTime, viewWidth) {
        this.setTimeRange(timeRange);
        this.wallTime = wallTime;
        this.setViewWidth(viewWidth);
    }

    setTimeRange(timeRange) {
        this.timeRange = timeRange.copy();
    }

    setViewWidth(viewWidth) {
        this.viewWidth = viewWidth;
    }

    fix() {
        const minLength = 3;
        this.timeRange.bound(0, this.wallTime);
        if (this.timeRange.length() >= minLength) {
            return this;
        }

        this.timeRange.b = this.timeRange.a + minLength;
        if (this.timeRange.b > this.wallTime) {
            this.timeRange.shift(this.wallTime - this.timeRange.b);
        }

        return this;
    }

    shiftViewRange(dist) {
        this.timeRange = this._viewRangeToTimeRange(
            this.getViewRange().shift(dist)
        );

        return this.fix();
    }

    shiftScaledViewRange(dist) {
        return this.shiftViewRange(dist / this.getScale());
    }

    zoomScaledViewRange(factor, center) {
        center /= this.getScale();       // scaled
        center += this.getViewRange().a; // translated
        center /= this.viewWidth;        // view space -> norm space
        center *= this.wallTime;         // norm space -> time space

        this.timeRange.shift(-center);
        this.timeRange.scale(1 / factor);
        this.timeRange.shift(center);

        return this.fix();
    }

    getScale() {
        return this.wallTime / this.timeRange.length();
    }

    getViewRange() {
        return this._timeRangeToViewRange(this.timeRange);
    }

    getScaledViewRange() {
        return this.getViewRange().scale(this.getScale());
    }

    getTimeRange() {
        return this.timeRange.copy();
    }

    _viewRangeToTimeRange(range) {
        return range.copy().scale(this.wallTime / this.viewWidth);
    }

    _timeRangeToViewRange(range) {
        return range.copy().scale(this.viewWidth / this.wallTime);
    }
}

class Widget {

    constructor(container, profileData) {
        this.container = container;
        this.profileData = profileData;
        this.currentMetric = profileData.getMetadata().enabled_metrics[0];

        $(window).on('resize', () => {
            this._fitToContainerSize();
        });
    }

    setCurrentMetric(metric) {
        this.currentMetric = metric;
    }

    clear() {
        this.container.empty();
    }

    render() {
    }

    repaint() {
        const id = this.container.attr('id');
        console.time('repaint ' + id);
        this.clear();
        this.render();
        console.timeEnd('repaint ' + id);
    }

    _fitToContainerSize() {
        this.repaint();
    }
}

class SVGWidget extends Widget {

    constructor(container, profileData) {
        super(container, profileData);

        this.w = this.container.width();
        this.h = this.container.height();
        this.viewPort = svg.createNode('svg', {
            width: this.w,
            height: this.h,
        });

        this.container.append(this.viewPort);
    }

    clear() {
        while (this.viewPort.firstChild) {
            this.viewPort.removeChild(this.viewPort.firstChild);
        }
    }

    _fitToContainerSize() {
        this.w = this.container.width();
        this.h = this.container.height();
        this.viewPort.setAttribute('width', this.w);
        this.viewPort.setAttribute('height', this.h);
        super._fitToContainerSize();
    }
}

export class OverView extends SVGWidget {

    constructor(container, profileData) {
        super(container, profileData);
        
        this.viewTimeRange = new ViewTimeRange(
            this.profileData.getTimeRange(),
            this.profileData.getWallTime(),
            this.w
        );

        const updateTimeRangeRect = () => {
            if (!this.timeRangeRect) {
                return;
            }

            const viewRange = this.viewTimeRange.getViewRange();

            this.timeRangeRect.setAttribute('x', viewRange.a);
            this.timeRangeRect.setAttribute('width', viewRange.length());
        }

        $(window).on('spx-timerange-change', (e, timeRange) => {
            this.viewTimeRange.setTimeRange(timeRange);
            updateTimeRangeRect();
        });

        this.container.on('click mousemove', e => {
            if (e.type == 'mousemove' && e.buttons != 1) {
                return;
            }

            this.viewTimeRange.shiftViewRange(
                e.clientX - this.viewTimeRange.getViewRange().center()
            );

            $(window).trigger('spx-timerange-change', [this.viewTimeRange.getTimeRange()]);
        });
    }

    _fitToContainerSize() {
        this.viewTimeRange.setViewWidth(this.container.width());
        super._fitToContainerSize();
    }

    render() {
        this.viewPort.appendChild(svg.createNode('rect', {
            x: 0,
            y: 0,
            width: this.w,
            height: this.h,
            'fill-opacity': '0.3',
        }));

        const calls = this.profileData.getCalls(
            this.profileData.getTimeRange(),
            0.3 / this.w
        );

        for (let i = 0; i < calls.length; i++) {
            const call = calls[i];

            const x = this.w * call.getStart('wt') / this.profileData.getWallTime();
            const w = this.w * call.getInc('wt') / this.profileData.getWallTime() - 1;

            if (w < 0.3) {
                continue;
            }

            const h = 1;
            const y = call.getDepth();

            this.viewPort.appendChild(svg.createNode('line', {
                x1: x,
                y1: y,
                x2: x + w,
                y2: y + h,
                stroke: math.Vec3.lerpPath(
                    [
                        new math.Vec3(0, 0.3, 0.9),
                        new math.Vec3(0, 0.9, 0.9),
                        new math.Vec3(0, 0.9, 0),
                        new math.Vec3(0.9, 0.9, 0),
                        new math.Vec3(0.9, 0.2, 0),
                    ],
                    Math.log10(
                        call.getInc(this.currentMetric)
                            - this.profileData.getStats().getCallMin(this.currentMetric)
                    )
                        / Math.log10(
                            this.profileData.getStats().getCallMax(this.currentMetric)
                                - this.profileData.getStats().getCallMin(this.currentMetric)
                        )
                ).toHTMLColor(),
            }));
        }

        renderSVGTimeGrid(
            this.viewPort,
            this.w,
            this.h,
            this.profileData.getTimeRange()
        );

        (() => {
            /*this.viewPort.appendChild(svg.createNode('polyline', {
                points: points.join(' ')
            }));*/
        })()

        const viewRange = this.viewTimeRange.getViewRange();

        this.timeRangeRect = svg.createNode('rect', {
            x: viewRange.a,
            y: 0,
            width: viewRange.length(),
            height: this.h,
            stroke: new math.Vec3(0, 0.7, 0).toHTMLColor(),
            'stroke-width': 2,
            fill: new math.Vec3(0, 1, 0).toHTMLColor(),
            'fill-opacity': '0.1',
        });

        this.viewPort.appendChild(this.timeRangeRect);
    }
}

export class TimeLine extends SVGWidget {

    constructor(container, profileData) {
        super(container, profileData);

        this.viewTimeRange = new ViewTimeRange(
            this.profileData.getTimeRange(),
            this.profileData.getWallTime(),
            this.w
        );

        this.offsetY = 0;

        this.svgRectPool = new svg.NodePool('rect');
        this.svgTextPool = new svg.NodePool('text');

        $(window).on('spx-timerange-change', (e, timeRange) => {
            this.viewTimeRange.setTimeRange(timeRange.copy());
            this.repaint();
        });

        this.container.bind('wheel', e => {
            e.preventDefault();
            let f = 1.5;
            if (e.originalEvent.deltaY < 0) {
                f = 1 / f;
            }

            this.viewTimeRange.zoomScaledViewRange(f, e.clientX);

            $(window).trigger('spx-timerange-change', [this.viewTimeRange.getTimeRange()]);
        });

        let lastPos = {x: 0, y: 0};

        this.container.mousedown(e => {
            lastPos.x = e.clientX;
            lastPos.y = e.clientY;
        });

        this.container.mousemove(e => {
            let delta = {
                x: e.clientX - lastPos.x,
                y: e.clientY - lastPos.y,
            };

            lastPos.x = e.clientX;
            lastPos.y = e.clientY;

            switch (e.buttons) {
                case 1:
                    this.viewTimeRange.shiftScaledViewRange(-delta.x);

                    this.offsetY += delta.y;
                    this.offsetY = Math.min(0, this.offsetY);

                    break;

                case 4:
                    let f = Math.pow(1.01, Math.abs(delta.y));
                    if (delta.y < 0) {
                        f = 1 / f;
                    }

                    this.viewTimeRange.zoomScaledViewRange(f, e.clientX);

                    break;

                default:
                    return;
            }

            $(window).trigger('spx-timerange-change', [this.viewTimeRange.getTimeRange()]);
        });
    }

    _fitToContainerSize() {
        this.viewTimeRange.setViewWidth(this.container.width());
        super._fitToContainerSize();
    }

    render() {
        this.viewPort.appendChild(svg.createNode('rect', {
            x: 0,
            y: 0,
            width: this.w,
            height: this.h,
            'fill-opacity': '0.1',
        }));

        const timeRange = this.viewTimeRange.getTimeRange();
        const calls = this.profileData.getCalls(
            timeRange,
            timeRange.length() / 50000
        );

        const viewRange = this.viewTimeRange.getScaledViewRange();
        const offsetX = -viewRange.a;
        
        this.svgRectPool.releaseAll();
        this.svgTextPool.releaseAll();

        for (let i = 0; i < calls.length; i++) {
            const call = calls[i];

            let x = offsetX + this.w * call.getStart('wt') / timeRange.length();
            if (x > this.w) {
                continue;
            }

            let w = this.w * call.getInc('wt') / timeRange.length() - 1;
            if (w < 0.3 || x + w < 0) {
                continue;
            }

            w = x < 0 ? w + x : w;
            x = x < 0 ? 0 : x;
            w = Math.min(w, this.w - x);

            const h = 12;
            const y = (h + 1) * call.getDepth() + this.offsetY;

            const rect = this.svgRectPool.acquire({
                x: x,
                y: y,
                width: w,
                height: h,
                fill: math.Vec3.lerpPath(
                    [
                        new math.Vec3(0, 0.3, 0.9),
                        new math.Vec3(0, 0.9, 0.9),
                        new math.Vec3(0, 0.9, 0),
                        new math.Vec3(0.9, 0.9, 0),
                        new math.Vec3(0.9, 0.2, 0),
                    ],
                    Math.log10(
                        call.getInc(this.currentMetric)
                            - this.profileData.getStats().getCallMin(this.currentMetric)
                    )
                        / Math.log10(
                            this.profileData.getStats().getCallMax(this.currentMetric)
                                - this.profileData.getStats().getCallMin(this.currentMetric)
                        )
                ).toHTMLColor(),
            });

            this.viewPort.appendChild(rect);

            if (w > 20) {
                const text = this.svgTextPool.acquire({
                    x: x + 2,
                    y: y + (h * 0.75),
                    width: w,
                    height: h,
                    'font-size': h - 2,
                });

                text.textContent = utils.truncateFunctionName(call.getFunctionName(), w / 7);
                this.viewPort.appendChild(text);
            }
        }

        renderSVGTimeGrid(
            this.viewPort,
            this.w,
            this.h,
            timeRange,
            true
        );
    }
}

export class FlameGraph extends SVGWidget {

    constructor(container, profileData) {
        super(container, profileData);

        this.timeRange = this.profileData.getTimeRange();
        this.svgRectPool = new svg.NodePool('rect');
        this.svgTextPool = new svg.NodePool('text');

        $(window).on('spx-timerange-change', (e, timeRange) => {
            this.timeRange = timeRange.copy();
            this.repaint();
        });
    }

    render() {
        this.viewPort.appendChild(svg.createNode('rect', {
            x: 0,
            y: 0,
            width: this.w,
            height: this.h,
            'fill-opacity': '0.1',
        }));

        if (this.profileData.isReleasableMetric(this.currentMetric)) {
            this.viewPort.appendChild(svg.createNode('text', {
                x: this.w / 3,
                y: this.h / 2,
                height: 20,
                'font-size': 14,
                fill: '#089',
            }, function(node) {
                node.textContent = 'Not available for this metric';
            }));

            return;
        }

        this.svgRectPool.releaseAll();
        this.svgTextPool.releaseAll();

        const renderNode = (node, minInc, maxCumInc, x, y) => {
            x = x || 0;
            y = y || this.h;

            const w = this.w
                * (node.getInc().getValue(this.currentMetric) - minInc)
                / (maxCumInc)
                - 1
            ;

            if (w < 0.3) {
                return x;
            }

            const h = math.bound(y / (node.getDepth() + 1), 2, 12);
            y -= h + 0.5;

            let childrenX = x;
            for (let child of node.getChildren()) {
                childrenX = renderNode(child, minInc, maxCumInc, childrenX, y);
            }

            this.viewPort.appendChild(this.svgRectPool.acquire({
                x: x,
                y: y,
                width: w,
                height: h,
                fill: math.Vec3.lerp(
                    new math.Vec3(1, 0, 0),
                    new math.Vec3(1, 1, 0),
                    0.5
                        * Math.min(1, node.getDepth() / 20)
                    + 0.5
                        * (node.getInc().getValue(this.currentMetric) - minInc)
                        / (maxCumInc)
                ).toHTMLColor(),
                'fill-opacity': '1',
            }));

            if (w > 20 && h > 5) {
                const text = this.svgTextPool.acquire({
                    x: x + 2,
                    y: y + (h * 0.75),
                    width: w,
                    height: h,
                    'font-size': h - 2,
                });
            
                text.textContent = utils.truncateFunctionName(node.getFunctionName(), w / 7);
                this.viewPort.appendChild(text);
            }

            return Math.max(x + w, childrenX);
        };

        const cgRoot = this
            .profileData
            .getCallGraphStats(this.timeRange)
            .getRoot()
        ;

        const minInc = cgRoot.getMinInc().getValue(this.currentMetric);
        const maxCumInc = cgRoot.getMaxCumInc().getValue(this.currentMetric);

        let x = 0;
        for (const child of cgRoot.getChildren()) {
            x = renderNode(child, minInc, maxCumInc, x);
        }
    };
}

export class FlatProfile extends Widget {

    constructor(container, profileData) {
        super(container, profileData);

        this.timeRange = this.profileData.getTimeRange();
        this.sortCol = 'exc';
        this.sortDir = -1;

        $(window).on('spx-timerange-change', (e, timeRange) => {
            this.timeRange = timeRange.copy();
            this.repaint();
        });
    }

    render() {

        let html = `
<table width="${this.container.width() - 20}px">
<thead>
    <tr>
        <th rowspan="3" class="sortable" data-sort="name">Function</th>
        <th rowspan="3" width="80px" class="sortable" data-sort="called">Called</th>
        <th colspan="4">Wall Time</th>
    </tr>
    <tr>
        <th colspan="2">Percentage</th>
        <th colspan="2">Value</th>
    </tr>
    <tr>
        <th width="80px" class="sortable" data-sort="inc_rel">Inc.</th>
        <th width="80px" class="sortable" data-sort="exc_rel">Exc.</th>
        <th width="80px" class="sortable" data-sort="inc">Inc.</th>
        <th width="80px" class="sortable" data-sort="exc">Exc.</th>
    </tr>
</thead>
</table>
        `;

        html += `
<div style="overflow-y: auto; height: ${this.container.height() - 60}px">
<table width="${this.container.width() - 20}px"><tbody>
        `;

        let functionsStats = this.profileData.getFunctionsStats(this.timeRange);

        functionsStats.sort((a, b) => {
            switch (this.sortCol) {
                case 'name':
                    a = a.functionName;
                    b = b.functionName;

                    break;

                case 'called':
                    a = a.called;
                    b = b.called;

                    break;

                case 'inc_rel':
                case 'inc':
                    a = a.inc.getValue(this.currentMetric);
                    b = b.inc.getValue(this.currentMetric);

                    break;

                case 'exc_rel':
                case 'exc':
                default:
                    a = a.exc.getValue(this.currentMetric);
                    b = b.exc.getValue(this.currentMetric);
            }

            return (a < b ? -1 : (a > b)) * this.sortDir;
        });

        const formatter = this.profileData.getMetricFormater(this.currentMetric);
        const limit = Math.min(100, functionsStats.length);

        for (let i = 0; i < limit; i++) {
            var  stats = functionsStats[i];

            let inc = formatter(stats.inc.getValue(this.currentMetric));
            let incRel = fmt.pct(
                stats.inc.getValue(this.currentMetric) / (
                    this.profileData.getStats().getMax(this.currentMetric)
                        - this.profileData.getStats().getMin(this.currentMetric)
                )
            );

            let exc = formatter(stats.exc.getValue(this.currentMetric));
            let excRel = fmt.pct(
                stats.exc.getValue(this.currentMetric) / (
                    this.profileData.getStats().getMax(this.currentMetric)
                        - this.profileData.getStats().getMin(this.currentMetric)
                )
            );

            let funcName = stats.functionName;
            if (stats.maxCycleDepth > 0) {
                funcName += '@' + stats.maxCycleDepth;
            }

            html += `
<tr>
    <td title="${funcName}" style="text-align: left; font-size: 12px">
        ${utils.truncateFunctionName(funcName, (this.container.width() - 5 * 90) / 8)}
    </td>
    <td width="80px">${fmt.quantity(stats.called)}</td>
    <td width="80px">${incRel}</td>
    <td width="80px">${excRel}</td>
    <td width="80px">${inc}</td>
    <td width="80px">${exc}</td>
</tr>
            `;
        }

        html += '</tbody></table></div>';

        this.container.append(html);

        this.container.find('th[data-sort="' + this.sortCol + '"]').addClass('sort');

        this.container.find('th').click(e => {
            let sortCol = $(e.target).data('sort');
            if (!sortCol) {
                return;
            }

            if (this.sortCol == sortCol) {
                this.sortDir *= -1;
            }

            this.sortCol = sortCol;
            this.repaint();
        });
    }
}
