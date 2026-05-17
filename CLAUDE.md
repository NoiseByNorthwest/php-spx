# CLAUDE.md — php-spx

PHP profiler shipped as a C extension with an integrated Web UI. This file documents the
*systematic* conventions observed in the codebase. Anything not listed here is, by default,
an isolated case.

## High-level architecture

```
src/                     PHP extension in C (compiles to spx.so)
tests/                   PHPT tests (standard PHP run-tests.php)
assets/web-ui/v1/        Legacy Web UI (jQuery) — frozen, no build step
assets/web-ui/v2/        Next-gen Web UI (ESM, esbuild + terser)
build/                   m4 macros + autotools scripts
.github/workflows/       CI matrix (Linux/Debian/macOS × PHP 7.0..8.5)
```

The Web UI HTTP server lives in `src/php_spx.c::http_ui_handle_request` and serves files
from `SPX_HTTP_UI_ASSETS_DIR` (defined at `configure` time). Reports are stored in
`spx.data_dir` (`/tmp/spx` by default).

## C code — naming conventions

All conventions below are **systematic** across `src/`:

- **Files**: `spx_<module>.c` / `spx_<module>.h`. OS-specific variants are suffixed:
  `spx_resource_stats-linux.c`, `spx_stdio-unix.c`. Selection happens via `#if defined(__linux__)`
  etc. in the parent `.c` (see `src/spx_resource_stats.c`).
- **Public symbols**: `spx_<module>_<verb>(...)`. Examples: `spx_hmap_create`,
  `spx_metric_collector_destroy`, `spx_utils_str_starts_with`.
- **Private symbols**: `static`, no `spx_` prefix. Examples in `src/php_spx.c`:
  `profiling_handler_init`, `http_ui_handle_static_file`, `check_access`.
- **Types**: `_t` suffix. Opaque structs are forward-declared in the `.h` and defined in
  the `.c` (`spx_str_builder_t`, `spx_output_stream_t`, `spx_metric_collector_t`).
- **Enums**: `SPX_<MODULE>_<VALUE>` in UPPERCASE. Sentinel members `_COUNT` then `_NONE`
  (see `src/spx_metric.h::spx_metric_t`):

  ```c
  typedef enum {
      SPX_METRIC_WALL_TIME,
      /* ... */
      SPX_METRIC_COUNT,
      SPX_METRIC_NONE,
  } spx_metric_t;
  ```
- **Macros**: `SPX_<MODULE>_<NAME>` in UPPERCASE. Always wrapped in `do { ... } while (0)`.
  Examples: `SPX_METRIC_FOREACH`, `SPX_UTILS_TOKENIZE_STRING`.
- **Header guards**: `#ifndef SPX_<MODULE>_H_DEFINED`. The `/* SPX_<MODULE>_H_DEFINED */`
  comment is repeated on the closing `#endif`:

  ```c
  #ifndef SPX_UTILS_H_DEFINED
  #define SPX_UTILS_H_DEFINED
  /* ... */
  #endif /* SPX_UTILS_H_DEFINED */
  ```

### Declaration order in a `.c` file

1. GPL-3.0 license header (16 lines, `2017-<year>` copyright). Convention respected
   across every C, JS, and HTML file — not enforced by CI, applied by hand.
2. System `#include`s (stdio, stdlib, string, …)
3. PHP `#include`s (`main/php.h`, `Zend/zend_observer.h`, …)
4. SPX `#include`s (`spx_*.h`)
5. Private `#define`s
6. Private `typedef struct { ... }`
7. Grouped `static` prototypes
8. Public functions, then private functions

### Polymorphism via "base struct"

Systematic pattern for the `profiler` and `reporter` hierarchies (C-style inheritance):

```c
/* spx_profiler.h — interface */
typedef struct spx_profiler_t {
    void (*call_start)(struct spx_profiler_t * profiler, const spx_php_function_t * function);
    void (*call_end)(struct spx_profiler_t * profiler);
    void (*finalize)(struct spx_profiler_t * profiler);
    void (*destroy)(struct spx_profiler_t * profiler);
} spx_profiler_t;

/* spx_profiler_tracer.c — implementation */
typedef struct {
    spx_profiler_t base;        /* first field — direct cast */
    /* implementation-specific fields... */
} tracing_profiler_t;

static void tracing_profiler_call_start(spx_profiler_t * base_profiler, ...) {
    tracing_profiler_t * profiler = (tracing_profiler_t *) base_profiler;  /* downcast */
    /* ... */
}
```

See also `src/spx_profiler.h::spx_profiler_reporter_t` and its 3 implementations
(`src/spx_reporter_full.c`, `src/spx_reporter_fp.c`, `src/spx_reporter_trace.c`).

### Error handling — systematic patterns

- **`goto error` pattern** in factories / constructors: centralized cleanup on allocation
  failure. The `error:` label calls the matching `destroy`.

  ```c
  spx_profiler_t * spx_profiler_tracer_create(...) {
      tracing_profiler_t * profiler = malloc(sizeof(*profiler));
      if (!profiler) goto error;
      /* ... initialization ... */
      profiler->func_table.hmap = spx_hmap_create(...);
      if (!profiler->func_table.hmap) goto error;

      return (spx_profiler_t *) profiler;

  error:
      if (profiler) tracing_profiler_destroy((spx_profiler_t *) profiler);
      return NULL;
  }
  ```

  (see `src/spx_profiler_tracer.c::spx_profiler_tracer_create`)

- **Fatal error**: `spx_utils_die("message")` (macro injecting `__FILE__`/`__LINE__`).
  Implemented in `src/spx_utils.c::spx_utils_die_` — `pthread_exit(NULL)` under ZTS,
  `exit(EXIT_FAILURE)` otherwise. Reserved for unreachable invariants (buffer too small,
  etc.).

- **PHP-side logging**: `spx_php_log_notice("fmt", ...)` (PHP `E_NOTICE`, not fatal).
  Used for any error visible to the PHP user. Message convention: `"<function>(): <reason>"`:

  ```c
  spx_php_log_notice("spx_profiler_start(): profiling is not enabled");
  ```

- **Return codes**: `int` 0 = success / non-zero = failure in HTTP helpers
  (`http_ui_handle_data_request`, `http_ui_handle_static_file`).
  For predicate helpers (`spx_utils_ip_match`, `spx_utils_str_starts_with`),
  the convention is reversed: 1 = true, 0 = false.

- **Allocation**: always `malloc(sizeof(*ptr))` (never `sizeof(struct_t)`) so the call
  stays correct after a type rename:

  ```c
  spx_hmap_t * hmap = malloc(sizeof(*hmap));
  ```

  (see `src/spx_hmap.c::spx_hmap_create`)

### Variadic multi-source configuration

`spx_config_get(config, cli, source1, source2, ..., -1)` — the source list is terminated
by `-1` (sentinel). Each source overrides the previous one for the parameters it defines.
Used for cookie/header/query string in HTTP, env-only in CLI:

```c
spx_config_get(
    &context.config,
    context.cli_sapi,
    SPX_CONFIG_SOURCE_HTTP_COOKIE,
    SPX_CONFIG_SOURCE_HTTP_HEADER,
    SPX_CONFIG_SOURCE_HTTP_QUERY_STRING,
    -1
);
```

See `src/spx_config.c::spx_config_get` and the caller `src/php_spx.c::PHP_RINIT_FUNCTION`.

### TLS and ZTS

Every runtime-scoped global uses `SPX_THREAD_TLS` (see `src/spx_thread.h`).
`SPX_THREAD_TLS` resolves to `__thread` (POSIX), `__declspec(thread)` (Windows MSVC),
or nothing (NTS):

```c
static SPX_THREAD_TLS struct { ... } context;
```

(see `src/php_spx.c::context`)

The `zend_execute_ex` / `zend_execute_internal` Zend hooks are replaced by the Observer
API (PHP 8.2+) when `spx.use_observer_api=1` (the default). The legacy path remains
mandatory under ZTS for compatibility (see `src/php_spx.c::PHP_MINIT_FUNCTION`).

### Iterating over metrics

Macros `SPX_METRIC_FOREACH(it, block)` (all metrics) and `SPX_METRIC_FOREACH_L(it, limit, block)`
(enabled metrics). Always preferred over hand-written `for` loops, e.g.:

```c
SPX_METRIC_FOREACH(i, {
    config->metric_settings[i] = 0;
});
```

(see `src/spx_config.c::init_config`)

### Designated array initialization

Info tables (`spx_metric_info[]`) use `[INDEX] = { ... }` via the `ARRAY_INIT_INDEX` macro
in `src/spx_metric.c`. Explicit safety net `#error "Please open an issue"` if the compiler
isn't GCC:

```c
#ifdef __GNUC__
#   define ARRAY_INIT_INDEX(idx) [idx] =
#else
#   error "Please open an issue"
#endif
```

## Tests — PHPT layout

Standard PHP testing format. Observed conventions:

### Naming

- **Generic cases**: `spx_NNN.phpt` (`spx_004.phpt` through `spx_013.phpt`, except `spx_008.phpt` which is absent).
- **Topic-based**: `spx_<topic>_<sub-case>.phpt`. Prefixes:
  - `spx_auth_*` (IP/key/proxy auth, `_ok`/`_ko` suffixes)
  - `spx_auto_start_NNN[_observer_api].phpt`
  - `spx_handle_ui_request_<ko|ok>_<reason>.phpt`
  - `spx_ini_params_*`
  - `spx_full_report_*`, `spx_log_*`, `spx_ui_*`
- **Per-PHP-version variants**: the suffix encodes the version range. Several forms coexist:
  - `_X_Y+` / `_X_Y-` — `>= X.Y` / `<= X.Y` (`_7_0+`, `_7_3+`, `_8_0+`, `_8_2+`, `_8_3-`,
    `_8_4+`, `_8_1-`).
  - `_X_Y` — exact `X.Y` (`_7_0`, `_7_1`, `_7_2`, `_7_3`, `_7_4`).
  - `_X_Y_X_Z` — closed range `X.Y..X.Z` (`_userland_stats_7_0_7_2`).
  - `_X.Y` / `_X.Y+` with a literal dot — only used by `spx_memory_001_*` (`_7.0`, `_7.1+`,
    `_7.4`). Inconsistent with the dominant underscore form, but kept as-is.
  - The Observer-API variants append `_observer_api` *after* the version suffix
    (`_8_0+_observer_api`, `_8_2+_observer_api`).

  All variants are filtered via the `--SKIPIF--` section:

  ```
  --SKIPIF--
  <?php
  if (version_compare(PHP_VERSION, '8.0') < 0) {
      die('skip this test is for PHP 8.0+ only');
  }
  ?>
  ```
- **Observer API variants**: `_observer_api.phpt` suffix; the others force the legacy path
  via `spx.use_observer_api=0` in `--INI--`.

### Common sections

| Section | Use |
|---|---|
| `--TEST--` | Short description (1 line) |
| `--SKIPIF--` | PHP-version filtering |
| `--INI--` | `spx.*` configuration + `log_errors=on` |
| `--ENV--` | `SPX_*` variables (CLI) — use `return <<<END ... END;` |
| `--CGI--` | Forces CGI / HTTP SAPI mode |
| `--GET--` / `--FILE--` | HTTP inputs / PHP script |
| `--EXPECT--` | Exact match |
| `--EXPECTF--` | Match with wildcards `%s`, `%d`, `%S`, `%w` |
| `--EXPECTHEADERS--` | Asserts HTTP response headers |

Minimal CLI example (`tests/spx_005.phpt`):

```
--TEST--
Explicitly disabled
--ENV--
return <<<END
SPX_ENABLED=0
END;
--FILE--
<?php
echo 'Normal output';
?>
--EXPECT--
Normal output
```

HTTP example (`tests/spx_auth_ip_ok.phpt`):

```
--CGI--
--INI--
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1,127.0.0.2,127.0.0.3"
spx.http_ui_assets_dir="{PWD}/../assets/web-ui"
log_errors=on
```

`{PWD}` is resolved by `run-tests.php`. Tests that touch the Web UI set
`spx.http_ui_assets_dir="{PWD}/../assets/web-ui"` (or `.../web-ui/v2` when the test
specifically targets v2, see `spx_ui_uri_confinement_check_ok.phpt`).

### Automatic bypass for `.skip.php` scripts

`src/spx_config.c::finalize_config` explicitly disables SPX when `SCRIPT_FILENAME` matches
`/tests/spx_*.skip.php` — do not break this:

```c
if (script_file_name
    && strstr(script_file_name, "/tests/spx_")
    && spx_utils_str_ends_with(script_file_name, ".skip.php")) {
    config->enabled = 0;
}
```

### Running the tests

```sh
make test                                     # all
TESTS=tests/spx_006.phpt make test            # single test
TEST_PHP_ARGS="--show-diff" NO_INTERACTION=1 make test
```

## Web UI v2 — JavaScript

Native ESM, **no framework**, no transpilation in dev. In dev the browser loads the
modules directly (`build-dev.sh` runs esbuild in watch mode with `--define:DEBUG=true`,
no minification); release goes through esbuild + terser.

### Layout

```
js/main.js                    Bundle entry — re-exports flat modules as namespaces
js/<module>.js                Flat modules (utils, fmt, math, svg, dataTable,
                              profileDataAnalyzer, colorSchemeManager, searchManager,
                              layoutSplitter)
js/widget.js                  Re-exports concrete widget classes by name (used by
                              report.html)
js/widget/widget.js           `Widget` base class + ViewPort/ViewTimeRange + shared
                              `renderSVG*()` helpers
js/widget/svgWidget.js        `SVGWidget` second-level base (extends Widget,
                              owns a ViewPort)
js/widget/<widgetName>.js     One concrete widget class per file (FlameGraph,
                              TimeLine, OverView, FlatProfile, SearchBox,
                              ColorSchemeSelector)
css/main.css                  Single stylesheet — CSS variables + a
                              `prefers-color-scheme: light` override for the
                              built-in dark/light theme switch
```

Two structural CSS classes are applied in HTML:
- `.widget` on **every** widget container — gives the dark border + hover highlight.
- `.visualization` only on SVG widgets (`#overview`, `#timeline`, `#flamegraph`) —
  applies `user-select: none` to keep drag/zoom from selecting text.

`main.js` re-exports **modules** as namespaces:

```js
import * as utils from './utils.js';
import * as widget from './widget.js';
/* ... */
export { utils, fmt, profileDataAnalyzer, widget, dataTable, layoutSplitter };
```

`js/widget.js` (top-level, distinct from `js/widget/widget.js`) re-exports the **widget
classes** by name. HTML pages then pick whichever entry they need (see `index.html` vs
`report.html`).

The bundle (`spx.js`, with the source map **inlined as a `data:` URL at the end of the
file** via terser's `--source-map url=inline`) is committed — there is no separate
`spx.js.map`. CI fails if `format.sh` or `build-release.sh` produces a diff — see the
`check-js` job in `.github/workflows/main.yml`.

### Naming — files, classes, members

Systematic across `assets/web-ui/v2/js/`:

| Kind | Convention | Examples |
|---|---|---|
| File | `camelCase.js` | `profileDataAnalyzer.js`, `colorSchemeManager.js`, `flameGraph.js` |
| File ⇄ main export | `camelCase` filename matches the main `PascalCase` export | `flameGraph.js` → `FlameGraph`, `searchManager.js` → `SearchManager` |
| Class | `PascalCase` | `Widget`, `SVGWidget`, `FlameGraph`, `ViewPort` |
| Manager singleton | `Manager` suffix; only `static` members + `static #` private fields | `ColorSchemeManager`, `SearchManager` |
| Builder | `Builder` suffix for heavy/async construction paths | `ProfileDataAnalyzerBuilder` |
| Public function / method | `camelCase` | `truncateFunctionName`, `getMetricInfo` |
| Private instance method | leading `_` | `_renderCallInfo` (`widget/timeLine.js`), `_viewRangeToTimeRange` (`widget/widget.js`) |
| Private field | `#name` (ES private syntax) | `static #selectedColorMode`, `static #regexCache` |
| "Static constant" | `static get UPPER_SNAKE() { return '<value>'; }` | `ColorSchemeManager.COLOR_MODE_METRIC_COST` |
| Module-internal helper | plain `function`, **no** leading `_`, not exported | `loadCustomCategories`, `saveCustomCategories` in `colorSchemeManager.js` |
| Custom event | `spx-<topic>-<verb>` (kebab-case) | `spx-timerange-update`, `spx-color-scheme-mode-update` |
| DOM `data-*` | `data-<kebab>` ⇄ `dataset.<camelCase>` | `data-call-idx` ⇄ `dataset.callIdx`, `data-cg-node-idx` ⇄ `dataset.cgNodeIdx` |
| HTML container ID | kebab-case, referenced as fixed strings from JS | `flat-profile`, `color-scheme-selector`, `flamegraph` |
| Local variable holding an instance | full type as `camelCase`, do **not** shorten to a generic noun | `const abortController = new AbortController()` (not `const controller`); `const progressBar = new ProgressBar()` (not `const bar`) |
| Metric descriptor objects (from `/data/metrics`) | `metricsInfo` | `setMetricsInfo(metricsInfo)`, `this.metricsInfo` |
| Enabled metric keys (from `metadata.enabled_metrics`) | `enabledMetrics` | `this.enabledMetrics`, never `metrics` alone (ambiguous with `metricsInfo`) |

The "static getter as constant" pattern is unusual but systematic. It also enables
reflective enumeration (see `colorSchemeManager.js`):

```js
static get COLOR_MODE_METRIC_COST() { return 'metric_cost'; }
static get COLOR_MODE_AUTO_CATEGORY() { return 'auto_category'; }
static get COLOR_MODE_CUSTOM_CATEGORY() { return 'custom_category'; }

static getColorModes() {
    return Object.getOwnPropertyNames(this)
        .filter((name) => name.startsWith('COLOR_MODE_'))
        .map((name) => this[name]);
}
```

### Method definition order

Methods are defined in lifecycle / construction order:

1. `constructor`
2. Setup methods called once after construction (`setMetricsInfo`, `setMetadata`, …)
3. Incremental / streaming methods called repeatedly (`addEvent`, …)
4. Output / read methods called last (`getContent`, `render`, …)
5. Private helpers (`_rewritePath`, `_renderCallInfo`, …)

### Imports

- Utility / "flat" modules as namespaces: `import * as utils from './utils.js';`
- Classes as named imports: `import { FlameGraph } from './widget/flameGraph.js';`
- Mixed modules (functions + classes, e.g. `widget/widget.js`) → named imports of what
  is actually used:

  ```js
  import { ViewTimeRange, renderSVGTimeGrid, renderSVGMetricValuesPlot, renderSVGMultiLineText } from './widget.js';
  ```
- **Always** include the `.js` extension — required by strict ESM (no resolution magic
  at runtime in dev).
- Relative paths only (`./`, `./../`), no aliases.

### Widget hierarchy and lifecycle

Two-level base in `widget/widget.js` and `widget/svgWidget.js`:

```
Widget                  // generic, DOM-based (FlatProfile, SearchBox, ColorSchemeSelector)
└── SVGWidget           // owns a ViewPort (svg root), handles container resize
                        // (FlameGraph, TimeLine, OverView)
```

Constructor signature is fixed: `(container, profileDataAnalyzer)`. The base wires
the event bus once; subclasses override `render()` plus only the hooks they care about.
A widget that doesn't depend on a given signal opts out of the default `repaint()` by
overriding the hook with an **empty body** (see `searchBox.js`, `colorSchemeSelector.js`
overriding `onColorSchemeModeUpdate()`, `onColorSchemeCategoryUpdate()`,
`onHighlightedFunctionUpdate()`, `onSearchQueryUpdate()` as no-ops).

Lifecycle hooks (defined on `Widget`, default to no-op or `repaint()`):

| Hook | Triggered by event | Default body |
|---|---|---|
| `onTimeRangeUpdate()` | `spx-timerange-update` | (no-op) |
| `onLevelOfDetailsUpdate()` | `spx-lod-update` | (no-op) |
| `onColorSchemeModeUpdate()` | `spx-color-scheme-mode-update` | `repaint()` |
| `onColorSchemeCategoryUpdate()` | `spx-color-scheme-category-update` * | `repaint()` |
| `onHighlightedFunctionUpdate()` | `spx-highlighted-function-update` ** | `repaint()` |
| `onSearchQueryUpdate()` | `spx-search-query-update` | `repaint()` |
| `onContainerResize()` | `window` resize | (no-op) |
| `render()` | called by `repaint()` | (no-op) |

\* The `spx-color-scheme-category-update` listener is **gated** in the base: it only
invokes `onColorSchemeCategoryUpdate()` when
`ColorSchemeManager.getSelectedColorMode() === COLOR_MODE_CUSTOM_CATEGORY`. Editing
custom categories while in `metric_cost` / `auto_category` mode does not trigger a repaint.

\*\* The `spx-highlighted-function-update` listener has a **side effect**: it calls
`ColorSchemeManager.setHighlightedFunctionName(e.detail)` *before* invoking the hook, so
producers (TimeLine, FlameGraph, FlatProfile click handlers) just dispatch the event
and the base syncs the manager. (A `FIXME` in `widget/widget.js` notes that this runs
once per widget instance instead of centrally.)

`repaint()` is debounced via a `setTimeout(0)` guarded by `repaintTimeout` — many calls
in the same tick collapse into one redraw. `SVGWidget` overrides `clear()` to clear its
`viewPort` (preserving the SVG root) instead of `replaceChildren()` on the container,
so a repaint of an SVG widget does not destroy the `<svg>` element itself.

Producers don't dispatch events directly; they call `notify*` helpers on the base:

```js
notifyTimeRangeUpdate(timeRange)     // dispatches spx-timerange-update + LoD ramp
notifyColorSchemeModeUpdate(mode)    // dispatches spx-color-scheme-mode-update
notifyColorSchemeCategoryUpdate()    // dispatches spx-color-scheme-category-update
```

The other events (`spx-highlighted-function-update`, `spx-search-query-update`) are
dispatched inline by the producers (`flameGraph.js`, `flatProfile.js`, `timeLine.js`,
`searchBox.js`); a `notify*` wrapper exists only when the producer does extra work
alongside (e.g. the level-of-details staircase in `notifyTimeRangeUpdate`).

The LoD staircase (`widget/widget.js::notifyTimeRangeUpdate`): on a time range change,
`timeRangeAnalyzer.setTimeRange(timeRange, 0.01)` runs synchronously (cheapest pass);
then `setTimeout`-scheduled `setLevelOfDetails(0.11..1.0)` calls fire every 60 ms,
each dispatching `spx-lod-update`. Result: the new range becomes visible almost
immediately at low resolution, and details fill in over ~600 ms. `searchBox` is
debounced 500 ms before dispatching `spx-search-query-update` for the same reason
(avoid recomputing on every keystroke).

### profileDataAnalyzer.js — only the Builder is exported

The 60 KB `profileDataAnalyzer.js` module defines ~18 classes
(`PackedRecordArray[SoA]`, `Poolable`, `MetricValueSet`, `CallList`, `CallRangeTree`,
`Stats`, `FunctionsStats`, `CallTreeStats`, `CumCostStats`, `TimeRangeStats`,
`TimeRangeAnalyzer`, `ProfileDataAnalyzer`, …) but `export`s only
`ProfileDataAnalyzerBuilder`. Construction is a two-phase async flow:

```js
const builder = new ProfileDataAnalyzerBuilder(metricsInfo);
builder.setMetadata(metadata);
// stream-feed events + function names from the report
for (const event of events)            { builder.addEvent(event); }
for (const [idx, name] of functionNames) { builder.setFunctionName(idx, name); }
await builder.buildCallRangeTree(progressCb);  // async — chunked tree build
const profileDataAnalyzer = builder.getProfileDataAnalyzer();
```

`report.html` drives this from a `fetch().body.getReader()` stream so the UI stays
responsive on multi-million-call reports.

### TimeRangeAnalyzer — the bridge between data and widgets

`TimeRangeAnalyzer` (private to `profileDataAnalyzer.js`, exposed through
`profileDataAnalyzer.getTimeRangeAnalyzer()`) is what every visualization actually
reads. The `Widget` base stashes it as `this.timeRangeAnalyzer` in the constructor;
visualizations then pull cached results from it during `render()`:

| Method | Returns | Used by |
|---|---|---|
| `getSignificantCalls()` | calls above the duration threshold for the current range | `TimeLine`, `OverView` |
| `getCallTreeStats()` | aggregated call tree (root → children, with inc/exc per metric) | `FlameGraph` |
| `getFunctionsStats()` | flat per-function stats (called, inc, exc, maxCycleDepth) | `FlatProfile` |
| `getCumCostStats()` | cumulative cost ranges (pos/neg) for relative-bar rendering | `FlatProfile` |
| `getSubRangesInfo()` | per-sub-range max depth, used for the LoD shading bands | `TimeLine` |
| `getLevelOfDetails()` | current LoD ∈ [0..1] | `TimeLine` (sub-range opacity) |

`setTimeRange(timeRange, lod, minRelativeDurationThreshold)` is the single entry
point. Internally it short-circuits when nothing changed, and on a **LoD-only**
change (same `timeRange`) it recomputes only `subRangesInfo` and `callTreeStats` —
`significantCalls`, `functionsStats`, `cumCostStats` are kept from the previous
full pass. This is why the LoD ramp is cheap.

### SVG rendering helpers

In `assets/web-ui/v2/js/svg.js`:
- `svg.createNode(name, attributes, builder?)` — every SVG element is created through
  this helper (uses `createElementNS('http://www.w3.org/2000/svg', name)`).
- `svg.NodePool(name)` — node recycling for hot redraw paths. Used by widgets that
  redraw thousands of `<rect>` / `<text>` (TimeLine, FlameGraph):

  ```js
  this.svgRectPool = new svg.NodePool('rect');
  // each render():
  this.svgRectPool.releaseAll();
  // per-call:
  this.svgRectPool.acquire({ x, y, width, height, fill, /* ... */ });
  ```

  `releaseAll()` resets the cursor, `acquire()` reuses pooled nodes — avoids GC pressure.

`viewPort.appendChildToFragment(node)` + `viewPort.flushFragment()` is the other
hot-path pattern: nodes accumulate in a `DocumentFragment` and ship in one DOM mutation.

Three shared SVG helpers live in `widget/widget.js` (alongside the `Widget` base) and
are imported as named imports by widgets that need them:

- `renderSVGTimeGrid(viewPort, timeRange, detailed)` — vertical major/minor ticks +
  time labels (TimeLine, OverView).
- `renderSVGMetricValuesPlot(viewPort, profileDataAnalyzer, metric, timeRange)` —
  polyline plot of a metric's values across the range, with horizontal grid + labels
  (TimeLine, OverView). For `ct`/`it` (time-component metrics) it plots the
  *derivative* against `wt` instead of the raw cumulative value.
- `renderSVGMultiLineText(viewPort, lines)` — stacked `<tspan>` lines for the
  hover/select info overlay (FlameGraph, TimeLine).

### DEBUG flag

`DEBUG` is a build-time constant (esbuild `--define:DEBUG=true|false`). Used for
assertions that get dead-code-eliminated in release:

```js
if (DEBUG) {
    if (start > end) {
        throw new Error('Invalid range: ' + start + ' ' + end);
    }
}
```

(`math.js::Range`, also `profileDataAnalyzer.js`)

`DEBUG=true` in `build-dev.sh`, `DEBUG=false` in `build-release.sh`. Don't reference
`DEBUG` from a file consumed outside the bundle path — it is undefined in the browser.

### Data-driven DOM helpers (framework-free by design)

- **`dataTable.makeDataTable(containerId, options, rows)`** (`dataTable.js`) — sortable
  table from `options = { columns, makeRowUrl? }`. Each column is `{ label, value,
  format?, cssClass? }`; `value` is either a property name or an accessor function.
  When `makeRowUrl(row)` is provided, every cell is wrapped in an `<a href="...">`
  pointing at the same URL (used by `index.html` to link each report row to
  `report.html?key=...`). The CSS classes emitted are snake_case (`data_table`,
  `data_table-sort`) — the only snake_case identifiers in v2.
- **`layoutSplitter.init()`** (`layoutSplitter.js`) — generic resizable splitter, wired
  declaratively from HTML via `data-layout-splitter-{target,axis,dir,min}` attributes.
  Companion `layoutSplitter.change(handler)` registers a callback fired during the drag —
  used by `report.html` to re-flow SVG widgets (`timeline`, `flat-profile`, `flamegraph`)
  via `widget.handleResize()`.

Note that not every interactive element in `report.html` is a `Widget`. The metric
selector (`#metric-selector > select`) is plain inline JS in `report.html`: its `change`
handler iterates the widget list and calls `setCurrentMetric(value)` + `repaint()` on
each. There is no `MetricSelector` widget class.

### Build

```sh
cd assets/web-ui/v2/js
npm ci
./build-dev.sh        # esbuild --watch, DEBUG=true, no terser, no sourcemap
./build-release.sh    # esbuild bundle (DEBUG=false) + terser; writes spx.js (sourcemap inlined)
./format.sh           # npm run lint:fix (eslint + prettier as a rule)
```

`build-release.sh` specifics worth knowing:
- `--external:*.min.js --external:node_modules` — nothing under `node_modules` ends up
  in the bundle (vendored deps live in their own `*.min.js`).
- terser flags: `--compress passes=3,drop_console=false,keep_classnames=true,keep_fnames=true --mangle`.
  Class and function names are preserved on purpose — stack traces, `func.name` lookups,
  and reflective enumeration like `getColorModes()` rely on them. `drop_console=false`
  is also intentional: `console.time('repaint ' + id)`, `console.time('Call list
  building')`, etc. are kept in release builds for field diagnosis.
- A GPL banner is injected via esbuild `--banner:js=...`; terser preserves it.
- The intermediate `spx.js.tmp` and `spx.js.tmp.map` are gitignored.

### Web UI v1

Legacy jQuery-based, monolithic (`widget.js` is 53 KB). Kept only for backward
compatibility — all new features go into v2. Do not refactor it.

## Building the extension

```sh
phpize
./configure                              # add --enable-spx-dev for debug symbols
make -j$(nproc)
sudo make install
```

- `config.m4` enforces
  `-Werror -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-attributes -O3 -pthread`,
  plus `-Wextra` on Linux, and `-march=native` outside CI. The three `-Wno-*` are
  load-bearing — new code is allowed to ignore unused parameters / sign comparisons /
  unknown attributes; do not re-enable them locally without checking the existing code
  doesn't rely on them.
- C standard: `-std=c11` for PHP 8.2+, `-std=gnu90` otherwise. Never introduce
  C99-only syntax without a version guard:

  ```sh
  if test "$php_ver_num" -ge 80200; then
      CFLAGS="$CFLAGS -std=c11"
  else
      CFLAGS="$CFLAGS -std=gnu90"
  fi
  ```
- `-Wno-typedef-redefinition` is required (macOS / clang compatibility).
- To add a source file: edit the `PHP_NEW_EXTENSION(spx, ...)` block in `config.m4`
  (explicit list, no glob).
- ZSTD is optional — gated by `#ifdef HAVE_ZSTD`.

## Supported platforms

GNU/Linux, macOS, FreeBSD, Windows (partial). x86-64 and ARM64 only.
Compile-time guard at the top of `src/php_spx.h`.

PHP 7.0 → 8.5 — bounded **on both ends** by
`ZEND_MODULE_API_NO < 20151012 || ZEND_MODULE_API_NO > 20250925` in `src/php_spx.h`,
so a future PHP version will refuse to compile until the upper bound is bumped.
Branch on `ZEND_MODULE_API_NO` (never `PHP_VERSION_ID`):

```c
#if ZEND_MODULE_API_NO >= 20170718  /* PHP 7.2+ */
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(s, len)
    ZEND_PARSE_PARAMETERS_END();
#else
    if (zend_parse_parameters(...) == FAILURE) return;
#endif
```

(see `src/php_spx.c::spx_profiler_full_report_set_custom_metadata_str`)

## CI

`.github/workflows/main.yml` — matrix `linux × macOS × debian (docker)` × `PHP 7.0..8.5`.
PHP 8.5 is currently **excluded from the Debian leg** (commented in `main.yml`: waiting
on `packages.sury.org`); the exclusion can be dropped once the package is available.
Three jobs:
1. `build` — phpize + configure + make + `make test` over the full matrix.
2. `check-js` — Prettier lint + Web UI v2 release build; fails on any diff.
3. `release` (on tag) — assembles the zips and publishes via `ncipollo/release-action`.

## Versioning and changelog

- Version lives in `src/php_spx.h::PHP_SPX_VERSION` (bumped manually).
- `CHANGELOG.md` follows Keep-a-Changelog. Sections: `### Added` / `### Fixed` / `### Changed`.
  Each entry references a PR `[#NNN]`.
- `CONTRIBUTING.md` rule: open an issue before any PR; only compatibility patches and
  bug fixes are merged.
