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

import * as math from './../math.js';
import * as svg from './../svg.js';
import { ColorSchemeManager } from './../colorSchemeManager.js';
import {
    ViewTimeRange,
    renderSVGTimeGrid,
    renderSVGMetricValuesPlot,
} from './widget.js';
import { SVGWidget } from './svgWidget.js';

export class OverView extends SVGWidget {
    constructor(container, profileDataAnalyzer) {
        super(container, profileDataAnalyzer);

        this.viewTimeRange = new ViewTimeRange(
            this.profileDataAnalyzer.getTimeRange(),
            this.profileDataAnalyzer.getWallTime(),
            this.viewPort.width
        );

        let action = null;
        let dragging = false;

        this.container.addEventListener('mousemove', (e) => {
            if (dragging) {
                return;
            }

            const viewRange = this.viewTimeRange.getViewRange();

            if (Math.abs(e.clientX - viewRange.start) < 4) {
                this.container.style.cursor = 'e-resize';
                action = 'move-start';
            } else if (Math.abs(e.clientX - viewRange.end) < 4) {
                this.container.style.cursor = 'w-resize';
                action = 'move-end';
            } else {
                this.container.style.cursor = 'pointer';
                action = 'move';
            }
        });

        const updateTimeRange = (posX) => {
            switch (action) {
                case 'move-start':
                    this.viewTimeRange.shiftViewRangeStart(
                        posX - this.viewTimeRange.getViewRange().start
                    );

                    break;

                case 'move-end':
                    this.viewTimeRange.shiftViewRangeEnd(
                        posX - this.viewTimeRange.getViewRange().end
                    );

                    break;

                case 'move':
                    this.viewTimeRange.shiftViewRange(
                        posX - this.viewTimeRange.getViewRange().center()
                    );

                    break;
            }

            this.notifyTimeRangeUpdate(this.viewTimeRange.getTimeRange());
        };

        this.container.addEventListener('mousedown', (e) => {
            dragging = true;

            updateTimeRange(e.clientX);
        });

        document.addEventListener('mousemove', (e) => {
            if (!dragging) {
                return;
            }

            updateTimeRange(e.clientX);
        });

        document.addEventListener('mouseup', () => {
            dragging = false;
        });

        this.container.addEventListener('mouseleave', () => {
            if (!dragging) {
                action = null;
                this.container.style.cursor = 'default';
            }
        });
    }

    onTimeRangeUpdate() {
        this.viewTimeRange.setTimeRange(this.timeRange);
        if (!this.timeRangeRect) {
            return;
        }

        const viewRange = this.viewTimeRange.getViewRange();

        this.timeRangeRect.setAttribute('x', viewRange.start);
        this.timeRangeRect.setAttribute('width', viewRange.length());
    }

    onContainerResize() {
        super.onContainerResize();
        this.viewTimeRange.setViewWidth(this.container.offsetWidth);
    }

    render() {
        this.viewPort.appendChildToFragment(
            svg.createNode('rect', {
                x: 0,
                y: 0,
                width: this.viewPort.width,
                height: this.viewPort.height,
                'fill-opacity': '0.3',
            })
        );

        const calls = this.timeRangeAnalyzer.getSignificantCalls();

        for (let i = 0; i < calls.length; i++) {
            const call = calls[i];

            const x =
                (this.viewPort.width * call.getStart('wt')) /
                this.profileDataAnalyzer.getWallTime();
            const w =
                (this.viewPort.width * call.getInc('wt')) /
                    this.profileDataAnalyzer.getWallTime() -
                1;

            if (w < 0.3) {
                continue;
            }

            const h = 1;
            const y = call.getDepth();

            this.viewPort.appendChildToFragment(
                svg.createNode('line', {
                    x1: x,
                    y1: y,
                    x2: x + w,
                    y2: y + h,
                    stroke: ColorSchemeManager.resolveFunctionColor(
                        call.getFunctionName(),
                        this.profileDataAnalyzer
                            .getStats()
                            .getCallRange(this.currentMetric),
                        call.getInc(this.currentMetric)
                    ),
                })
            );
        }

        renderSVGTimeGrid(
            this.viewPort,
            this.profileDataAnalyzer.getTimeRange()
        );

        if (this.currentMetric != 'wt') {
            renderSVGMetricValuesPlot(
                this.viewPort,
                this.profileDataAnalyzer,
                this.currentMetric,
                this.profileDataAnalyzer.getTimeRange()
            );
        }

        const viewRange = this.viewTimeRange.getViewRange();

        this.timeRangeRect = svg.createNode('rect', {
            x: viewRange.start,
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
