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

    function renderRowActionBtn(action, i, row) {
        return `<a class="data_table-action-btn${action.cssClass ? ` ${action.cssClass}` : ''}" href="${action.href ? action.href(row) : '#'}" data-action-index="${i}" data-row-key="${row.key}"${action.title ? ` title="${action.title}"` : ''}>${action.label}</a>`;
    }

    function renderRowActions(row) {
        let html = '';
        for (let i = 0; i < options.rowActions.length; ) {
            const group = options.rowActions[i].group;
            if (!group) {
                html += renderRowActionBtn(options.rowActions[i], i, row);
                i++;
                continue;
            }

            let itemsHtml = '';
            for (
                ;
                i < options.rowActions.length &&
                options.rowActions[i].group === group;
                i++
            ) {
                itemsHtml += renderRowActionBtn(options.rowActions[i], i, row);
            }

            html += `<details class="data_table-action-group"><summary class="data_table-action-btn">${group}</summary><div class="data_table-action-menu">${itemsHtml}</div></details>`;
        }

        return html;
    }

    let openActionGroup = null;
    let actionGroupCloseTimeout = null;

    function closeOpenActionGroup() {
        clearTimeout(actionGroupCloseTimeout);
        actionGroupCloseTimeout = null;
        openActionGroup?.removeAttribute('open');
        openActionGroup = null;
    }

    const container = document.getElementById(containerId);
    let render = () => {
        closeOpenActionGroup();

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
            const headerClasses = [
                column.cssClass,
                i == sort_col ? 'data_table-sort' : null,
            ].filter((e) => e);

            html += `<th${headerClasses.length ? ` class="${headerClasses.join(' ')}"` : ''}>${column.label}</th>`;
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
                    value = column.format(value, row);
                }

                if (value === null || value === undefined) {
                    value = '';
                }

                if (url) {
                    value = `<a href="${url}">${value}</a>`;
                }

                html += `<td${column.cssClass ? ` class="${column.cssClass}"` : ''}>${value}</td>`;
            }

            if (hasRowActions) {
                html += `<td class="data_table-actions">${renderRowActions(row)}</td>`;
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
            const actionGroup = e.target.closest(
                'details.data_table-action-group'
            );
            if (actionGroup !== openActionGroup) {
                closeOpenActionGroup();
            }

            if (actionGroup && e.target.closest('summary')) {
                // The native toggle runs after this handler.
                openActionGroup = actionGroup.open ? null : actionGroup;
            }

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
                closeOpenActionGroup();
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

    if (hasRowActions) {
        document.addEventListener('click', (e) => {
            if (!e.target.closest('details.data_table-action-group')) {
                closeOpenActionGroup();
            }
        });

        container.addEventListener('pointerover', (e) => {
            const actionGroup = e.target.closest(
                'details.data_table-action-group'
            );
            if (actionGroup && actionGroup === openActionGroup) {
                clearTimeout(actionGroupCloseTimeout);
                actionGroupCloseTimeout = null;
            }
        });

        container.addEventListener('pointerout', (e) => {
            const actionGroup = e.target.closest(
                'details.data_table-action-group'
            );
            if (
                !actionGroup ||
                actionGroup !== openActionGroup ||
                actionGroup.contains(e.relatedTarget)
            ) {
                return;
            }

            actionGroupCloseTimeout = setTimeout(closeOpenActionGroup, 400);
        });
    }
}
