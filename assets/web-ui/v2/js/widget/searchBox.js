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
import { SearchManager } from './../searchManager.js';
import { Widget } from './widget.js';

export class SearchBox extends Widget {
    constructor(container, profileDataAnalyzer) {
        super(container, profileDataAnalyzer);
    }

    onColorSchemeModeUpdate() {}

    onColorSchemeCategoryUpdate() {}

    onHighlightedFunctionUpdate() {}

    onSearchQueryUpdate() {}

    render() {
        this.container.innerHTML = `
            <input
                type="text"
                autocomplete="off"
                value="${SearchManager.getSearchQuery() ?? ''}"
                placeholder="Search functions (substring/glob/regex)"
            >
            <button
                style="display: ${SearchManager.hasSearchQuery() ? 'inline-block' : 'none'}"
            >
                X
            </button>
        `;

        const input = this.container.querySelector('input');
        const clearButton = this.container.querySelector('button');

        input.addEventListener(
            'input',
            utils.debounce(() => {
                if (input.value.length > 0) {
                    clearButton.style.display = 'inline-block';
                } else {
                    clearButton.style.display = 'none';
                }

                SearchManager.setSearchQuery(input.value);
                window.dispatchEvent(
                    new CustomEvent('spx-search-query-update')
                );
            }, 500)
        );

        clearButton.addEventListener('click', () => {
            input.value = '';
            clearButton.style.display = 'none';

            input.dispatchEvent(new Event('input'));
        });
    }
}
