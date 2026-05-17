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

import * as fmt from './fmt.js';

export class ProgressBar {
    constructor() {
        this.element = document.createElement('div');
        this.element.className = 'progress';
        this.element.style.display = 'none';

        this._bar = document.createElement('div');
        this._bar.style.width = '0%';
        this.element.appendChild(this._bar);

        this.infoElement = document.createElement('p');
        this.infoElement.className = 'progress-info';

        this._progressData = {};
    }

    update(current, total, label = '') {
        if (!total) {
            this.element.style.display = 'none';
            this.infoElement.innerText = '';
            return;
        }

        this.element.style.display = 'block';
        this._bar.style.width = Math.round((100 * current) / total) + '%';

        const currentTime = performance.now();

        if (!(label in this._progressData)) {
            this._progressData[label] = {
                windowSamples: [
                    {
                        count: current,
                        time: currentTime,
                    },
                ],
            };
        }

        const progressData = this._progressData[label];

        const rate =
            (current - progressData.windowSamples[0].count) /
            ((currentTime - progressData.windowSamples[0].time) / 1000);
        const remainingTime = Math.round((total - current) / rate);

        while (
            progressData.windowSamples.length > 2 &&
            currentTime - progressData.windowSamples[0].time > 3000
        ) {
            progressData.windowSamples.shift();
        }

        progressData.windowSamples.push({
            count: current,
            time: currentTime,
        });

        this.infoElement.innerText = `${fmt.pct(current / total)} (${fmt.quantity(current)}, ${fmt.quantity(rate)}/s) ETA: ${remainingTime}s`;
    }

    reset() {
        this._progressData = {};
        this.element.style.display = 'none';
        this._bar.style.width = '0%';
        this.infoElement.innerText = '';
    }
}
