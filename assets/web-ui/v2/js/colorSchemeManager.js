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

import * as math from './math.js';
import * as utils from './utils.js';
import { SearchManager } from './searchManager.js';

export class ColorSchemeManager {
    static get COLOR_MODE_METRIC_COST() {
        return 'metric_cost';
    }

    static get COLOR_MODE_AUTO_CATEGORY() {
        return 'auto_category';
    }

    static get COLOR_MODE_CUSTOM_CATEGORY() {
        return 'custom_category';
    }

    static #selectedColorMode = ColorSchemeManager.COLOR_MODE_AUTO_CATEGORY;
    static #highlightedFunctionName = null;
    static #customCategories;
    static #regexCache = new Map();

    static getColorModes() {
        return Object.getOwnPropertyNames(this)
            .filter((name) => name.startsWith('COLOR_MODE_'))
            .map((name) => this[name]);
    }

    static setSelectedColorMode(colorMode) {
        ColorSchemeManager.#selectedColorMode = colorMode;
    }

    static getSelectedColorMode() {
        return ColorSchemeManager.#selectedColorMode;
    }

    static getCustomCategories() {
        if (ColorSchemeManager.#customCategories === undefined) {
            ColorSchemeManager.#customCategories = loadCustomCategories();
        }

        return ColorSchemeManager.#customCategories;
    }

    static setCustomCategories(customCategories) {
        ColorSchemeManager.#customCategories = customCategories;
        saveCustomCategories(ColorSchemeManager.#customCategories);
        ColorSchemeManager.#regexCache.clear();
    }

    static updateCustomCategory(index, updater) {
        const categories = ColorSchemeManager.getCustomCategories();
        if (categories[index]) {
            updater(categories[index]);
            ColorSchemeManager.setCustomCategories(categories);
        }
    }

    static removeCustomCategory(index) {
        const categories = ColorSchemeManager.getCustomCategories();
        categories.splice(index, 1);
        ColorSchemeManager.setCustomCategories(categories);
    }

    static setHighlightedFunctionName(functionName) {
        this.#highlightedFunctionName = functionName;
    }

    static resolveFunctionColor(functionName, metricRange, metricValue) {
        let color;

        switch (ColorSchemeManager.#selectedColorMode) {
            case ColorSchemeManager.COLOR_MODE_METRIC_COST:
                color = resolveMetricValueColor(metricRange, metricValue);

                break;

            case ColorSchemeManager.COLOR_MODE_AUTO_CATEGORY:
                color = resolveFunctionColor(functionName);

                break;

            case ColorSchemeManager.COLOR_MODE_CUSTOM_CATEGORY:
                color = '#888';

                for (const category of ColorSchemeManager.getCustomCategories()) {
                    let found = false;

                    for (const pattern of category.patterns) {
                        let regex;

                        if (ColorSchemeManager.#regexCache.has(pattern)) {
                            regex = ColorSchemeManager.#regexCache.get(pattern);
                        } else {
                            regex = utils.createRegexFromSearchQuery(pattern);
                            ColorSchemeManager.#regexCache.set(pattern, regex);
                        }

                        if (regex && regex.test(functionName)) {
                            color = category.color;
                            found = true;
                        }

                        if (found) {
                            break;
                        }
                    }

                    if (found) {
                        break;
                    }
                }

                break;

            default:
                throw new Error('Invalid color mode');
        }

        if (ColorSchemeManager.#highlightedFunctionName !== null) {
            color = math.Vec3.createFromHtmlColor(color)
                .mult(
                    functionName == ColorSchemeManager.#highlightedFunctionName
                        ? 1.5
                        : 0.33
                )
                .toHTMLColor();
        }

        if (SearchManager.isFunctionMatchingQuery(functionName)) {
            color = '#fcc';
        }

        return color;
    }
}

function resolveFunctionColor(fullyQualifiedName) {
    const parts = fullyQualifiedName.split('\\');

    let topLevel = parts[0];
    const secondLevel = parts[1] ?? null;

    if (secondLevel === null) {
        // so that all global functions will have the same color
        topLevel = '$global';
    }

    // Very common algo, found in many places on the web.
    // This one was found here: https://mojoauth.com/hashing/fast-hash-in-javascript-in-browser#advantages-and-disadvantages-of-fast-hash
    function fastHash(str) {
        let hash = 0;
        for (let i = 0; i < str.length; i++) {
            hash = (hash << 5) - hash + str.charCodeAt(i); // Hash computation
            hash |= 0; // Convert to 32bit integer
        }
        return hash >>> 0; // Ensure the result is unsigned
    }

    const hue = fastHash(topLevel) % 360;
    const saturation =
        secondLevel !== null ? 50 + (fastHash(secondLevel) % 51) : 30;
    const lightness = 60;

    // Found here: https://stackoverflow.com/questions/2353211/hsl-to-rgb-color-conversion
    // input: h as an angle in [0,360] and s,l in [0,1] - output: r,g,b in [0,1]
    function hsl2rgb(h, s, l) {
        let a = s * Math.min(l, 1 - l);
        let f = (n, k = (n + h / 30) % 12) =>
            l - a * Math.max(Math.min(k - 3, 9 - k, 1), -1);
        return [f(0), f(8), f(4)];
    }

    function rgbColorToHtmlHexString(color) {
        const toHex = (n) =>
            Math.round(n * 255)
                .toString(16)
                .padStart(2, '0')
                .toUpperCase();
        return `#${toHex(color[0])}${toHex(color[1])}${toHex(color[2])}`;
    }

    return rgbColorToHtmlHexString(
        hsl2rgb(hue, saturation / 100, lightness / 100)
    );
}

function resolveMetricValueColor(metricRange, metricValue) {
    let scaleValue = 0;

    // this bounding is required since value can be lower than the lowest sample
    // (represented by metricRange.start). It is the case when value is interpolated
    // from 2 consecutive samples
    metricValue = Math.max(metricRange.start, metricValue);

    if (metricRange.length() > 100) {
        scaleValue =
            Math.log10(metricValue - metricRange.start) /
            Math.log10(metricRange.length());
    } else {
        scaleValue = metricRange.lerp(metricValue);
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

const localStorageKey = 'spx-report-custom-categories';

function loadCustomCategories() {
    try {
        const json = localStorage.getItem(localStorageKey);
        if (!json) {
            return [];
        }

        const categories = JSON.parse(json);

        if (!Array.isArray(categories)) {
            return [];
        }

        return categories;
    } catch (e) {
        console.error('Custom categories loading error: ', e);

        return [];
    }
}

function saveCustomCategories(categories) {
    try {
        localStorage.setItem(localStorageKey, JSON.stringify(categories));
    } catch (e) {
        console.error('Custom categories saving error: ', e);
    }
}
