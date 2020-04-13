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



export function round(n, d) {
    const scale = Math.pow(10, d || 0);

    return Math.round(n * scale) / scale;
}

export function bound(a, low, up) {
    return Math.max(low || 0, Math.min(a, up || 1));
}

export function lerp(a, b, dist) {
    dist = bound(dist);

    return a * (1 - dist) + b * dist;
}

export function lerpDist(a, b, value) {
    return (value - a) / (b - a);
}

export function dist(a, b) {
    return Math.abs(a - b);
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

    mult(v) {
        this.x *= v;
        this.y *= v;
        this.z *= v;

        return this;
    }

    toHTMLColor() {
        const c = this.copy().bound();

        return 'rgb(' + [
            parseInt(c.x * 255),
            parseInt(c.y * 255),
            parseInt(c.z * 255),
        ].join(',') + ')';
    }

    static createFromHTMLColor(htmlColor) {
        const matches = /rgb\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)/.exec(htmlColor);

        return this.createFromRGB888(
            matches[1],
            matches[2],
            matches[3]
        );
    }

    static createFromRGB888(r, g, b) {
        const v = new Vec3(
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

        const span = 1 / (vectors.length - 1);
        const firstIdx = bound(parseInt(dist / span), 0, vectors.length - 2);

        return this.lerp(
            vectors[firstIdx],
            vectors[firstIdx + 1],
            (dist - firstIdx * span) / span
        );
    }
}

export class Range {

    constructor(begin, end) {
        if (begin > end) {
            throw new Error('Invalid range: ' + begin + ' ' + end);
        }

        this.begin = begin;
        this.end = end;
    }

    copy() {
        return new Range(this.begin, this.end);
    }

    length() {
        return this.end - this.begin;
    }

    center() {
        return (this.begin + this.end) / 2;
    }

    lerp(dist) {
        return lerp(this.begin, this.end, dist);
    }

    lerpDist(value) {
        return lerpDist(this.begin, this.end, value);
    }

    intersect(other) {
        if (!this.overlaps(other)) {
            throw new Error('Ranges do not overlap');
        }

        this.begin = Math.max(this.begin, other.begin);
        this.end = Math.min(this.end, other.end);

        return this;
    }

    subRange(ratio, num) {
        const width = ratio * this.length();

        return new Range(
            Math.max(this.begin, this.begin + width * num),
            Math.min(this.end, this.begin + width * (num + 1))
        );
    }

    scale(factor) {
        this.begin *= factor;
        this.end *= factor;

        return this;
    }

    shift(dist) {
        this.begin += dist;
        this.end += dist;

        return this;
    }

    shiftBegin(dist) {
        this.begin = Math.min(this.begin + dist, this.end);

        return this;
    }

    shiftEnd(dist) {
        this.end = Math.max(this.end + dist, this.begin);

        return this;
    }

    bound(low, up) {
        low = low || 0;
        up = up || 1;

        this.begin = bound(this.begin, low, up);
        this.end = bound(this.end, low, up);

        if (this.begin > this.end) {
            this.begin = low;
            this.end = up;
        }

        return this;
    }

    containsValue(val) {
        return this.begin <= val && val <= this.end;
    }

    contains(other) {
        return this.begin <= other.begin && other.end <= this.end;
    }

    isContainedBy(other) {
        return other.contains(this);
    }

    overlaps(other) {
        return !(this.end < other.begin || other.end < this.begin);
    }

    sub(other) {
        if (other.contains(this)) {
            this.end = this.begin;
            return this;
        }

        if (this.contains(other)) {
            this.end -= other.length();
        }

        if (!this.overlaps(other)) {
            return this;
        }

        if (this.containsValue(other.begin)) {
            this.end = other.begin;
        } else if (this.containsValue(other.end)) {
            this.begin = other.end;
        }

        return this;
    }
}
