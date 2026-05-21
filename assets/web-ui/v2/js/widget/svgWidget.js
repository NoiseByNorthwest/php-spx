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

import { Widget, ViewPort } from './widget.js';

export class SVGWidget extends Widget {
    constructor(container, profileDataAnalyzer) {
        super(container, profileDataAnalyzer);

        this.viewPort = new ViewPort(
            this.container.offsetWidth,
            this.container.offsetHeight
        );

        this.container.appendChild(this.viewPort.node);
    }

    clear() {
        this.viewPort.clear();
    }

    onContainerResize() {
        super.onContainerResize();

        // viewPort internal svg shrinking is first required to let the container get
        // its actual size.
        this.viewPort.resize(0, 0);

        this.viewPort.resize(
            this.container.offsetWidth,
            this.container.offsetHeight
        );
    }
}
