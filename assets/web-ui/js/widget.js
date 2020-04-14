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
import * as svg from './?SPX_UI_URI=/js/svg.js';

function getCallMetricValueColor(profileData, metric, value) {
    const metricRange = profileData.getStats().getCallRange(metric);

    let scaleValue = 0;

    // this bounding is required since value can be lower than the lowest sample
    // (represented by metricRange.begin). It is the case when value is interpolated
    // from 2 consecutive samples
    value = Math.max(metricRange.begin, value)

    if (metricRange.length() > 100) {
        scaleValue =
            Math.log10(value - metricRange.begin)
                / Math.log10(metricRange.length())
        ;
    } else {
        scaleValue = metricRange.lerp(value);
    }

    return math.Vec3.lerpPath(
        [
            new math.Vec3(0, 0.3, 0.9),
            new math.Vec3(0, 0.9, 0.9),
            new math.Vec3(0, 0.9, 0),
            new math.Vec3(0.9, 0.9, 0),
            new math.Vec3(0.9, 0.2, 0),
        ],
        scaleValue
    ).toHTMLColor();
}

function getFunctionCategoryColor(funcName) {
    let categories = utils.getCategories(true);
    for (let category of categories) {
        for (let pattern of category.patterns) {
            if (pattern.test(funcName)) {
                pattern.lastIndex = 0;
                return `rgb(${category.color[0]},${category.color[1]},${category.color[2]})`;
            }
        }
    }
}

function renderSVGTimeGrid(viewPort, timeRange, detailed) {
    const delta = timeRange.length();
    let step = Math.pow(10, parseInt(Math.log10(delta)));
    if (delta / step < 4) {
        step /= 5;
    }

    // 5 as min value so that minor step is lower bounded to 1
    step = Math.max(step, 5);

    const minorStep = step / 5;

    let tickTime = (parseInt(timeRange.begin / minorStep) + 1) * minorStep;
    while (1) {
        const majorTick = tickTime % step == 0;
        const x = viewPort.width * (tickTime - timeRange.begin) / delta;
        viewPort.appendChildToFragment(svg.createNode('line', {
            x1: x,
            y1: 0,
            x2: x,
            y2: viewPort.height,
            stroke: '#777',
            'stroke-width': majorTick ? 0.5 : 0.2
        }));

        if (majorTick) {
            if (detailed) {
                const units = ['s', 'ms', 'us', 'ns'];
                let t = tickTime;
                let line = 0;
                while (t > 0 && units.length > 0) {
                    const unit = units.pop();
                    let m = t;
                    if (units.length > 0) {
                        m = m % 1000;
                        t = parseInt(t / 1000);
                    }

                    if (m == 0) {
                        continue;
                    }

                    viewPort.appendChildToFragment(svg.createNode('text', {
                        x: x + 2,
                        y: viewPort.height - 10 - 20 * line++,
                        width: 100,
                        height: 15,
                        'font-size': 12,
                        fill: line > 1 ? '#777' : '#ccc',
                    }, node => {
                        node.textContent = m + unit;
                    }));
                }
            } else {
                viewPort.appendChildToFragment(svg.createNode(
                    'text',
                    {
                        x: x + 2,
                        y: viewPort.height - 10,
                        width: 100,
                        height: 15,
                        'font-size': 12,
                        fill: '#aaa',
                    },
                    node => node.textContent = fmt.time(tickTime)
                ));
            }
        }

        tickTime += minorStep;
        if (tickTime > timeRange.end) {
            break;
        }
    }
}

function renderSVGMultiLineText(viewPort, lines) {
    let y = 15;

    const text = svg.createNode('text', {
        x: 0,
        y: y,
        'font-size': 12,
        fill: '#fff',
    });

    viewPort.appendChild(text);

    for (let line of lines) {
        text.appendChild(svg.createNode(
            'tspan',
            {
                x: 0,
                y: y,
            },
            node => node.textContent = line
        ));

        y += 15;
    }
}

function renderSVGMetricValuesPlot(viewPort, profileData, metric, timeRange) {
    const timeComponentMetric = ['ct', 'it'].includes(metric);
    const valueRange = timeComponentMetric ? new math.Range(0, 1) : profileData.getStats().getRange(metric);

    const step = 4;
    let previousMetricValues = null;
    let points = [];
    console.time('renderSVGMetricValuesPlot')
    for (let i = 0; i < viewPort.width; i += step) {
        const currentMetricValues = profileData.getMetricValues(
            timeRange.lerp(i / viewPort.width)
        );

        if (timeComponentMetric && previousMetricValues == null) {
            previousMetricValues = currentMetricValues;

            continue;
        }

        let currentValue = currentMetricValues.getValue(metric);
        if (timeComponentMetric) {
            currentValue = (currentMetricValues.getValue(metric) - previousMetricValues.getValue(metric))
                / (currentMetricValues.getValue('wt') - previousMetricValues.getValue('wt'))
            ;
        }

        points.push(i);
        points.push(parseInt(
            viewPort.height * (
                1 - valueRange.lerpDist(currentValue)
            )
        ));

        previousMetricValues = currentMetricValues;
    }

    console.timeEnd('renderSVGMetricValuesPlot')

    viewPort.appendChildToFragment(svg.createNode('polyline', {
        points: points.join(' '),
        stroke: '#0af',
        'stroke-width': 2,
        fill: 'none',
    }));

    const tickValueStep = valueRange.lerp(0.25);
    let tickValue = tickValueStep;
    while (tickValue < valueRange.end) {
        const y = parseInt(viewPort.height * (1 - valueRange.lerpDist(tickValue)));

        viewPort.appendChildToFragment(svg.createNode('line', {
            x1: 0,
            y1: y,
            x2: viewPort.width,
            y2: y,
            stroke: '#777',
            'stroke-width': 0.5
        }));

        viewPort.appendChildToFragment(svg.createNode('text', {
            x: 10,
            y: y - 5,
            width: 100,
            height: 15,
            'font-size': 12,
            fill: '#aaa',
        }, node => {
            const formatter = timeComponentMetric ? fmt.pct : profileData.getMetricFormatter(metric);
            node.textContent = formatter(tickValue);
        }));

        tickValue += tickValueStep;
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

        this.timeRange.end = this.timeRange.begin + minLength;
        if (this.timeRange.end > this.wallTime) {
            this.timeRange.shift(this.wallTime - this.timeRange.end);
        }

        return this;
    }

    shiftViewRange(dist) {
        this.timeRange = this._viewRangeToTimeRange(
            this.getViewRange().shift(dist)
        );

        return this.fix();
    }

    shiftViewRangeBegin(dist) {
        this.timeRange = this._viewRangeToTimeRange(
            this.getViewRange().shiftBegin(dist)
        );

        return this.fix();
    }

    shiftViewRangeEnd(dist) {
        this.timeRange = this._viewRangeToTimeRange(
            this.getViewRange().shiftEnd(dist)
        );

        return this.fix();
    }

    shiftScaledViewRange(dist) {
        return this.shiftViewRange(dist / this.getScale());
    }

    zoomScaledViewRange(factor, center) {
        center /= this.getScale();           // scaled
        center += this.getViewRange().begin; // translated
        center /= this.viewWidth;            // view space -> norm space
        center *= this.wallTime;             // norm space -> time space

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

class ViewPort {

    constructor(width, height, x, y) {
        this.width = width;
        this.height = height;
        this.x = x || 0;
        this.y = y || 0;

        this.node = svg.createNode('svg', {
            width: this.width,
            height: this.height,
            x: this.x,
            y: this.y,
        });

        this.fragment = null;
    }

    createSubViewPort(width, height, x, y) {
        const viewPort = new ViewPort(width, height, x, y);
        this.appendChild(viewPort.node);

        return viewPort;
    }

    resize(width, height) {
        this.width = width;
        this.height = height;
        this.node.setAttribute('width', this.width);
        this.node.setAttribute('height', this.height);
    }

    appendChildToFragment(child) {
        if (!this.fragment) {
            this.fragment = document.createDocumentFragment();
        }

        this.fragment.appendChild(child);
    }

    flushFragment() {
        if (!this.fragment) {
            return;
        }

        this.appendChild(this.fragment);
        this.fragment = null;
    }

    appendChild(child) {
        this.node.appendChild(child);
    }

    clear() {
        this.node.innerHTML = null;
    }
}

class Widget {

    constructor(container, profileData) {
        this.container = container;
        this.profileData = profileData;
        this.timeRange = profileData.getTimeRange();
        this.timeRangeStats = profileData.getTimeRangeStats(this.timeRange);
        this.currentMetric = profileData.getMetadata().enabled_metrics[0];
        this.repaintTimeout = null;
        this.resizingTimeouts = [];
        this.colorSchemeMode = null;
        this.highlightedFunctionName = null;
        this.functionColorResolver = (functionName, defaultColor) => {
            let color;
            switch (this.colorSchemeMode) {
                case ColorSchemeManager.MODE_CATEGORY:
                    color = getFunctionCategoryColor(functionName);
                    break;

                default:
                    color = defaultColor;
            }

            if (this.highlightedFunctionName) {
                color = math.Vec3
                    .createFromHTMLColor(color)
                    .mult(functionName == this.highlightedFunctionName ? 1.5 : 0.33)
                    .toHTMLColor()
                ;
            }

            return color;
        };

        $(window).on('resize', () => this.handleResize());

        $(window).on('spx-timerange-update', (e, timeRange, timeRangeStats) => {
            this.timeRange = timeRange;
            this.timeRangeStats = timeRangeStats;

            this.onTimeRangeUpdate();
        });

        $(window).on('spx-colorscheme-mode-update', (e, colorSchemeMode) => {
            this.colorSchemeMode = colorSchemeMode;

            this.onColorSchemeModeUpdate();
        });

        $(window).on('spx-colorscheme-category-update', () => {
            if (this.colorSchemeMode != ColorSchemeManager.MODE_CATEGORY) {
                return;
            }

            this.onColorSchemeCategoryUpdate();
        });

        $(window).on('spx-highlighted-function-update', (e, highlightedFunctionName) => {
            this.highlightedFunctionName = highlightedFunctionName;

            this.onHighlightedFunctionUpdate();
        });
    }

    onTimeRangeUpdate() {
    }

    onColorSchemeModeUpdate() {
        this.repaint();
    }

    onColorSchemeCategoryUpdate() {
        this.repaint();
    }

    onHighlightedFunctionUpdate() {
        this.repaint();
    }

    notifyTimeRangeUpdate(timeRange) {
        this.timeRange = timeRange;
        this.timeRangeStats = this.profileData.getTimeRangeStats(this.timeRange);

        $(window).trigger('spx-timerange-update', [this.timeRange, this.timeRangeStats]);
    }

    notifyColorSchemeModeUpdate(colorSchemeMode) {
        this.colorSchemeMode = colorSchemeMode;
        $(window).trigger('spx-colorscheme-mode-update', [this.colorSchemeMode]);
    }

    notifyColorSchemeCategoryUpdate() {
        $(window).trigger('spx-colorscheme-category-update');
    }

    setCurrentMetric(metric) {
        this.currentMetric = metric;
    }

    clear() {
        this.container.empty();
    }

    handleResize() {
        for (const t of this.resizingTimeouts) {
            clearTimeout(t);
        }

        this.resizingTimeouts = [];

        const handle = () => {
            this.onContainerResize();
            this.repaint();
        };

        // Several delayed handler() calls are required to both optimize responsiveness and fix
        // the appearing/disappearing scrollbar issue.

        handle();
        this.resizingTimeouts.push(setTimeout(handle, 80));
        this.resizingTimeouts.push(setTimeout(handle, 800));
        this.resizingTimeouts.push(setTimeout(handle, 1500));
    }

    onContainerResize() {
    }

    render() {
    }

    repaint() {
        if (this.repaintTimeout !== null) {
            return;
        }

        this.repaintTimeout = setTimeout(
            () => {
                this.repaintTimeout = null;

                const id = this.container.attr('id');
                console.time('repaint ' + id);
                console.time('clear ' + id);
                this.clear();
                console.timeEnd('clear ' + id);
                console.time('render ' + id);
                this.render();
                console.timeEnd('render ' + id);
                console.timeEnd('repaint ' + id);
            },
            0
        );
    }
}

export class ColorSchemeManager extends Widget {

    static get MODE_DEFAULT() { return 'default'; }
    static get MODE_CATEGORY() { return 'category'; }

    constructor(container, profileData) {
        super(container, profileData);

        // FIXME remove DOM dependencies like element ids

        this.container.html(
            `
            <span>Color scheme: </span><a href="#" id="colorscheme-current-name">default</a>
            <div id="colorscheme-panel">
                <input
                    type="radio"
                    name="colorscheme-mode"
                    id="colorscheme-mode-${ColorSchemeManager.MODE_DEFAULT}"
                    value="${ColorSchemeManager.MODE_DEFAULT}"
                    checked
                >
                <label for="colorscheme-mode-${ColorSchemeManager.MODE_DEFAULT}">
                    ${ColorSchemeManager.MODE_DEFAULT}
                </label>
                <input
                    type="radio"
                    name="colorscheme-mode"
                    id="colorscheme-mode-${ColorSchemeManager.MODE_CATEGORY}"
                    value="${ColorSchemeManager.MODE_CATEGORY}"
                >
                <label for="colorscheme-mode-${ColorSchemeManager.MODE_CATEGORY}">
                    ${ColorSchemeManager.MODE_CATEGORY}
                </label>
                <hr />
                <button id="new-category">Add new category</button>
                <ol></ol>
            </div>
            `
        );

        this.panelOpen = false;

        this.toggleLink = this.container.find('#colorscheme-current-name');
        this.panel = this.container.find('#colorscheme-panel');
        this.categoryList = this.container.find('#colorscheme-panel ol');

        this.toggleLink.on('click', e => {
            e.preventDefault();
            this.togglePanel();
        });

        $('#new-category').on('click', e => {
            e.preventDefault();
            const cats = utils.getCategories();
            cats.unshift({
                color: [90, 90, 90],
                label: 'untitled',
                patterns: []
            });

            utils.setCategories(cats);

            this.repaint();
        });

        this.container.find('input[name="colorscheme-mode"]:radio').on('change', e => {
            if (!e.target.checked) {
                return
            }

            const label = this.panel.find(`label[for="${e.target.id}"]`);
            this.toggleLink.html(label.html());

            this.notifyColorSchemeModeUpdate(e.target.value);
        });

        this.categoryList.on('input', 'textarea', e => {
            e.target.style.height = 'auto';
            e.target.style.height = e.target.scrollHeight + 'px';
        });

        const editHandler = e => {
            this.onCategoryEdit(e.target);
            e.stopPropagation();
        };

        this.categoryList.on('change', '.jscolor,input,textarea', editHandler);
        this.categoryList.on('click', 'button', editHandler);
    }

    onColorSchemeCategoryUpdate() {
    }

    clear() {

    }

    render() {
        const categories = utils.getCategories();
        const hex = n => n.toString(16).padStart(2, "0");

        const items = categories.map((cat, i) => {
            return `
<li class="category" data-index=${i}>
    <input
        name="colorpicker"
        class="jscolor"
        value="${hex(cat.color[0])}${hex(cat.color[1])}${hex(cat.color[2])}"
    >
    <input type="text" name="label" value="${cat.label}">
    <button name="push-up">⬆︎</button>
    <button name="push-down">⬇︎</button>
    <button name="del">✖</button>
    <textarea name="patterns">${cat.patterns.map(p => p.source).join('\n')}</textarea>
</li>`;
        });

        this.categoryList.html(items.join(''));
        this.categoryList.find('textarea').trigger('input');
        this.categoryList.find('.jscolor').each((i, el) => {
            el.picker = new jscolor(el, {
                width: 101,
                padding: 0,
                shadow: false,
                borderWidth: 0,
                backgroundColor: 'transparent',
                insetColor: '#000'
            });

            if (this.openPicker === i) {
                this.openPicker = null;
                el.picker.show();
            }
        });
    }

    togglePanel() {
        this.panelOpen = !this.panelOpen;
        this.panel.toggle();
        if (this.panelOpen) {
            this.repaint();
            setTimeout(() => this.listenForPanelClose(), 0);
        } else {
            this.panel.find('.jscolor').each((_, e) => e.picker.hide());
        }
    }

    listenForPanelClose() {
        const onOutsideClick = e => {
            if (
                !!e.target._jscControlName
                || e.target.closest('#colorscheme-panel') !== null
            ) {
                return;
            }

            e.preventDefault();
            off();
            this.togglePanel();
        };

        const onEscKey = e => {
            if (e.key != 'Escape') { return; }
            off();
            this.togglePanel();
        };

        const off = () => {
            $(document).off('mousedown', onOutsideClick);
            $(document).off('keydown', onEscKey);
        };

        $(document).on('mousedown', onOutsideClick);
        $(document).on('keydown', onEscKey);
    }

    onCategoryEdit(elem) {
        const idx = parseInt(elem.closest('li').dataset['index'], 10);
        const categories = utils.getCategories();

        const pushTarget = Math.max(idx-1, 0);
        switch (elem.name) {
            case 'push-down':
                pushTarget = Math.min(idx+1, categories.length-1);
            case 'push-up':
                categories.splice(pushTarget, 0, categories.splice(idx, 1)[0]);
                break;

            case 'del':
                categories.splice(idx, 1);
                break;

            case 'colorpicker':
                this.openPicker = idx;
                categories[idx].color = elem.picker.rgb.map(n => Math.floor(n));
                break;

            case 'label':
                categories[idx].label = elem.value.trim();
                break;

            case 'patterns':
                const regexes = elem.value
                    .split(/[\r\n]+/)
                    .map(line => line.trim())
                    .filter(line => line != '')
                    .map(line => new RegExp(line, 'gi'));

                categories[idx].patterns = regexes;
                break;

            default:
                throw new Error(`Unknown category prop '${elem.name}'`);
        }

        utils.setCategories(categories);
        this.repaint();
        this.notifyColorSchemeCategoryUpdate();
    }
}

class SVGWidget extends Widget {

    constructor(container, profileData) {
        super(container, profileData);

        this.viewPort = new ViewPort(
            this.container.width(),
            this.container.height()
        );

        this.container.append(this.viewPort.node);
    }

    clear() {
        this.viewPort.clear();
    }

    onContainerResize() {
        super.onContainerResize()

        // viewPort internal svg shrinking is first required to let the container get
        // its actual size.
        this.viewPort.resize(0, 0);

        this.viewPort.resize(
            this.container.width(),
            this.container.height()
        );
    }
}

export class ColorScale extends SVGWidget {

    constructor(container, profileData) {
        super(container, profileData);
    }

    onColorSchemeModeUpdate() {
        if (this.colorSchemeMode == ColorSchemeManager.MODE_DEFAULT) {
            this.container.show(() => this.repaint());
        } else {
            this.container.hide();
        }
    }

    render() {
        const step = 8;
        const exp = 5;

        const getCurrentMetricValue = x => {
            return this
                .profileData
                .getStats()
                .getCallRange(this.currentMetric)
                .lerp(
                    Math.pow(x, exp) / Math.pow(this.viewPort.width, exp)
                )
            ;
        }

        for (let i = 0; i < this.viewPort.width; i += step) {
            this.viewPort.appendChildToFragment(svg.createNode('rect', {
                x: i,
                y: 0,
                width: step,
                height: this.viewPort.height,
                fill: getCallMetricValueColor(
                    this.profileData,
                    this.currentMetric,
                    getCurrentMetricValue(i)
                ),
            }));
        }

        for (let i = 0; i < this.viewPort.width; i += step * 20) {
            this.viewPort.appendChildToFragment(svg.createNode('text', {
                x: i,
                y: this.viewPort.height - 5,
                width: 100,
                height: 15,
                'font-size': 12,
                fill: '#777',
            }, node => {
                node.textContent = this.profileData.getMetricFormatter(this.currentMetric)(
                    getCurrentMetricValue(i)
                );
            }));
        }

        this.viewPort.flushFragment();
    }
}

export class CategoryLegend extends SVGWidget {

    constructor(container, profileData) {
        super(container, profileData);
    }

    onColorSchemeModeUpdate() {
        if (this.colorSchemeMode == ColorSchemeManager.MODE_CATEGORY) {
            this.container.show(() => this.repaint());
        } else {
            this.container.hide();
        }
    }

    render() {
        let categories = utils.getCategories(true);
        let width = this.viewPort.width / categories.length;

        for (let i = 0; i < categories.length; i++) {
            let category = categories[i];
            let [r, g, b] = category.color;
            this.viewPort.appendChildToFragment(svg.createNode('rect', {
                x: width * i,
                y: 0,
                width,
                height: this.viewPort.height,
                fill: `rgb(${r},${g},${b})`,
            }));
            this.viewPort.appendChildToFragment(svg.createNode('text', {
                x: width * i + 4,
                y: 13,
                width,
                height: this.viewPort.height,
                fill: `rgb(${r},${g},${b})`,
                'font-size': 12,
                fill: '#000',
            }, node => { node.textContent = category.label }));
        }

        this.viewPort.flushFragment();
    }
}

export class OverView extends SVGWidget {

    constructor(container, profileData) {
        super(container, profileData);

        this.viewTimeRange = new ViewTimeRange(
            this.profileData.getTimeRange(),
            this.profileData.getWallTime(),
            this.viewPort.width
        );

        let action = null;
        this.container.mouseleave(e => {
            action = null;
        });

        this.container.on('mousedown mousemove', e => {
            if (e.type == 'mousemove' && e.buttons != 1) {
                if (math.dist(e.clientX, this.viewTimeRange.getViewRange().begin) < 4) {
                    this.container.css('cursor', 'e-resize');
                    action = 'move-begin';
                } else if (math.dist(e.clientX, this.viewTimeRange.getViewRange().end) < 4) {
                    this.container.css('cursor', 'w-resize');
                    action = 'move-end';
                } else {
                    this.container.css('cursor', 'pointer');
                    action = 'move';
                }

                return;
            }

            switch (action) {
                case 'move-begin':
                    this.viewTimeRange.shiftViewRangeBegin(
                        e.clientX - this.viewTimeRange.getViewRange().begin
                    );

                    break;

                case 'move-end':
                    this.viewTimeRange.shiftViewRangeEnd(
                        e.clientX - this.viewTimeRange.getViewRange().end
                    );

                    break;

                case 'move':
                    this.viewTimeRange.shiftViewRange(
                        e.clientX - this.viewTimeRange.getViewRange().center()
                    );

                    break;
            }

            this.notifyTimeRangeUpdate(this.viewTimeRange.getTimeRange());
        });
    }

    onTimeRangeUpdate() {
        this.viewTimeRange.setTimeRange(this.timeRange.copy());
        if (!this.timeRangeRect) {
            return;
        }

        const viewRange = this.viewTimeRange.getViewRange();

        this.timeRangeRect.setAttribute('x', viewRange.begin);
        this.timeRangeRect.setAttribute('width', viewRange.length());
    }

    onContainerResize() {
        super.onContainerResize();
        this.viewTimeRange.setViewWidth(this.container.width());
    }

    render() {
        this.viewPort.appendChildToFragment(svg.createNode('rect', {
            x: 0,
            y: 0,
            width: this.viewPort.width,
            height: this.viewPort.height,
            'fill-opacity': '0.3',
        }));

        const calls = this.profileData.getCalls(
            this.profileData.getTimeRange(),
            this.profileData.getTimeRange().length() / this.viewPort.width
        );

        for (let i = 0; i < calls.length; i++) {
            const call = calls[i];

            const x = this.viewPort.width * call.getStart('wt') / this.profileData.getWallTime();
            const w = this.viewPort.width * call.getInc('wt') / this.profileData.getWallTime() - 1;

            if (w < 0.3) {
                continue;
            }

            const h = 1;
            const y = call.getDepth();

            this.viewPort.appendChildToFragment(svg.createNode('line', {
                x1: x,
                y1: y,
                x2: x + w,
                y2: y + h,
                stroke: this.functionColorResolver(
                    call.getFunctionName(),
                    getCallMetricValueColor(
                        this.profileData,
                        this.currentMetric,
                        call.getInc(this.currentMetric)
                    )
                ),
            }));
        }

        renderSVGTimeGrid(
            this.viewPort,
            this.profileData.getTimeRange()
        );

        if (this.currentMetric != 'wt') {
            renderSVGMetricValuesPlot(
                this.viewPort,
                this.profileData,
                this.currentMetric,
                this.profileData.getTimeRange()
            );
        }

        const viewRange = this.viewTimeRange.getViewRange();

        this.timeRangeRect = svg.createNode('rect', {
            x: viewRange.begin,
            y: 0,
            width: viewRange.length(),
            height: this.viewPort.height,
            stroke: new math.Vec3(0, 0.7, 0).toHTMLColor(),
            'stroke-width': 2,
            fill: new math.Vec3(0, 1, 0).toHTMLColor(),
            'fill-opacity': '0.1',
        });

        this.viewPort.appendChildToFragment(this.timeRangeRect);
        this.viewPort.flushFragment();
    }
}

export class TimeLine extends SVGWidget {

    constructor(container, profileData) {
        super(container, profileData);

        this.viewTimeRange = new ViewTimeRange(
            this.profileData.getTimeRange(),
            this.profileData.getWallTime(),
            this.viewPort.width
        );

        this.offsetY = 0;

        this.svgRectPool = new svg.NodePool('rect');
        this.svgTextPool = new svg.NodePool('text');

        this.container.bind('wheel', e => {
            if (e.originalEvent.deltaY == 0) {
                return;
            }

            e.preventDefault();
            let f = 1.5;
            if (e.originalEvent.deltaY < 0) {
                f = 1 / f;
            }

            this.viewTimeRange.zoomScaledViewRange(f, e.clientX);

            this.notifyTimeRangeUpdate(this.viewTimeRange.getTimeRange());
        });

        this.infoViewPort = null;
        this.selectedCallIdx = null;

        const firstPos = {x: 0, y: 0};
        const lastPos = {x: 0, y: 0};
        let dragging = false;
        let pointedElement = null, callIdx = null, holdCallInfo = false;

        this.container.mousedown(e => {
            dragging = true;

            firstPos.x = e.clientX;
            firstPos.y = e.clientY;
            lastPos.x = e.clientX;
            lastPos.y = e.clientY;
        });

        this.container.mouseup(e => {
            dragging = false;
            if (
                firstPos.x != e.clientX
                || firstPos.y != e.clientY
            ) {
                return;
            }

            $(window).trigger(
                'spx-highlighted-function-update',
                [
                    callIdx != null ?
                        this.profileData.getCall(callIdx).getFunctionName()
                        : null
                ]
            );

            this.selectedCallIdx = callIdx;
        });

        this.container.mouseleave(e => {
            dragging = false;
        });

        this.container.mousemove(e => {
            if (e.buttons == 0) {
                dragging = false;
            }

            if (!dragging) {
                return;
            }

            const delta = {
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

            this.notifyTimeRangeUpdate(this.viewTimeRange.getTimeRange());
        });

        $(this.viewPort.node).dblclick(e => {
            if (callIdx == null) {
                return;
            }

            this.notifyTimeRangeUpdate(this.profileData.getCall(callIdx).getTimeRange());
        });

        $(this.viewPort.node).on('mousemove mouseout', e => {
            if (this.infoViewPort == null) {
                return;
            }

            if (pointedElement != null) {
                if (this.selectedCallIdx == null) {
                    pointedElement.setAttribute('stroke', 'none');
                }

                pointedElement = null;
                callIdx = null;
            }

            if (this.selectedCallIdx == null) {
                this.infoViewPort.clear();
            }

            if (e.type == 'mouseout') {
                return;
            }

            pointedElement = document.elementFromPoint(e.clientX, e.clientY);
            if (pointedElement.nodeName == 'text') {
                pointedElement = pointedElement.previousSibling;
            }

            callIdx = pointedElement.dataset.callIdx;
            if (callIdx === undefined) {
                callIdx = null;
                pointedElement = null;

                return;
            }

            if (this.selectedCallIdx != null) {
                return;
            }

            pointedElement.setAttribute('stroke', '#0ff');

            this._renderCallInfo(callIdx);
        });
    }

    onTimeRangeUpdate() {
        this.viewTimeRange.setTimeRange(this.timeRange.copy());
        this.repaint();
    }

    onContainerResize() {
        super.onContainerResize();
        this.viewTimeRange.setViewWidth(this.container.width());
    }

    onHighlightedFunctionUpdate() {
        this.selectedCallIdx = null;
        super.onHighlightedFunctionUpdate();
    }

    render() {
        this.viewPort.appendChildToFragment(svg.createNode('rect', {
            x: 0,
            y: 0,
            width: this.viewPort.width,
            height: this.viewPort.height,
            'fill-opacity': '0.1',
        }));

        const timeRange = this.viewTimeRange.getTimeRange();
        const calls = this.profileData.getCalls(
            timeRange,
            timeRange.length() / this.viewPort.width
        );

        const viewRange = this.viewTimeRange.getScaledViewRange();
        const offsetX = -viewRange.begin;
        
        this.svgRectPool.releaseAll();
        this.svgTextPool.releaseAll();

        for (let i = 0; i < calls.length; i++) {
            const call = calls[i];

            let x = offsetX + this.viewPort.width * call.getStart('wt') / timeRange.length();
            if (x > this.viewPort.width) {
                continue;
            }

            let w = this.viewPort.width * call.getInc('wt') / timeRange.length() - 1;
            if (w < 0.1 || x + w < 0) {
                continue;
            }

            w = x < 0 ? w + x : w;
            x = x < 0 ? 0 : x;
            w = Math.min(w, this.viewPort.width - x);

            const h = 12;
            const y = (h + 1) * call.getDepth() + this.offsetY;
            if (y + h < 0 || y > this.viewPort.height) {
                continue;
            }

            const rect = this.svgRectPool.acquire({
                x: x,
                y: y,
                width: w,
                height: h,
                stroke: call.getIdx() == this.selectedCallIdx ? '#0ff' : 'none',
                'stroke-width': 2,
                fill: this.functionColorResolver(
                    call.getFunctionName(),
                    getCallMetricValueColor(
                        this.profileData,
                        this.currentMetric,
                        call.getInc(this.currentMetric)
                    )
                ),
                'data-call-idx': call.getIdx(),
            });

            this.viewPort.appendChildToFragment(rect);

            if (w > 20) {
                const text = this.svgTextPool.acquire({
                    x: x + 2,
                    y: y + (h * 0.75),
                    width: w,
                    height: h,
                    'font-size': h - 2,
                });

                text.textContent = utils.truncateFunctionName(call.getFunctionName(), w / 7);
                this.viewPort.appendChildToFragment(text);
            }
        }

        renderSVGTimeGrid(
            this.viewPort,
            timeRange,
            true
        );

        this.viewPort.flushFragment();

        const overlayHeight = 100;
        const overlayViewPort = this.viewPort.createSubViewPort(
            this.viewPort.width,
            overlayHeight,
            0,
            this.viewPort.height - overlayHeight
        );

        overlayViewPort.appendChildToFragment(svg.createNode('rect', {
            x: 0,
            y: 0,
            width: overlayViewPort.width,
            height: overlayViewPort.height,
            'fill-opacity': '0.5',
        }));

        if (this.currentMetric != 'wt') {
            renderSVGMetricValuesPlot(
                overlayViewPort,
                this.profileData,
                this.currentMetric,
                timeRange
            );
        }

        overlayViewPort.flushFragment();

        this.infoViewPort = overlayViewPort.createSubViewPort(
            overlayViewPort.width,
            65,
            0,
            0
        );

        $(this.infoViewPort.node).css('cursor', 'text');
        $(this.infoViewPort.node).css('user-select', 'text');
        $(this.infoViewPort.node).on('mousedown mousemove', e => {
            e.stopPropagation();
        });

        if (this.selectedCallIdx != null) {
            this._renderCallInfo(this.selectedCallIdx);
        }
    }

    _renderCallInfo(callIdx) {
        const call = this.profileData.getCall(callIdx);
        const currentMetricName = this.profileData.getMetricInfo(this.currentMetric).name;
        const formatter = this.profileData.getMetricFormatter(this.currentMetric);

        renderSVGMultiLineText(
            this.infoViewPort.createSubViewPort(
                this.infoViewPort.width - 5,
                this.infoViewPort.height,
                5,
                0
            ),
            [
                'Function: ' + call.getFunctionName(),
                'Depth: ' + call.getDepth(),
                currentMetricName + ' inc.: ' + formatter(call.getInc(this.currentMetric)),
                currentMetricName + ' exc.: ' + formatter(call.getExc(this.currentMetric)),
            ]
        );
    }
}

export class FlameGraph extends SVGWidget {

    constructor(container, profileData) {
        super(container, profileData);

        this.svgRectPool = new svg.NodePool('rect');
        this.svgTextPool = new svg.NodePool('text');

        this.pointedElement = null;
        this.renderedCgNodes = [];
        this.infoViewPort = null;

        this.viewPort.node.addEventListener('mouseout', e => {
            if (this.pointedElement != null) {
                this.pointedElement.setAttribute('stroke', 'none');
                this.pointedElement = null;
            }

            this.infoViewPort.clear();
        });

        this.viewPort.node.addEventListener('mousemove', e => {
            if (this.pointedElement != null) {
                this.pointedElement.setAttribute('stroke', 'none');
                this.pointedElement = null;
            }

            this.infoViewPort.clear();

            this.pointedElement = document.elementFromPoint(e.clientX, e.clientY);
            if (this.pointedElement.nodeName == 'text') {
                this.pointedElement = this.pointedElement.previousSibling;
            }

            const cgNodeIdx = this.pointedElement.dataset.cgNodeIdx;
            if (cgNodeIdx === undefined) {
                this.pointedElement = null;

                return;
            }

            this.pointedElement.setAttribute('stroke', '#0ff');

            this.infoViewPort.appendChild(svg.createNode('rect', {
                x: 0,
                y: 0,
                width: this.infoViewPort.width,
                height: this.infoViewPort.height,
                'fill-opacity': '0.5',
            }));

            const cgNode = this.renderedCgNodes[cgNodeIdx];
            const currentMetricName = this.profileData.getMetricInfo(this.currentMetric).name;
            const formatter = this.profileData.getMetricFormatter(this.currentMetric);

            renderSVGMultiLineText(
                this.infoViewPort.createSubViewPort(
                    this.infoViewPort.width - 5,
                    this.infoViewPort.height,
                    5,
                    0
                ),
                [
                    'Function: ' + cgNode.getFunctionName(),
                    'Depth: ' + cgNode.getDepth(),
                    'Called: ' + cgNode.getCalled(),
                    currentMetricName + ' inc.: ' + formatter(cgNode.getInc().getValue(this.currentMetric)),
                ]
            );
        });

        this.viewPort.node.addEventListener('click', e => {
            $(window).trigger(
                'spx-highlighted-function-update',
                [
                    this.pointedElement != null ?
                        this.renderedCgNodes[this.pointedElement.dataset.cgNodeIdx].getFunctionName()
                        : null
                ]
            );
        });
    }

    onTimeRangeUpdate() {
        this.repaint();
    }

    render() {
        this.viewPort.appendChild(svg.createNode('rect', {
            x: 0,
            y: 0,
            width: this.viewPort.width,
            height: this.viewPort.height,
            'fill-opacity': '0.1',
        }));

        if (this.profileData.isReleasableMetric(this.currentMetric)) {
            this.viewPort.appendChild(svg.createNode('text', {
                x: this.viewPort.width / 4,
                y: this.viewPort.height / 2,
                height: 20,
                'font-size': 14,
                fill: '#089',
            }, function(node) {
                node.textContent = 'This visualization is not available for this metric.';
            }));

            return;
        }

        this.svgRectPool.releaseAll();
        this.svgTextPool.releaseAll();

        this.renderedCgNodes = [];

        const renderNode = (node, maxCumInc, x, y) => {
            x = x || 0;
            y = y || this.viewPort.height;

            const w = this.viewPort.width
                * node.getInc().getValue(this.currentMetric)
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
                childrenX = renderNode(child, maxCumInc, childrenX, y);
            }

            this.renderedCgNodes.push(node);
            const nodeIdx = this.renderedCgNodes.length - 1;

            this.viewPort.appendChildToFragment(this.svgRectPool.acquire({
                x: x,
                y: y,
                width: w,
                height: h,
                stroke: 'none',
                'stroke-width': 2,
                fill: this.functionColorResolver(
                    node.getFunctionName(),
                    math.Vec3.lerp(
                        new math.Vec3(1, 0, 0),
                        new math.Vec3(1, 1, 0),
                        0.5
                            * Math.min(1, node.getDepth() / 20)
                        + 0.5
                            * node.getInc().getValue(this.currentMetric)
                            / (maxCumInc)
                    ).toHTMLColor()
                ),
                'fill-opacity': '1',
                'data-cg-node-idx': nodeIdx,
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
                this.viewPort.appendChildToFragment(text);
            }

            return Math.max(x + w, childrenX);
        };

        const cgRoot = this
            .timeRangeStats
            .getCallTreeStats(this.timeRange)
            .getRoot()
        ;

        const maxCumInc = cgRoot.getMaxCumInc().getValue(this.currentMetric);

        let x = 0;
        for (const child of cgRoot.getChildren()) {
            x = renderNode(child, maxCumInc, x);
        }

        this.viewPort.flushFragment();

        this.pointedElement = null;

        this.infoViewPort = this.viewPort.createSubViewPort(
            this.viewPort.width,
            65,
            0,
            0
        );
    };
}

export class FlatProfile extends Widget {

    constructor(container, profileData) {
        super(container, profileData);

        this.sortCol = 'exc';
        this.sortDir = -1;
    }

    onTimeRangeUpdate() {
        this.repaint();
    }

    render() {

        let html = `
<table width="${this.container.width() - 20}px">
<thead>
    <tr>
        <th rowspan="3" class="sortable" data-sort="name">Function</th>
        <th rowspan="3" width="80px" class="sortable" data-sort="called">Called</th>
        <th colspan="4">${this.profileData.getMetricInfo(this.currentMetric).name}</th>
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

        const functionsStats = this.timeRangeStats.getFunctionsStats().getValues();

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

        const formatter = this.profileData.getMetricFormatter(this.currentMetric);
        const limit = Math.min(100, functionsStats.length);

        const cumCostStats = this.timeRangeStats.getCumCostStats();

        const renderRelativeCostBar = (value) => {
            if (this.profileData.isReleasableMetric(this.currentMetric)) {
                return `
                    <div style="display: flex; width: 100%; height: 2px">
                        <div style="width: ${value > 0 ? 50 : Math.round(50 * (1 + value))}%;"></div>
                        <div style="width: 50%; height: 100%">
                            <div style="width: ${Math.round(100 * Math.abs(value))}%; height: 100%; background-color: ${value > 0 ? 'red' : 'blue'}"></div>
                        </div>
                    </div>
                `;
            }

            return `
                <div style="width=100%; height: 2px">
                    <div style="width: ${Math.round(100 * value)}%; height: 100%; background-color: red"></div>
                </div>
            `;
        };

        for (let i = 0; i < limit; i++) {
            const stats = functionsStats[i];

            const neg = stats.inc.getValue(this.currentMetric) < 0 ? 1 : 0;
            const relRange =  neg ?
                cumCostStats.getNegRange(this.currentMetric) : cumCostStats.getPosRange(this.currentMetric);

            const inc = stats.inc.getValue(this.currentMetric);
            const incRel = -1 * neg + relRange.lerpDist(
                stats.inc.getValue(this.currentMetric)
            );

            const exc = stats.exc.getValue(this.currentMetric);
            const excRel = -1 * neg + relRange.lerpDist(
                stats.exc.getValue(this.currentMetric)
            );

            let functionLabel = stats.functionName;
            if (stats.maxCycleDepth > 0) {
                functionLabel += '@' + stats.maxCycleDepth;
            }

            html += `
<tr>
    <td
        data-function-name="${stats.functionName}"
        title="${functionLabel}"
        style="text-align: left; font-size: 12px; color: black; background-color: ${
            this.functionColorResolver(
                stats.functionName,
                getCallMetricValueColor(
                    this.profileData,
                    this.currentMetric,
                    stats.inc.getValue(this.currentMetric)
                )
            )
        }"
    >
        ${utils.truncateFunctionName(functionLabel, (this.container.width() - 5 * 90) / 8)}
    </td>
    <td width="80px">${fmt.quantity(stats.called)}</td>
    <td width="80px">${fmt.pct(incRel)}${renderRelativeCostBar(incRel)}</td>
    <td width="80px">${fmt.pct(excRel)}${renderRelativeCostBar(excRel)}</td>
    <td width="80px">${formatter(inc)}</td>
    <td width="80px">${formatter(exc)}</td>
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

        this.container.find('tbody td').click(e => {
            const functionName = $(e.target).data('function-name');

            $(window).trigger(
                'spx-highlighted-function-update',
                [
                    functionName != undefined ? functionName : null
                ]
            );
        });
    }
}
