
import {round} from './math.js';

export function lpad(str, len, char) {
    str = str + '';
    let d = len - str.length;
    if (d <= 0) {
        return str;
    }

    return char.repeat(d) + str;
}

export function date(d) {
    return d.getFullYear()
        + '-' + lpad(d.getMonth() + 1, 2, '0')
        + '-' + lpad(d.getDate(), 2, '0')
        + ' ' + lpad(d.getHours(), 2, '0')
        + ':' + lpad(d.getMinutes(), 2, '0')
        + ':' + lpad(d.getSeconds(), 2, '0')
    ;
}

export function quantity(n) {
    if (n >= 1000 * 1000 * 1000) {
        return round(n / (1000 * 1000 * 1000), 2).toFixed(2) + 'G';
    }

    if (n >= 1000 * 1000) {
        return round(n / (1000 * 1000), 2).toFixed(2) + 'M';
    }

    if (n >= 1000) {
        return round(n / 1000, 2).toFixed(2) + 'K';
    }

    return round(n, 0);
}

export function pct(n) {
    return round(n * 100, 2).toFixed(2) + '%';
}

export function time(n) {
    if (n >= 1000 * 1000 * 1000) {
        return round(n / (1000 * 1000 * 1000), 2).toFixed(2) + 's';
    }

    if (n >= 1000 * 1000) {
        return round(n / (1000 * 1000), 2).toFixed(2) + 'ms';
    }

    if (n >= 1000) {
        return round(n / (1000), 2).toFixed(2) + 'us';
    }

    return round(n, 0) + 'ns';
}

export function memory(n) {
    const abs = Math.abs(n);

    if (abs >= (1 << 30)) {
        return round(n / (1 << 30), 2).toFixed(2) + 'GB';
    }

    if (abs >= (1 << 20)) {
        return round(n / (1 << 20), 2).toFixed(2) + 'MB';
    }

    if (abs >= (1 << 10)) {
        return round(n / (1 << 10), 2).toFixed(2) + 'KB';
    }

    return round(n, 0) + 'B';
}
