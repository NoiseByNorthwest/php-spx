
export function round(n, d) {
    let scale = Math.pow(10, d || 0);

    return Math.round(n * scale) / scale;
}

export function bound(a, low, up) {
    return Math.max(low || 0, Math.min(a, up || 1));
}

export function lerp(a, b, dist) {
    dist = bound(dist);

    return a * (1 - dist) + b * dist;
}

export class Vec3 {

    constructor(x, y, z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    copy() {
        return new Vec3(
            this.x,
            this.y,
            this.z
        );
    }

    bound(low, up) {
        bound(this.x, low, up);
        bound(this.y, low, up);
        bound(this.z, low, up);

        return this;
    }

    toHTMLColor() {
        let c = this.copy().bound();

        return 'rgb(' + [
            parseInt(c.x * 255),
            parseInt(c.y * 255),
            parseInt(c.z * 255),
        ].join(',') + ')';
    }

    static createFromRGB888(r, g, b) {
        let v = new Vec3(
            r / 255,
            g / 255,
            b / 255,
        );

        return v.bound();
    }

    static lerp(a, b, dist) {
        return new Vec3(
            lerp(a.x, b.x, dist),
            lerp(a.y, b.y, dist),
            lerp(a.z, b.z, dist)
        );
    }

    static lerpPath(vectors, dist) {
        dist = bound(dist);

        let span = 1 / (vectors.length - 1);
        let firstIdx = Math.min(vectors.length - 2, parseInt(dist / span));

        return this.lerp(
            vectors[firstIdx],
            vectors[firstIdx + 1],
            (dist - firstIdx * span) / span
        );
    }
}

export class Range {

    constructor(a, b) {
        if (a > b) {
            throw new Error('Invalid range: ' + a + ' ' + b);
        }

        this.a = a;
        this.b = b;
    }

    copy() {
        return new Range(this.a, this.b);
    }

    length() {
        return this.b - this.a;
    }

    center() {
        return (this.a + this.b) / 2;
    }

    intersection(other) {
        if (!this.overlaps(other)) {
            throw new Error('Ranges do not overlap');
        }

        return new Range(
            Math.max(this.a, other.a),
            Math.min(this.b, other.b)
        );
    }

    subRange(ratio, num) {
        let width = ratio * this.length();

        return new Range(
            Math.max(this.a, this.a + width * num),
            Math.min(this.b, this.a + width * (num + 1))
        );
    }

    scale(factor) {
        this.a *= factor;
        this.b *= factor;

        return this;
    }

    shift(dist) {
        this.a += dist;
        this.b += dist;

        return this;
    }

    bound(low, up) {
        low = low || 0;
        up = up || 1;

        this.a = bound(this.a, low, up);
        this.b = bound(this.b, low, up);

        if (this.a > this.b) {
            this.a = low;
            this.b = up;
        }

        return this;
    }

    contains(other) {
        return this.a <= other.a && other.b <= this.b;
    }

    isContainedBy(other) {
        return other.contains(this);
    }

    overlaps(other) {
        return !(this.b < other.a || other.b < this.a);
    }
}
