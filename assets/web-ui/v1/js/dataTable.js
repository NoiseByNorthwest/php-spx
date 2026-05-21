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



export function makeDataTable(containerId, options, rows) {
    let sort_col = 0;
    let sort_dir = -1;

    function getColumnValue(accessor, row) {
        if ($.type(accessor) === 'function') {
            return accessor(row);
        }

        return row[accessor];
    }

    let container = $('#' + containerId);
    let render = () => {
        let html = '<table class="data_table"><thead><tr>';

        for (let i = 0; i < options.columns.length; i++) {
            let column = options.columns[i];
            html += `<th${i == sort_col ? ' class="data_table-sort"' : ''}>${column.label}</th>`;
        }

        html += '</tr></thead><tbody>';

        rows.sort((a, b) => {
            a = getColumnValue(options.columns[sort_col].value, a);
            b = getColumnValue(options.columns[sort_col].value, b);

            return (a < b ? -1 : (a > b)) * sort_dir;
        });

        for (let row of rows) {
            let url = options.makeRowUrl ? options.makeRowUrl(row) : null;
            html += `<tr${row.key ? ` id="row-${row.key}"` : ''}>`;
            for (let column of options.columns) {
                let value = getColumnValue(column.value, row);
                if (column.format) {
                    value = column.format(value);
                }

                if (url && !column.skipUrl) {
                    value = `<a href="${url}">${value}</a>`;
                }

                html += `<td${column.cssClass ? ` class="${column.cssClass}"` : ''}>${value}</td>`;
            }

            html += '</tr>';
        }

        html += '</tbody>';

        if (rows.length && options.footer) {
          html += '<tfoot><tr>';
          for (const footer of options.footer) {
            html += `<td${footer.colspan ? ` colspan="${footer.colspan}"` : ''}>`;
            html += footer.value;
            html += '</td>';
          }
          html += '</tr></tfoot>';
        }

        html += '</table>';

        container.append(html);
        container.find('th').click(e => {
            let current = $(e.target).index();
            if (sort_col == current) {
                sort_dir *= -1;
            }

            sort_col = current;

            container.empty();
            render();
        });
    }

    render();
}
