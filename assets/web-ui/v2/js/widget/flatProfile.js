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
import * as fmt from './../fmt.js';
import { ColorSchemeManager } from './../colorSchemeManager.js';
import { SearchManager } from './../searchManager.js';
import { Widget } from './widget.js';

export class FlatProfile extends Widget {
    constructor(container, profileDataAnalyzer) {
        super(container, profileDataAnalyzer);

        this.sortCol = 'exc';
        this.sortDir = -1;
    }

    onTimeRangeUpdate() {
        this.repaint();
    }

    render() {
        let html = `
<table width="${this.container.offsetWidth - 20}px">
<thead>
    <tr>
        <th rowspan="3" class="sortable" data-sort="name">Function</th>
        <th rowspan="3" width="80px" class="sortable" data-sort="called">Called</th>
        <th colspan="4">${this.profileDataAnalyzer.getMetricInfo(this.currentMetric).name}</th>
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
<div style="overflow-y: auto; height: ${this.container.offsetHeight - 60}px">
<table width="${this.container.offsetWidth - 20}px"><tbody>
        `;

        const functionsStats = this.timeRangeAnalyzer
            .getFunctionsStats()
            .getValues()
            .filter(
                (stats) =>
                    !(
                        SearchManager.hasSearchQuery() &&
                        !SearchManager.isFunctionMatchingQuery(
                            stats.functionName
                        )
                    )
            );

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

            return (a < b ? -1 : a > b) * this.sortDir;
        });

        const formatter = this.profileDataAnalyzer.getMetricFormatter(
            this.currentMetric
        );
        const limit = SearchManager.hasSearchQuery()
            ? functionsStats.length
            : Math.min(100, functionsStats.length);

        const cumCostStats = this.timeRangeAnalyzer.getCumCostStats();

        const renderRelativeCostBar = (value) => {
            if (
                this.profileDataAnalyzer.isReleasableMetric(this.currentMetric)
            ) {
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
            const relRange = neg
                ? cumCostStats.getNegRange(this.currentMetric)
                : cumCostStats.getPosRange(this.currentMetric);

            const inc = stats.inc.getValue(this.currentMetric);
            const incRel =
                -1 * neg +
                relRange.lerpDist(stats.inc.getValue(this.currentMetric));

            const exc = stats.exc.getValue(this.currentMetric);
            const excRel =
                -1 * neg +
                relRange.lerpDist(stats.exc.getValue(this.currentMetric));

            let functionLabel = stats.functionName;
            if (stats.maxCycleDepth > 0) {
                functionLabel += '@' + stats.maxCycleDepth;
            }

            html += `
<tr>
    <td
        data-function-name="${stats.functionName}"
        title="${functionLabel}"
        style="text-align: left; font-size: 12px; color: black; background-color: ${ColorSchemeManager.resolveFunctionColor(
            stats.functionName,
            this.profileDataAnalyzer
                .getStats()
                .getCallRange(this.currentMetric),
            stats.inc.getValue(this.currentMetric)
        )}"
    >
        ${utils.truncateFunctionName(functionLabel, (this.container.offsetWidth - 5 * 90) / 8)}
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

        this.container.innerHTML += html;

        this.container
            .querySelector('th[data-sort="' + this.sortCol + '"]')
            .classList.add('sort');

        this.container.querySelectorAll('th').forEach((el) =>
            el.addEventListener('click', (e) => {
                let sortCol = e.target.dataset.sort;
                if (!sortCol) {
                    return;
                }

                if (this.sortCol == sortCol) {
                    this.sortDir *= -1;
                }

                this.sortCol = sortCol;
                this.repaint();
            })
        );

        this.container.querySelectorAll('tbody td').forEach((el) =>
            el.addEventListener('click', (e) => {
                const functionName = e.target.dataset.functionName;

                window.dispatchEvent(
                    new CustomEvent('spx-highlighted-function-update', {
                        detail: functionName != undefined ? functionName : null,
                    })
                );
            })
        );
    }
}
