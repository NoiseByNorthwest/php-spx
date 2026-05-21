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

import * as fmt from './../fmt.js';
import * as math from './../math.js';
import * as svg from './../svg.js';
import { ColorSchemeManager } from './../colorSchemeManager.js';

export function renderSVGTimeGrid(viewPort, timeRange, detailed) {
    const delta = timeRange.length();
    let step = Math.pow(10, parseInt(Math.log10(delta)));
    if (delta / step < 4) {
        step /= 5;
    }

    // 5 as min value so that minor step is lower bounded to 1
    step = Math.max(step, 5);

    const minorStep = step / 5;

    let tickTime = (parseInt(timeRange.start / minorStep) + 1) * minorStep;
    while (1) {
        const majorTick = tickTime % step == 0;
        const x = (viewPort.width * (tickTime - timeRange.start)) / delta;
        viewPort.appendChildToFragment(
            svg.createNode('line', {
                x1: x,
                y1: 0,
                x2: x,
                y2: viewPort.height,
                stroke: '#777',
                'stroke-width': majorTick ? 0.5 : 0.2,
            })
        );

        if (majorTick) {
            if (detailed) {
                const units = ['s', 'ms', 'us', 'ns'];
                let t = tickTime;
                const timeParts = [];
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

                    timeParts.push(m + unit);
                }

                let line = 0;
                for (const [i, timePart] of timeParts.reverse().entries()) {
                    viewPort.appendChildToFragment(
                        svg.createNode(
                            'text',
                            {
                                x: x + 2,
                                y: viewPort.height - 10 - 20 * i,
                                width: 100,
                                height: 15,
                                'font-size': 12,
                                fill:
                                    i < timeParts.length - 1 ? '#777' : '#ccc',
                            },
                            (node) => {
                                node.textContent = timePart;
                            }
                        )
                    );
                }
            } else {
                viewPort.appendChildToFragment(
                    svg.createNode(
                        'text',
                        {
                            x: x + 2,
                            y: viewPort.height - 10,
                            width: 100,
                            height: 15,
                            'font-size': 12,
                            fill: '#aaa',
                        },
                        (node) => (node.textContent = fmt.time(tickTime))
                    )
                );
            }
        }

        tickTime += minorStep;
        if (tickTime > timeRange.end) {
            break;
        }
    }
}

export function renderSVGMultiLineText(viewPort, lines) {
    let y = 15;

    const text = svg.createNode('text', {
        x: 0,
        y: y,
        'font-size': 12,
        fill: '#fff',
    });

    viewPort.appendChild(text);

    for (let line of lines) {
        text.appendChild(
            svg.createNode(
                'tspan',
                {
                    x: 0,
                    y: y,
                },
                (node) => (node.textContent = line)
            )
        );

        y += 15;
    }
}

export function renderSVGMetricValuesPlot(
    viewPort,
    profileDataAnalyzer,
    metric,
    timeRange
) {
    const timeComponentMetric = ['ct', 'it'].includes(metric);
    const valueRange = timeComponentMetric
        ? new math.Range(0, 1)
        : profileDataAnalyzer.getStats().getRange(metric);

    const step = 4;
    let previousMetricValues = null;
    let points = [];
    console.time('renderSVGMetricValuesPlot');
    for (let i = 0; i < viewPort.width; i += step) {
        const currentMetricValues = profileDataAnalyzer.getMetricValues(
            timeRange.lerp(i / viewPort.width)
        );

        if (timeComponentMetric && previousMetricValues == null) {
            previousMetricValues = currentMetricValues;

            continue;
        }

        let currentValue = currentMetricValues.getValue(metric);
        if (timeComponentMetric) {
            currentValue =
                (currentMetricValues.getValue(metric) -
                    previousMetricValues.getValue(metric)) /
                (currentMetricValues.getValue('wt') -
                    previousMetricValues.getValue('wt'));
        }

        points.push(i);
        points.push(
            parseInt(viewPort.height * (1 - valueRange.lerpDist(currentValue)))
        );

        previousMetricValues = currentMetricValues;
    }

    console.timeEnd('renderSVGMetricValuesPlot');

    viewPort.appendChildToFragment(
        svg.createNode('polyline', {
            points: points.join(' '),
            stroke: '#0af',
            'stroke-width': 2,
            fill: 'none',
        })
    );

    const tickValueStep = valueRange.lerp(0.25);
    let tickValue = tickValueStep;
    while (tickValue < valueRange.end) {
        const y = parseInt(
            viewPort.height * (1 - valueRange.lerpDist(tickValue))
        );

        viewPort.appendChildToFragment(
            svg.createNode('line', {
                x1: 0,
                y1: y,
                x2: viewPort.width,
                y2: y,
                stroke: '#777',
                'stroke-width': 0.5,
            })
        );

        viewPort.appendChildToFragment(
            svg.createNode(
                'text',
                {
                    x: 10,
                    y: y - 5,
                    width: 100,
                    height: 15,
                    'font-size': 12,
                    fill: '#aaa',
                },
                (node) => {
                    const formatter = timeComponentMetric
                        ? fmt.pct
                        : profileDataAnalyzer.getMetricFormatter(metric);
                    node.textContent = formatter(tickValue);
                }
            )
        );

        tickValue += tickValueStep;
    }
}

export class ViewTimeRange {
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

        this.timeRange.end = this.timeRange.start + minLength;
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

    shiftViewRangeStart(dist) {
        this.timeRange = this._viewRangeToTimeRange(
            this.getViewRange().shiftStart(dist)
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
        center /= this.getScale(); // scaled
        center += this.getViewRange().start; // translated
        center /= this.viewWidth; // view space -> norm space
        center *= this.wallTime; // norm space -> time space

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

export class ViewPort {
    constructor(width, height, x, y) {
        this.width = width;
        this.height = height;
        this.x = x ?? 0;
        this.y = y ?? 0;

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
        this.node.replaceChildren();
    }
}

export class Widget {
    constructor(container, profileDataAnalyzer) {
        // FIXME lots of data here are duplicated for each instances instead of being shared
        this.container = container;
        this.profileDataAnalyzer = profileDataAnalyzer;
        this.timeRange = profileDataAnalyzer.getTimeRange();

        this.timeRangeAnalyzer = profileDataAnalyzer.getTimeRangeAnalyzer();

        this.currentMetric =
            profileDataAnalyzer.getMetadata().enabled_metrics[0];

        this.repaintTimeout = null;
        this.lodTimeouts = [];
        this.resizingTimeouts = [];

        window.addEventListener('resize', () => this.handleResize());

        window.addEventListener('spx-timerange-update', (e) => {
            this.timeRange = e.detail;

            this.onTimeRangeUpdate();
        });

        window.addEventListener('spx-lod-update', (e) => {
            this.onLevelOfDetailsUpdate();
        });

        window.addEventListener('spx-color-scheme-mode-update', (e) => {
            this.onColorSchemeModeUpdate();
        });

        window.addEventListener('spx-color-scheme-category-update', () => {
            if (
                ColorSchemeManager.getSelectedColorMode() !==
                ColorSchemeManager.COLOR_MODE_CUSTOM_CATEGORY
            ) {
                return;
            }

            this.onColorSchemeCategoryUpdate();
        });

        window.addEventListener('spx-highlighted-function-update', (e) => {
            // FIXME this is done for each widget instances instead of once
            ColorSchemeManager.setHighlightedFunctionName(e.detail);
            this.onHighlightedFunctionUpdate();
        });

        window.addEventListener('spx-search-query-update', (e) => {
            this.onSearchQueryUpdate();
        });
    }

    onTimeRangeUpdate() {}

    onLevelOfDetailsUpdate() {}

    onColorSchemeModeUpdate() {
        this.repaint();
    }

    onColorSchemeCategoryUpdate() {
        this.repaint();
    }

    onHighlightedFunctionUpdate() {
        this.repaint();
    }

    onSearchQueryUpdate() {
        this.repaint();
    }

    notifyTimeRangeUpdate(timeRange) {
        for (const t of this.lodTimeouts) {
            clearTimeout(t);
        }

        this.lodTimeouts = [];

        this.timeRange = timeRange;

        let lod = 0.01;
        this.timeRangeAnalyzer.setTimeRange(timeRange, lod);

        window.dispatchEvent(
            new CustomEvent('spx-timerange-update', { detail: this.timeRange })
        );

        const setLod = (lod) => {
            this.timeRangeAnalyzer.setLevelOfDetails(lod);

            window.dispatchEvent(new CustomEvent('spx-lod-update'));
        };

        let i = 0;
        while (lod < 1) {
            lod = Math.min(1, lod + 0.1);

            const currentLod = lod;
            this.lodTimeouts.push(
                setTimeout(() => setLod(currentLod), ++i * 60)
            );
        }
    }

    notifyColorSchemeModeUpdate(colorSchemeMode) {
        ColorSchemeManager.setSelectedColorMode(colorSchemeMode);
        window.dispatchEvent(new CustomEvent('spx-color-scheme-mode-update'));
    }

    notifyColorSchemeCategoryUpdate() {
        window.dispatchEvent(new Event('spx-color-scheme-category-update'));
    }

    setCurrentMetric(metric) {
        this.currentMetric = metric;
    }

    clear() {
        this.container.replaceChildren();
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

        handle();

        // Several delayed handler() calls are required to both optimize responsiveness and fix
        // the appearing/disappearing scrollbar issue.
        this.resizingTimeouts.push(setTimeout(handle, 80));
        this.resizingTimeouts.push(setTimeout(handle, 800));
        this.resizingTimeouts.push(setTimeout(handle, 1500));
    }

    onContainerResize() {}

    render() {}

    repaint() {
        if (this.repaintTimeout !== null) {
            return;
        }

        this.repaintTimeout = setTimeout(() => {
            let initialScrollPos = 0;
            this.repaintTimeout = null;

            const id = this.container.id;
            if (id === 'flatprofile') {
                initialScrollPos =
                    this.container.querySelector('div').scrollTop;
            }
            console.time('repaint ' + id);
            this.clear();
            this.render();
            console.timeEnd('repaint ' + id);
            if (id === 'flatprofile') {
                this.container.querySelector('div').scrollTop =
                    initialScrollPos;
            }
        }, 0);
    }
}
