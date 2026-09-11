<?php

/**
 * Checks that every source file starts with the GPL header, with an up to date
 * copyright year.
 *
 * Usage:
 *     php .github/check-license-headers.php [--fix]
 */

chdir(dirname(__DIR__));

const HEADER_TEMPLATE_FILE = '.github/license-header.txt';
const YEAR_PLACEHOLDER = '%YEAR%';
const YEAR_SENTINEL = '0000';

function render_header(array $lines, $style)
{
    if ($style === 'html') {
        return "<!--\n" . implode("\n", $lines) . "\n-->\n";
    }

    $header = ($style === 'banner' ? '/*! ' : '/* ') . $lines[0] . "\n";
    foreach (array_slice($lines, 1) as $line) {
        $header .= rtrim(' * ' . $line) . "\n";
    }

    return $header . " */\n";
}

function get_style($file)
{
    if (substr($file, -5) === '.html') {
        return 'html';
    }

    if ($file === 'assets/web-ui/v2/js/spx.js') {
        return 'banner';
    }

    return 'block';
}

function get_expected_year($file)
{
    $year = trim(shell_exec(sprintf(
        'git log -1 --format=%%ad --date=format:%%Y -- %s',
        escapeshellarg($file)
    )));

    return $year === '' ? date('Y') : $year;
}

$fix = in_array('--fix', $argv, true);
$templateLines = file(HEADER_TEMPLATE_FILE, FILE_IGNORE_NEW_LINES);

$files = [];
exec("git ls-files '*.c' '*.h' '*.js' '*.css' '*.html'", $files);

$checkedFileCount = 0;
$failures = [];
$fixedFiles = [];

foreach ($files as $file) {
    $checkedFileCount++;

    $style = get_style($file);
    $expectedYear = get_expected_year($file);
    $expectedHeader = render_header(
        str_replace(YEAR_PLACEHOLDER, $expectedYear, $templateLines),
        $style
    );

    $anyYearHeaderPattern = '#^' . str_replace(
        YEAR_SENTINEL,
        '\d{4}',
        preg_quote(
            render_header(
                str_replace(YEAR_PLACEHOLDER, YEAR_SENTINEL, $templateLines),
                $style
            ),
            '#'
        )
    ) . '#';

    $content = file_get_contents($file);
    $preamble = '';
    if ($style === 'html' && preg_match('#^<!DOCTYPE[^\n]*\n#i', $content, $matches)) {
        $preamble = $matches[0];
        $content = substr($content, strlen($preamble));
    }

    if (strpos($content, $expectedHeader) === 0) {
        continue;
    }

    if (!preg_match($anyYearHeaderPattern, $content, $matches)) {
        $failures[] = $file . ': missing or altered license header';

        continue;
    }

    if (!$fix) {
        $failures[] = $file . ': outdated copyright year, expected 2017-' . $expectedYear;

        continue;
    }

    file_put_contents(
        $file,
        $preamble . $expectedHeader . substr($content, strlen($matches[0]))
    );

    $fixedFiles[] = $file;
}

foreach ($fixedFiles as $file) {
    echo 'Fixed copyright year: ', $file, "\n";
}

if ($failures) {
    fwrite(STDERR, "License header check failed:\n  " . implode("\n  ", $failures) . "\n");
    fwrite(STDERR, "\nRun: php .github/check-license-headers.php --fix\n");

    exit(1);
}

echo 'License header check passed on ', $checkedFileCount, " files\n";
