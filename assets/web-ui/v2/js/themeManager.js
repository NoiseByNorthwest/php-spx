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

const TOKEN_NAMES = [
    'widget-scrim-color',
    'grid-line-color',
    'axis-label-color',
    'axis-label-minor-color',
    'axis-label-major-color',
    'overlay-text-color',
    'selection-stroke-color',
    'metric-plot-color',
    'flamegraph-separator-color',
    'timeline-depth-band-color',
    'dimmed-target-color',
    'category-default-color',
    'cost-bar-positive-color',
    'cost-bar-negative-color',
];

const NUMBER_TOKEN_NAMES = ['highlighted-brightness-factor'];

function readTokens() {
    const style = getComputedStyle(document.documentElement);
    const values = {};
    for (const tokenName of TOKEN_NAMES.concat(NUMBER_TOKEN_NAMES)) {
        values[tokenName] = style.getPropertyValue('--' + tokenName).trim();
    }

    return values;
}

export class ThemeManager {
    static #values = null;

    static {
        // The palette is picked up from the stylesheet, so a change of the OS
        // colour scheme swaps every token at once. Dropping the cache and
        // asking for a repaint is enough to bring the widgets along.
        window
            .matchMedia('(prefers-color-scheme: light)')
            .addEventListener('change', () => {
                ThemeManager.#values = null;

                window.dispatchEvent(new CustomEvent('spx-theme-update'));
            });
    }

    static getColor(name) {
        if (ThemeManager.#values === null) {
            ThemeManager.#values = readTokens();
        }

        const color = ThemeManager.#values[name];
        if (DEBUG) {
            if (!color) {
                throw new Error('Unknown or unset theme color: ' + name);
            }
        }

        return color;
    }

    static getScrimColor() {
        return ThemeManager.getColor('widget-scrim-color');
    }

    static getGridLineColor() {
        return ThemeManager.getColor('grid-line-color');
    }

    static getAxisLabelColor() {
        return ThemeManager.getColor('axis-label-color');
    }

    static getAxisLabelMinorColor() {
        return ThemeManager.getColor('axis-label-minor-color');
    }

    static getAxisLabelMajorColor() {
        return ThemeManager.getColor('axis-label-major-color');
    }

    static getOverlayTextColor() {
        return ThemeManager.getColor('overlay-text-color');
    }

    static getSelectionStrokeColor() {
        return ThemeManager.getColor('selection-stroke-color');
    }

    static getMetricPlotColor() {
        return ThemeManager.getColor('metric-plot-color');
    }

    static getFlameGraphSeparatorColor() {
        return ThemeManager.getColor('flamegraph-separator-color');
    }

    static getTimelineDepthBandColor() {
        return ThemeManager.getColor('timeline-depth-band-color');
    }

    static getDimmedTargetColor() {
        return ThemeManager.getColor('dimmed-target-color');
    }

    static getHighlightedBrightnessFactor() {
        const factor = parseFloat(
            ThemeManager.getColor('highlighted-brightness-factor')
        );

        if (DEBUG) {
            if (isNaN(factor)) {
                throw new Error(
                    '--highlighted-brightness-factor is not a number'
                );
            }
        }

        return factor;
    }

    static getCategoryDefaultColor() {
        return ThemeManager.getColor('category-default-color');
    }

    static getCostBarPositiveColor() {
        return ThemeManager.getColor('cost-bar-positive-color');
    }

    static getCostBarNegativeColor() {
        return ThemeManager.getColor('cost-bar-negative-color');
    }
}
