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

import { confirm } from './confirmDialog.js';

export function makeDataTable(containerId, options, rows) {
    const hasRowActions = options.rowActions?.length > 0;
    const hasTableActions = options.tableActions?.length > 0;
    const totalCols = options.columns.length + (hasRowActions ? 1 : 0);

    let sort_col = 0;
    let sort_dir = -1;

    function getColumnValue(accessor, row) {
        if (typeof accessor === 'function') {
            return accessor(row);
        }

        return row[accessor];
    }

    const container = document.getElementById(containerId);
    let render = () => {
        let html = '';

        if (rows.length && hasTableActions) {
            const actionsHtml = options.tableActions
                .map(
                    (action, i) =>
                        `<a class="${action.cssClass || ''}" href="${action.href}" data-table-action-index="${i}">${action.label}</a>`
                )
                .join(' ');
            html += `<div class="data_table-toolbar">${actionsHtml}</div>`;
        }

        html += '<table class="data_table"><thead><tr>';

        for (let i = 0; i < options.columns.length; i++) {
            let column = options.columns[i];
            html += `<th ${i == sort_col ? 'class="data_table-sort"' : ''}>${column.label}</th>`;
        }

        if (hasRowActions) {
            html += '<th>Actions</th>';
        }

        html += '</tr></thead><tbody>';

        rows.sort((a, b) => {
            a = getColumnValue(options.columns[sort_col].value, a);
            b = getColumnValue(options.columns[sort_col].value, b);

            return (a < b ? -1 : a > b) * sort_dir;
        });

        for (let row of rows) {
            let url = options.makeRowUrl ? options.makeRowUrl(row) : null;
            html += `<tr${row.key ? ` id="row-${row.key}"` : ''}>`;
            for (let column of options.columns) {
                let value = getColumnValue(column.value, row);
                if (column.format) {
                    value = column.format(value);
                }

                if (url) {
                    value = `<a href="${url}">${value}</a>`;
                }

                html += `<td${column.cssClass ? ` class="${column.cssClass}"` : ''}>${value}</td>`;
            }

            if (hasRowActions) {
                const actionsHtml = options.rowActions
                    .map(
                        (action, i) =>
                            `<a class="data_table-action-btn${action.cssClass ? ` ${action.cssClass}` : ''}" href="${action.href ? action.href(row) : '#'}" data-action-index="${i}" data-row-key="${row.key}"${action.title ? ` title="${action.title}"` : ''}>${action.label}</a>`
                    )
                    .join(' ');
                html += `<td class="data_table-actions">${actionsHtml}</td>`;
            }

            html += '</tr>';
        }

        html += '</tbody></table>';

        container.insertAdjacentHTML('beforeend', html);
        container.querySelectorAll('th').forEach((th) => {
            th.addEventListener('click', (e) => {
                const current = Array.from(th.parentNode.children).indexOf(th);
                if (current >= options.columns.length) {
                    return;
                }

                if (sort_col === current) {
                    sort_dir *= -1;
                }

                sort_col = current;

                container.innerHTML = '';
                render();
            });
        });
    };

    render();

    if (hasRowActions || hasTableActions) {
        container.addEventListener('click', async (e) => {
            const tableActionEl = e.target.closest('[data-table-action-index]');
            if (tableActionEl) {
                e.preventDefault();
                const action =
                    options.tableActions[
                        tableActionEl.dataset.tableActionIndex
                    ];
                if (action.confirm) {
                    const { title, message, confirmLabel, cancelLabel } =
                        action.confirm();
                    if (
                        !(await confirm(
                            title,
                            message,
                            confirmLabel,
                            cancelLabel
                        ))
                    ) {
                        return;
                    }
                }
                const response = await fetch(tableActionEl.href, {
                    credentials: 'same-origin',
                });
                if (response.ok) {
                    action.onSuccess(container);
                }
                return;
            }

            const rowActionEl = e.target.closest('[data-action-index]');
            if (rowActionEl) {
                e.preventDefault();
                const action =
                    options.rowActions[rowActionEl.dataset.actionIndex];
                const row = rows.find(
                    (r) => String(r.key) === rowActionEl.dataset.rowKey
                );
                if (action.handler) {
                    action.handler(row);
                    return;
                }
                if (action.confirm) {
                    const { title, message, confirmLabel, cancelLabel } =
                        action.confirm(row);
                    if (
                        !(await confirm(
                            title,
                            message,
                            confirmLabel,
                            cancelLabel
                        ))
                    ) {
                        return;
                    }
                }
                const response = await fetch(rowActionEl.href, {
                    credentials: 'same-origin',
                });
                if (response.ok) {
                    action.onSuccess(row, container);
                }
            }
        });
    }
}
