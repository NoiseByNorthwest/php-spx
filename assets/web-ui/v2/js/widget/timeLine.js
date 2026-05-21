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

import * as utils from './../utils.js';
import * as svg from './../svg.js';
import {
    ViewTimeRange,
    renderSVGTimeGrid,
    renderSVGMultiLineText,
    renderSVGMetricValuesPlot,
} from './widget.js';
import { ColorSchemeManager } from './../colorSchemeManager.js';
import { SVGWidget } from './svgWidget.js';

export class TimeLine extends SVGWidget {
    constructor(container, profileDataAnalyzer) {
        super(container, profileDataAnalyzer);

        this.timeRangeAnalyzer.setMinRelativeDurationThreshold(
            1 / this.viewPort.width
        );

        this.viewTimeRange = new ViewTimeRange(
            this.profileDataAnalyzer.getTimeRange(),
            this.profileDataAnalyzer.getWallTime(),
            this.viewPort.width
        );

        this.offsetY = 0;

        this.svgRectPool = new svg.NodePool('rect');
        this.svgTextPool = new svg.NodePool('text');

        this.container.addEventListener('wheel', (e) => {
            if (e.deltaY == 0) {
                return;
            }

            e.preventDefault();
            let f = 1 + Math.abs(e.deltaY / 1000);
            if (e.deltaY < 0) {
                f = 1 / f;
            }

            this.viewTimeRange.zoomScaledViewRange(f, e.clientX);

            this.notifyTimeRangeUpdate(this.viewTimeRange.getTimeRange());
        });

        this.infoViewPort = null;
        this.selectedCallIdx = null;

        const firstPos = { x: 0, y: 0 };
        const lastPos = { x: 0, y: 0 };
        let dragging = false;
        let pointedElement = null,
            callIdx = null;

        this.container.addEventListener('mousedown', (e) => {
            dragging = true;

            firstPos.x = e.clientX;
            firstPos.y = e.clientY;
            lastPos.x = e.clientX;
            lastPos.y = e.clientY;
        });

        document.addEventListener('mouseup', (e) => {
            if (!dragging) {
                return;
            }

            dragging = false;
            if (firstPos.x != e.clientX || firstPos.y != e.clientY) {
                return;
            }

            window.dispatchEvent(
                new CustomEvent('spx-highlighted-function-update', {
                    detail:
                        callIdx != null
                            ? this.profileDataAnalyzer
                                  .getCall(callIdx)
                                  .getFunctionName()
                            : null,
                })
            );

            this.selectedCallIdx = callIdx;
        });

        document.addEventListener('mousemove', (e) => {
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

        this.viewPort.node.addEventListener('dblclick', (e) => {
            if (callIdx == null) {
                return;
            }

            this.notifyTimeRangeUpdate(
                this.profileDataAnalyzer.getCall(callIdx).getTimeRange()
            );
        });

        this.viewPort.node.addEventListener('mousemove', (e) => {
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

        this.viewPort.node.addEventListener('mouseout', (e) => {
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
        });
    }

    onTimeRangeUpdate() {
        this.viewTimeRange.setTimeRange(this.timeRange);
        this.repaint();
    }

    onLevelOfDetailsUpdate() {
        this.repaint();
    }

    onContainerResize() {
        super.onContainerResize();
        this.timeRangeAnalyzer.setMinRelativeDurationThreshold(
            1 / this.viewPort.width
        );
        this.viewTimeRange.setViewWidth(this.container.offsetWidth);
    }

    onHighlightedFunctionUpdate() {
        this.selectedCallIdx = null;
        super.onHighlightedFunctionUpdate();
    }

    render() {
        this.viewPort.appendChildToFragment(
            svg.createNode('rect', {
                x: 0,
                y: 0,
                width: this.viewPort.width,
                height: this.viewPort.height,
                'fill-opacity': '0.1',
            })
        );

        const timeRange = this.viewTimeRange.getTimeRange();
        const calls = this.timeRangeAnalyzer.getSignificantCalls();

        const viewRange = this.viewTimeRange.getScaledViewRange();
        const offsetX = -viewRange.start;

        this.svgRectPool.releaseAll();
        this.svgTextPool.releaseAll();

        for (const subRangeInfo of this.timeRangeAnalyzer.getSubRangesInfo()) {
            const rect = this.svgRectPool.acquire({
                x:
                    offsetX +
                    (this.viewPort.width * subRangeInfo.range.start) /
                        timeRange.length(),
                y: this.offsetY,
                width:
                    (this.viewPort.width * subRangeInfo.range.length()) /
                    timeRange.length(),
                height: 13 * (subRangeInfo.maxDepth + 1),
                stroke: 'none',
                fill: `rgba(19, 49, 69, ${0.6 * this.timeRangeAnalyzer.getLevelOfDetails()})`,
            });

            rect.removeAttribute('data-call-idx');

            this.viewPort.appendChildToFragment(rect);
        }

        for (let i = 0; i < calls.length; i++) {
            const call = calls[i];

            let x =
                offsetX +
                (this.viewPort.width * call.getStart('wt')) /
                    timeRange.length();

            if (x > this.viewPort.width) {
                continue;
            }

            let w =
                (this.viewPort.width * call.getInc('wt')) / timeRange.length() -
                1;

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
                fill: ColorSchemeManager.resolveFunctionColor(
                    call.getFunctionName(),
                    this.profileDataAnalyzer
                        .getStats()
                        .getCallRange(this.currentMetric),
                    call.getInc(this.currentMetric)
                ),
                'data-call-idx': call.getIdx(),
            });

            this.viewPort.appendChildToFragment(rect);

            if (w > 20) {
                const text = this.svgTextPool.acquire({
                    x: x + 2,
                    y: y + h * 0.75,
                    width: w,
                    height: h,
                    'font-size': h - 2,
                });

                text.textContent = utils.truncateFunctionName(
                    call.getFunctionName(),
                    w / 7
                );
                this.viewPort.appendChildToFragment(text);
            }
        }

        renderSVGTimeGrid(this.viewPort, timeRange, true);

        this.viewPort.flushFragment();

        const overlayHeight = 100;
        const overlayViewPort = this.viewPort.createSubViewPort(
            this.viewPort.width,
            overlayHeight,
            0,
            this.viewPort.height - overlayHeight
        );

        overlayViewPort.appendChildToFragment(
            svg.createNode('rect', {
                x: 0,
                y: 0,
                width: overlayViewPort.width,
                height: overlayViewPort.height,
                'fill-opacity': '0.5',
            })
        );

        if (this.currentMetric != 'wt') {
            renderSVGMetricValuesPlot(
                overlayViewPort,
                this.profileDataAnalyzer,
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

        this.infoViewPort.node.style.cursor = 'text';
        this.infoViewPort.node.style.userSelect = 'text';
        this.infoViewPort.node.addEventListener('mousedown', (e) => {
            e.stopPropagation();
        });
        this.infoViewPort.node.addEventListener('mousemove', (e) => {
            e.stopPropagation();
        });

        if (this.selectedCallIdx != null) {
            this._renderCallInfo(this.selectedCallIdx);
        }
    }

    _renderCallInfo(callIdx) {
        const call = this.profileDataAnalyzer.getCall(callIdx);
        const currentMetricName = this.profileDataAnalyzer.getMetricInfo(
            this.currentMetric
        ).name;
        const formatter = this.profileDataAnalyzer.getMetricFormatter(
            this.currentMetric
        );

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
                currentMetricName +
                    ' inc.: ' +
                    formatter(call.getInc(this.currentMetric)),
                currentMetricName +
                    ' exc.: ' +
                    formatter(call.getExc(this.currentMetric)),
            ]
        );
    }
}
