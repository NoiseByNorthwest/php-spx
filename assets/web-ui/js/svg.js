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



export function createNode(name, attributes, builder) {
    let node = document.createElementNS("http://www.w3.org/2000/svg", name);
    for (let k in attributes || {}) {
        node.setAttribute(k, attributes[k]);
    }

    if (builder) {
        builder(node);
    }

    return node;
}

export class NodePool {

    constructor(name) {
        this.name = name;
        this.nodes = [];
        this.top = 0;
    }

    acquire(attributes, builder) {
        if (this.nodes.length == this.top) {
            this.nodes.push(createNode(this.name));
        }

        const node = this.nodes[this.top];
        this.top++;

        for (let k in attributes || {}) {
            node.setAttribute(k, attributes[k]);
        }

        if (builder) {
            builder(node);
        }

        return node;
    }

    releaseAll() {
        this.top = 0;
    }
}
