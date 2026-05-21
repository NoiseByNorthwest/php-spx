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

import { ColorSchemeManager } from './../colorSchemeManager.js';
import { Widget } from './widget.js';

export class ColorSchemeSelector extends Widget {
    constructor(container, profileDataAnalyzer) {
        super(container, profileDataAnalyzer);
    }

    onColorSchemeModeUpdate() {}

    onColorSchemeCategoryUpdate() {}

    onHighlightedFunctionUpdate() {}

    onSearchQueryUpdate() {}

    render() {
        let html = `
            <span>Color scheme: </span><a href="#" id="color-scheme-toggle">${ColorSchemeManager.getSelectedColorMode()}</a>
            <div id="color-scheme-panel">
        `;

        for (const colorMode of ColorSchemeManager.getColorModes()) {
            html += `
                <input
                    type="radio"
                    name="color-scheme-mode"
                    id="color-scheme-mode-${colorMode}"
                    value="${colorMode}"
                    ${colorMode === ColorSchemeManager.getSelectedColorMode() ? 'checked' : ''}
                >
                <label for="color-scheme-mode-${colorMode}">
                    ${colorMode}
                </label>
            `;
        }

        html += `
                <hr />
                <button id="color-scheme-new-category-button">Add new category</button>
                <ol></ol>
            </div>
        `;

        this.container.innerHTML = html;

        const toggle = this.container.querySelector('#color-scheme-toggle');
        const panel = this.container.querySelector('#color-scheme-panel');

        toggle.addEventListener('click', (e) => {
            e.preventDefault();

            panel.style.display =
                panel.style.display === 'block' ? 'none' : 'block';
        });

        document.addEventListener('click', (e) => {
            if (!this.container.contains(e.target)) {
                panel.style.display = 'none';
            }
        });

        this.container
            .querySelectorAll('input[name="color-scheme-mode"]')
            .forEach((radio) =>
                radio.addEventListener('change', (e) => {
                    if (!e.target.checked) {
                        return;
                    }

                    const label = panel.querySelector(
                        `label[for="${e.target.id}"]`
                    );

                    toggle.innerHTML = label.innerHTML;

                    this.notifyColorSchemeModeUpdate(e.target.value);
                })
            );

        const categoryList = this.container.querySelector('ol');

        const addCategoryToView = (category, prepend) => {
            const li = document.createElement('li');
            li.innerHTML = `
                <input name="color" type="color" value="${category.color}">
                <input type="text" name="label" value="${category.label}">
                <button name="push-up">⬆︎</button>
                <button name="push-down">⬇︎</button>
                <button name="delete">✖</button>
                <textarea name="patterns" placeholder="Patterns, one by line (substring/glob/regex)" style="height: 34px;">${category.patterns.join('\n')}</textarea>
            `;

            const indexInList = () =>
                Array.from(categoryList.children).indexOf(li);

            li.querySelector('button[name="push-up"]').addEventListener(
                'click',
                (e) => {
                    e.preventDefault();
                    e.stopPropagation();

                    const index = indexInList();
                    if (index <= 0) {
                        return;
                    }

                    const items = Array.from(categoryList.children);

                    categoryList.insertBefore(li, items[index - 1]);

                    const categories = ColorSchemeManager.getCustomCategories();
                    [categories[index - 1], categories[index]] = [
                        categories[index],
                        categories[index - 1],
                    ];

                    ColorSchemeManager.setCustomCategories(categories);
                }
            );

            li.querySelector('button[name="push-down"]').addEventListener(
                'click',
                (e) => {
                    e.preventDefault();
                    e.stopPropagation();

                    const index = indexInList();
                    const items = Array.from(categoryList.children);

                    if (index === -1 || index >= items.length - 1) {
                        return;
                    }

                    const next = items[index + 1];

                    categoryList.insertBefore(next, li);

                    const categories = ColorSchemeManager.getCustomCategories();
                    [categories[index], categories[index + 1]] = [
                        categories[index + 1],
                        categories[index],
                    ];

                    ColorSchemeManager.setCustomCategories(categories);
                }
            );

            li.querySelector('button[name="delete"]').addEventListener(
                'click',
                (e) => {
                    e.preventDefault();
                    e.stopPropagation();

                    const index = indexInList();
                    if (index !== -1) {
                        li.remove();
                        ColorSchemeManager.removeCustomCategory(index);
                    }
                }
            );

            li.querySelector('input[name="color"]').addEventListener(
                'input',
                (e) => {
                    const index = indexInList();
                    if (index !== -1) {
                        ColorSchemeManager.updateCustomCategory(
                            index,
                            (category) => (category.color = e.target.value)
                        );
                    }
                }
            );

            li.querySelector('input[name="label"]').addEventListener(
                'input',
                (e) => {
                    const index = indexInList();
                    if (index !== -1) {
                        ColorSchemeManager.updateCustomCategory(
                            index,
                            (category) => (category.label = e.target.value)
                        );
                    }
                }
            );

            li.querySelector('textarea[name="patterns"]').addEventListener(
                'input',
                (e) => {
                    const index = indexInList();
                    if (index !== -1) {
                        ColorSchemeManager.updateCustomCategory(
                            index,
                            (category) =>
                                (category.patterns = e.target.value.split('\n'))
                        );
                    }
                }
            );

            if (prepend) {
                categoryList.prepend(li);
            } else {
                categoryList.appendChild(li);
            }
        };

        for (const category of ColorSchemeManager.getCustomCategories()) {
            addCategoryToView(category);
        }

        document
            .getElementById('color-scheme-new-category-button')
            .addEventListener('click', (e) => {
                e.preventDefault();

                const newCategory = {
                    color: '#5a5a5a',
                    label: 'untitled',
                    patterns: [],
                };

                addCategoryToView(newCategory, true);

                const categories = ColorSchemeManager.getCustomCategories();
                categories.unshift(newCategory);
                ColorSchemeManager.setCustomCategories(categories);
            });
    }
}
