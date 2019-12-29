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


let changeHandler = null;

export function init() {
    let resizedElement = null;
    let dir = 1;
    let horizontal = true;
    let min = 0;
    let max = 0;
    let originalSize = 0;
    let originalPos = 0;

    $("div[data-layout-splitter-target]").mousedown((e) => {
        resizedElement = document.getElementById(e.target.getAttribute("data-layout-splitter-target"));
        dir = parseInt(e.target.getAttribute("data-layout-splitter-dir") || "1");
        horizontal = "x" === (e.target.getAttribute("data-layout-splitter-axis") || "x");
        min = Math.max(10, parseInt(e.target.getAttribute("data-layout-splitter-min")));
        max = Math.round(0.9 * (horizontal ? resizedElement.parentNode.offsetWidth : resizedElement.parentNode.offsetHeight));

        originalPos = horizontal ? e.originalEvent.clientX : e.originalEvent.clientY;
        originalSize = horizontal ? resizedElement.offsetWidth : resizedElement.offsetHeight;

        e.preventDefault();
    });

    $(window).mouseup((e) => {
        if (!resizedElement) {
            return;
        }

        e.preventDefault();
        resizedElement = null;
    });

    $(window).mousemove((e) => {
        if (!resizedElement) {
            return;
        }

        e.preventDefault();

        const currentPos = horizontal ? e.originalEvent.clientX : e.originalEvent.clientY;
        const currentSize = horizontal ? resizedElement.offsetWidth : resizedElement.offsetHeight;

        let newSize = (originalSize + dir * (originalPos - currentPos));

        if (newSize < min) {
            newSize = min;
        }

        if (newSize > max) {
            newSize = max;
        }

        if (newSize == currentSize) {
            return;
        }

        if (horizontal) {
            resizedElement.style.width = newSize + 'px';
        } else {
            resizedElement.style.height = newSize + 'px';
        }

        changeHandler && changeHandler();
    });
}

export function change(handler) {
    changeHandler = handler;
}
