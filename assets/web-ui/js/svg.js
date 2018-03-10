
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

        let node = this.nodes[this.top];
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
