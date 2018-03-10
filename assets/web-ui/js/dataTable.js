
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
            html += `<th ${i == sort_col ? 'class="data_table-sort"' : ''}>${column.label}</th>`;
        }

        html += '</tr></thead><tbody>';

        rows.sort((a, b) => {
            a = getColumnValue(options.columns[sort_col].value, a);
            b = getColumnValue(options.columns[sort_col].value, b);

            return (a < b ? -1 : (a > b)) * sort_dir;
        });

        for (let row of rows) {
            let url = options.makeRowUrl ? options.makeRowUrl(row) : null;
            html += '<tr>';
            for (let column of options.columns) {
                let value = getColumnValue(column.value, row);
                if (column.format) {
                    value = column.format(value);
                }

                if (url) {
                    value = `<a href="${url}">${value}</a>`;
                }

                html += `<td class="${column.cssClass || ''}">${value}</td>`;
            }

            html += '</tr>';
        }

        html += '</tbody></table>';

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
