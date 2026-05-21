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
import * as math from './../math.js';
import * as svg from './../svg.js';
import { ColorSchemeManager } from './../colorSchemeManager.js';
import { renderSVGMultiLineText } from './widget.js';
import { SVGWidget } from './svgWidget.js';

export class FlameGraph extends SVGWidget {
    constructor(container, profileDataAnalyzer) {
        super(container, profileDataAnalyzer);

        this.svgRectPool = new svg.NodePool('rect');
        this.svgTextPool = new svg.NodePool('text');

        this.pointedElement = null;
        this.renderedCgNodes = [];
        this.infoViewPort = null;

        this.viewPort.node.addEventListener('mouseout', (e) => {
            if (this.pointedElement != null) {
                this.pointedElement.setAttribute('stroke', 'none');
                this.pointedElement = null;
            }

            this.infoViewPort.clear();
        });

        this.viewPort.node.addEventListener('mousemove', (e) => {
            if (this.pointedElement != null) {
                this.pointedElement.setAttribute('stroke', 'none');
                this.pointedElement = null;
            }

            this.infoViewPort.clear();

            this.pointedElement = document.elementFromPoint(
                e.clientX,
                e.clientY
            );
            if (this.pointedElement.nodeName == 'text') {
                this.pointedElement = this.pointedElement.previousSibling;
            }

            const cgNodeIdx = this.pointedElement?.dataset.cgNodeIdx;
            if (cgNodeIdx === undefined) {
                this.pointedElement = null;
                return;
            }

            this.pointedElement.setAttribute('stroke', '#0ff');

            this.infoViewPort.appendChild(
                svg.createNode('rect', {
                    x: 0,
                    y: 0,
                    width: this.infoViewPort.width,
                    height: this.infoViewPort.height,
                    'fill-opacity': '0.5',
                })
            );

            const cgNode = this.renderedCgNodes[cgNodeIdx];
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
                    'Function: ' + cgNode.getFunctionName(),
                    'Depth: ' + cgNode.getDepth(),
                    'Called: ' + cgNode.getCalled(),
                    currentMetricName +
                        ' inc.: ' +
                        formatter(cgNode.getInc().getValue(this.currentMetric)),
                ]
            );
        });

        this.viewPort.node.addEventListener('click', (e) => {
            const functionName =
                this.pointedElement != null
                    ? this.renderedCgNodes[
                          this.pointedElement.dataset.cgNodeIdx
                      ].getFunctionName()
                    : null;

            window.dispatchEvent(
                new CustomEvent('spx-highlighted-function-update', {
                    detail: functionName,
                })
            );
        });
    }

    onTimeRangeUpdate() {
        this.repaint();
    }

    onLevelOfDetailsUpdate() {
        this.repaint();
    }

    render() {
        this.viewPort.appendChild(
            svg.createNode('rect', {
                x: 0,
                y: 0,
                width: this.viewPort.width,
                height: this.viewPort.height,
                'fill-opacity': '0.1',
            })
        );

        if (this.profileDataAnalyzer.isReleasableMetric(this.currentMetric)) {
            this.viewPort.appendChild(
                svg.createNode(
                    'text',
                    {
                        x: this.viewPort.width / 4,
                        y: this.viewPort.height / 2,
                        height: 20,
                        'font-size': 14,
                        fill: '#089',
                    },
                    (node) => {
                        node.textContent =
                            'This visualization is not available for this metric.';
                    }
                )
            );

            return;
        }

        this.svgRectPool.releaseAll();
        this.svgTextPool.releaseAll();

        const cgRoot = this.timeRangeAnalyzer
            .getCallTreeStats(this.timeRange)
            .getRoot();

        const totalInc = cgRoot.getInc().getValue(this.currentMetric);
        this.renderedCgNodes = [];

        const renderNode = (node, x = 0, y = this.viewPort.height) => {
            const w =
                (this.viewPort.width *
                    node.getInc().getValue(this.currentMetric)) /
                totalInc;
            if (w < 0.3) {
                return x;
            }

            const h = math.bound(y / (node.getDepth() + 1), 2, 12);
            y -= h + 0.5;

            let childrenX = x;
            for (let child of node.getChildren()) {
                childrenX = renderNode(child, childrenX, y);
            }

            this.renderedCgNodes.push(node);
            const nodeIdx = this.renderedCgNodes.length - 1;

            this.viewPort.appendChildToFragment(
                this.svgRectPool.acquire({
                    x: x,
                    y: y,
                    width: w,
                    height: h,
                    stroke: 'none',
                    'stroke-width': 2,
                    fill:
                        ColorSchemeManager.getSelectedColorMode() ===
                        ColorSchemeManager.COLOR_MODE_METRIC_COST
                            ? math.Vec3.lerp(
                                  new math.Vec3(1, 0, 0),
                                  new math.Vec3(1, 1, 0),
                                  0.5 * Math.min(1, node.getDepth() / 20) +
                                      (0.5 *
                                          node
                                              .getInc()
                                              .getValue(this.currentMetric)) /
                                          totalInc
                              ).toHTMLColor()
                            : ColorSchemeManager.resolveFunctionColor(
                                  node.getFunctionName()
                              ),
                    'fill-opacity': '1',
                    'data-cg-node-idx': nodeIdx,
                })
            );

            if (w > 20 && h > 5) {
                const text = this.svgTextPool.acquire({
                    x: x + 2,
                    y: y + h * 0.75,
                    width: w,
                    height: h,
                    'font-size': h - 2,
                });

                text.textContent = utils.truncateFunctionName(
                    node.getFunctionName(),
                    w / 7
                );
                this.viewPort.appendChildToFragment(text);
            }

            return Math.max(x + w, childrenX);
        };

        let x = 0;
        for (const child of cgRoot.getChildren()) {
            x = renderNode(child, x);
        }

        this.viewPort.flushFragment();
        this.pointedElement = null;

        this.infoViewPort = this.viewPort.createSubViewPort(
            this.viewPort.width,
            65,
            0,
            0
        );
    }
}
