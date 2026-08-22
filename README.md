# SPX - A seamless profiler for PHP

[![Build Status][:badge-ci:]][:link-ci:]
![Supported PHP versions: 7.0 .. 8.x][:badge-php-versions:]
![Supported platforms: GNU/Linux, macOS & FreeBSD][:badge-supported-platforms:]
![Supported architectures: x86-64 or ARM64][:badge-supported-arch:]
[![License][:badge-license:]][:link-license:]


<a href="https://www.buymeacoffee.com/noisebynw" target="_blank"><img src="https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png" alt="Buy Me A Coffee" style="height: 41px !important;width: 174px !important;box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;-webkit-box-shadow: 0px 3px 2px 0px rgba(190, 190, 190, 0.5) !important;" ></a>


[Click here for a live demo of the analysis screen](https://noisebynorthwest.github.io/php-spx/demo/report.html?key=spx-full-20191229_175636-06d2fe5ee423-3795-233665123)

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/d8a90827d6eb256f49d580de448b6b6fad4119ac/php-spx/doc/as.apng)

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/43e3ffe185a1dcec70e7c8ced36acfdf316bae65/php-spx/doc/fp1.gif)

SPX, which stands for _Seamless Profiling eXperience_, is just another profiling extension for PHP.
It differentiates itself from other similar extensions as being:
- totally free and confined to your infrastructure (i.e. no data leaks to a SaaS).
- very simple to use: just set an environment variable (command line) or switch on a radio button (web request) to profile your script. Thus, you are free of:
  - manually instrumenting your code (Ctrl-C a long-running command line script is even supported).
  - using a dedicated browser extension or command line launcher.
- [multi-metric](#available-metrics) capable: 22 are currently supported (various time & memory metrics, included files, objects in use, I/O...).
- able to collect data without losing context. For example XHProf (and potentially its forks) aggregates data per caller / callee pairs, which implies the loss of the full call stack and forbids timeline or Flame Graph based analysis.
- low-overhead: roughly half the overhead of [XHProf](https://www.php.net/manual/en/book.xhprof.php) in tracing mode, close to [Excimer](https://www.mediawiki.org/wiki/Excimer) in sampling mode.
- shipped with its [web UI](#web-ui) which allows to:
  - enable / configure profiling for the current browser session
  - list profiled script reports
  - select a report for in-depth analysis, featuring these interactive visualizations:
    - timeline (scale to millions of function calls)
    - flat profile
    - Flame Graph
  - export a report to well-known formats (Callgrind, pprof and Trace Event Format) for analysis in external tools such as KCachegrind, Speedscope or Perfetto

## Table of contents

- [Requirements](#requirements)
- [Installation](#installation)
  - [Prerequisites](#prerequisites)
  - [Install the extension](#install-the-extension)
  - [ZTS PHP (multi-thread)](#zts-php-multi-thread)
  - [Linux, PHP-FPM & I/O stats](#linux-php-fpm--io-stats)
- [Basic usage](#basic-usage)
  - [Serving and accessing the web UI](#serving-and-accessing-the-web-ui)
  - [Web request profiling](#web-request-profiling)
  - [Command line script profiling](#command-line-script-profiling)
- [Advanced usage](#advanced-usage)
  - [Configuration](#configuration)
  - [Available metrics](#available-metrics)
  - [Command line reference](#command-line-reference)
  - [Web UI](#web-ui)
- [Security concern](#security-concern)
- [Notes on accuracy](#notes-on-accuracy)
- [Related projects](#related-projects)
- [Contributing](#contributing)
- [Credits](#credits)
- [License](#license)

## Requirements

Platform support is currently quite limited. Feel free to open an issue if your platform is not supported.
Current requirements are:

- x86-64 or ARM64
- **GNU/Linux**, **macOS** or **FreeBSD**
- PHP 7.0 to 8.5. PHP 5.x, starting from 5.4, is supported by SPX 0.4.x and earlier versions.
- zlib, and optionally Zstandard. The [prerequisites](#prerequisites) section lists the development packages providing them.

## Installation

### Prerequisites

Whichever installation method you choose, the extension is built from source. Install the following development packages first:

- PHP development package, matching your installed PHP version (provides `phpize`).
- zlib development package (required):
  - For Debian-based distros (including Ubuntu, Kubuntu...): `sudo apt-get install zlib1g-dev`.
  - For Fedora-based distros (including CentOS, AlmaLinux, Rocky Linux...): `sudo dnf install zlib-devel`.
- Zstandard development package (optional but recommended, as it minimizes SPX's overhead):
  - For Debian-based distros: `sudo apt-get install libzstd-dev`.
  - For Fedora-based distros: `sudo dnf install libzstd-devel`.

### Install the extension

#### Install via PIE

Requires [PIE](https://github.com/php/pie), the PHP extension installer.

```shell
pie install noisebynorthwest/php-spx
```

#### Install from source

```shell
git clone https://github.com/NoiseByNorthwest/php-spx.git
cd php-spx
git checkout release/latest
phpize
./configure
make
sudo make install
```

#### Activate & configure SPX

After installing SPX, add `extension=spx.so` to your *php.ini*, or to a dedicated *spx.ini* file created within the include directory.
You may also want to override the [default configuration](#configuration) in order to profile web requests. See the [private environment](#private-environment) setup for a local development environment, for example.


### ZTS PHP (multi-thread)

ZTS PHP is supported, with these extra limitations:
- a little overhead (theoretically unnoticeable in most cases) is added when SPX is loaded, even if it is not enabled.
- pressing Ctrl-C on a CLI script will prevent the ongoing profiling session from finishing properly.
- segfaults are more likely than for NTS PHP. In this regard, avoid more than ever mixing SPX with other instrumenting extensions (debuggers, profilers...).
- the web UI is not served automatically in the default setup, see below.

Under ZTS, SPX installs its instrumentation once for the whole process at startup, instead of doing it per request. SPX also uses the legacy `zend_execute_ex()` hook to stop the requested script from running, which is how the web UI takes its place. So `spx.use_observer_api` arbitrates between the PHP JIT and the web UI:
- `1` (default): function calls are instrumented with the Zend Observer API, so the PHP JIT stays enabled. The web UI is not served automatically, call `spx_ui_handle_request()` from your worker or front controller instead, as described in [worker-based PHP servers](#worker-based-php-servers). This is the recommended setup.
- `0`: function calls are instrumented with the legacy `zend_execute_ex()` hook, so the web UI is served automatically. The PHP JIT is then disabled for the whole process, even for requests you do not profile.

On PHP older than 8.2 the Zend Observer API is not available, so SPX always falls back to the legacy hook, whatever the value of `spx.use_observer_api`.

Also, consider ZTS PHP support as still being in beta.

### Linux, PHP-FPM & I/O stats

On GNU/Linux, SPX uses procfs (i.e. by reading files under `/proc` directory) to get some stats for the current process or thread. This is what is done under the hood when you select at least one of these metrics: `mor`, `io`, `ior` or `iow`.

But, on most PHP-FPM setups, you will have a permission issue preventing SPX from opening a file under the `/proc/self` directory.
This is because the PHP-FPM master process runs as root while child processes run as another unprivileged user.

When this is the case, the `process.dumpable = yes` line must be added to the FPM pool configuration so that child processes will be able to read any file under `/proc/self`.

## Basic usage

### Serving and accessing the web UI

Assuming a development environment using the [private environment](#private-environment) configuration, with your application accessible at `http://localhost`.

Just open the following URL in your browser: `http://localhost/?SPX_KEY=dev&SPX_UI_URI=/` to access the web UI [control panel](#control-panel--report-list).

#### Regular PHP server (FPM, mod_php)

You have nothing special to do, it will work out of the box.

However, `http://localhost/` must be served by a PHP script through standard web server features like directory index or URL rewriting. The PHP script will not be executed, SPX will intercept and disable its execution to serve its content in its place.

If you see only a blank page, then make sure to set `zlib.output_compression = 0` in your PHP configuration file.

#### Worker-based PHP servers

If you are running PHP in worker mode, such as with FrankenPHP, it is also possible to serve the SPX web UI directly from the worker process. This requires that the worker implementation correctly exposes PHP superglobals for each HTTP request, as FrankenPHP does.

To do this you can use the following function:

```php
spx_ui_handle_request(): bool
```

This function will attempt to serve the SPX web UI. If it handles the request, it returns `true`, and you should skip further application logic.

Example usage with a custom FrankenPHP handler:

```php
$handler = static function () use ($myApp) {
    if (spx_ui_handle_request()) {
        // A SPX UI request has been served
        return;
    }

    echo $myApp->handle($_GET, $_POST, $_COOKIE, $_FILES, $_SERVER);
};
```

### Web request profiling

Once in the control panel, you will see the following form:

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/93baabbcba04223586d06756dbcecfbd6ec1293d/php-spx/doc/cp-form.png)

Then switch on "Enabled". At this point profiling is enabled for the current domain and your current browser session through a set of dedicated cookies.

Profiling can also be triggered with `curl` as shown in this example:

`curl --cookie "SPX_ENABLED=1; SPX_KEY=dev" http://localhost/`

_N.B.: You can also enable the profiling at INI configuration level via the `spx.http_profiling_enabled` [setting](#configuration), and therefore for all HTTP requests. However, keep in mind that using this setting on a high-traffic environment could quickly exhaust the storage device's capacity of the SPX's data directory._

Then refresh the web request you want to profile and refresh the control panel to see the generated report in the list below the control panel form.

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/d8a90827d6eb256f49d580de448b6b6fad4119ac/php-spx/doc/cp-list2.png)

Then click on the report in the list and enjoy the [analysis screen](#analysis-screen).

### Command line script profiling

#### Instant flat profile

Just prepend your command line with `SPX_ENABLED=1` to trigger profiling. You will get the flat profile printed on STDERR at the end of the execution, even if you abort it by hitting Ctrl-C, as in the following example:

```shell
$ SPX_ENABLED=1 composer update
Loading composer repositories with package information
Updating dependencies (including require-dev)
^C
*** SPX Report ***

Global stats:

  Called functions    :    27.5K
  Distinct functions  :      714

  Wall time           :    7.39s
  ZE memory           :   62.6MB

Flat profile:

 Wall time           | ZE memory           |
 Inc.     | *Exc.    | Inc.     | Exc.     | Called   | Function
----------+----------+----------+----------+----------+----------
  101.6ms |  101.6ms |   41.8MB |   41.8MB |       12 | Composer\Json\JsonFile::parseJson
   53.6ms |   53.6ms |     544B |     544B |        4 | Composer\Cache::sha256
    6.91s |   41.5ms |   41.5MB |   -7.5MB |        4 | Composer\Repository\ComposerRepository::fetchFile
    6.85s |   32.3ms |   47.5MB |    5.4MB |        5 | 1@Composer\Repository\ComposerRepository::loadProviderListings
    7.8ms |    7.8ms |       0B |       0B |        4 | Composer\Cache::write
    1.1ms |    1.1ms |     -72B |     -72B |        1 | Composer\Console\Application::Composer\Console\{closure}
  828.5us |  828.5us |     976B |     976B |       12 | Composer\Util\RemoteFilesystem::findHeaderValue
  497.6us |  491.0us |  710.2KB |  710.2KB |        1 | Composer\Cache::read
    2.4ms |  332.6us |   20.9KB | -378.8KB |       34 | 3@Symfony\Component\Finder\Iterator\FilterIterator::rewind
  298.9us |  298.9us |    2.2KB |    2.2KB |       47 | Symfony\Component\Finder\Iterator\FileTypeFilterIterator::accept
```

N.B.: Just add `SPX_FP_LIVE=1` to enable the live refresh of the flat profile during script execution.

#### Generate profiling report for the web UI

You just have to specify `SPX_REPORT=full` to generate a report available via the web UI:

```shell
SPX_ENABLED=1 SPX_REPORT=full ./bin/console cache:clear
```


#### Handle long-living / daemon processes

If your CLI script is long-living and/or daemonized (e.g. via supervisord), profiling its whole lifespan could be meaningless. This is especially true in case of a service waiting for tasks to process.  
To handle this case, SPX allows to disable the automatic start of profiling and exposes 2 userland functions, `spx_profiler_start(): void` & `spx_profiler_stop(): ?string`, in order to respectively control the start and the end of the profiled spans.  

Here is how you can instrument your script:

```php
<?php

while ($task = get_next_ready_task()) {
  spx_profiler_start();
  try {
    $task->process();
  } finally {
    spx_profiler_stop();
  }
}

```

And of course this script must be run at least with profiling enabled and the automatic start disabled as in the following command:

```shell
SPX_ENABLED=1 SPX_REPORT=full SPX_AUTO_START=0 php my_script.php
```

Automatic start can also be disabled for web requests via the `spx.http_profiling_auto_start` INI parameter or via the control panel.


Side notes:
- `spx_profiler_start()` and `spx_profiler_stop()` can safely be nested.
- when profiling with the _full_ report type, `spx_profiler_stop()` returns the report key so that you will be able to store it somewhere, for instance among other information related to the profiled span. With the report key you can build the analysis screen URL which ends with this pattern `/?SPX_UI_URI=/report.html&key=<report key>`.  
- in CLI context, when automatic start is disabled, no signal handlers (i.e. on SIGINT/SIGTERM) are registered by SPX.


#### Add custom metadata to the current full report

When profiling with _full_ report as output, it could be handy to add custom metadata to the current report so that you will be able to easily retrieve it or differentiate it from other similar reports.

This is especially true for the long-living process use case which otherwise would not allow to differentiate a report from other ones of the same process.

To do that SPX exposes the `spx_profiler_full_report_set_custom_metadata_str(string $customMetadataStr): void` function.

As you may have noticed, this function accepts a string as custom metadata, for the sake of flexibility and simplicity on the SPX side. It is up to you to encode any structured data to a string, for instance using JSON format.

The metadata string is limited to 4KB, which is large enough for most use cases. If you pass a string exceeding this limit it will be discarded and a notice log will be emitted.

This string will be stored among the current report's other metadata and you will retrieve it in the report list on the web UI side.

`spx_profiler_full_report_set_custom_metadata_str()` can be called at any moment as long as the profiler is already started and not finished yet, which means:
- at any moment during the script execution when automatic start is enabled (default mode).
- at any moment after the call of `spx_profiler_start()` and before the call of `spx_profiler_stop()` when automatic start is disabled.

Here is an example:

```php
<?php

while ($task = get_next_ready_task()) {
  spx_profiler_start();

  spx_profiler_full_report_set_custom_metadata_str(json_encode(
    [
      'taskId' => $task->getId(),
    ]
  ));

  try {
    $task->process();
  } finally {
    spx_profiler_stop();
  }
}

```


## Advanced usage

### Configuration

| Name                  | Default  | Changeable  | Description  |
| --------------------- | -------- | ----------- | ------------ |
| _spx.use_observer_api_ | `1` | _PHP_INI_SYSTEM_ | Whether to instrument function calls with the Zend Observer API instead of the legacy `zend_execute_ex()` hook. It requires PHP 8.2 or later and is ignored on earlier versions. Setting this parameter to `0` is only worth it for troubleshooting purposes, or, on [ZTS PHP](#zts-php-multi-thread), to restore the automatic serving of the web UI at the cost of the PHP JIT being disabled for the whole process. |
| _spx.data_dir_     | `/tmp/spx` | _PHP_INI_SYSTEM_ | The directory where profiling reports will be stored. You may change it to point to a shared file system for example in case of multi-server architecture.  |
| _spx.http_enabled_      | `0`  | _PHP_INI_SYSTEM_ | Whether to enable web UI and HTTP request profiling. |
| _spx.http_key_          |  | _PHP_INI_SYSTEM_ | The secret key used for authentication (see [security concern](#security-concern) for more details). You can use the following command to generate a 16-byte random key as a hex string: `openssl rand -hex 16`. |
| _spx.http_ip_var_       | `REMOTE_ADDR` | _PHP_INI_SYSTEM_ | The `$_SERVER` key holding the client IP address used for authentication (see [security concern](#security-concern) for more details). Overriding the default value is required when your application is behind a reverse proxy. |
| _spx.http_trusted_proxies_       | `127.0.0.1` | _PHP_INI_SYSTEM_ | The trusted proxy list as a comma separated list of IP addresses<b>*</b>. This setting is ignored when `spx.http_ip_var`'s value is `REMOTE_ADDR`. |
| _spx.http_ip_whitelist_ |  | _PHP_INI_SYSTEM_ | The IP address whitelist used for authentication as a comma separated list of IP addresses<b>*</b>. |
| _spx.http_ui_assets_dir_ | `/usr/local/share/misc/php-spx/assets/web-ui` | _PHP_INI_SYSTEM_ | The directory where the [web UI](#web-ui) files are installed. In most cases you do not have to change it. |
| _spx.http_profiling_enabled_ | _NULL_ | _PHP_INI_SYSTEM_ | The INI level counterpart of the `SPX_ENABLED` parameter, for HTTP requests only. See [here for more details](#available-parameters). |
| _spx.http_profiling_auto_start_ | _NULL_ | _PHP_INI_SYSTEM_ | The INI level counterpart of the `SPX_AUTO_START` parameter, for HTTP requests only. See [here for more details](#available-parameters). |
| _spx.http_profiling_builtins_ | _NULL_ | _PHP_INI_SYSTEM_ | The INI level counterpart of the `SPX_BUILTINS` parameter, for HTTP requests only. See [here for more details](#available-parameters). |
| _spx.http_profiling_sampling_period_ | _NULL_ | _PHP_INI_SYSTEM_ | The INI level counterpart of the `SPX_SAMPLING_PERIOD` parameter, for HTTP requests only. See [here for more details](#available-parameters). |
| _spx.http_profiling_depth_ | _NULL_ | _PHP_INI_SYSTEM_ | The INI level counterpart of the `SPX_DEPTH` parameter, for HTTP requests only. See [here for more details](#available-parameters). |
| _spx.http_profiling_metrics_ | _NULL_ | _PHP_INI_SYSTEM_ | The INI level counterpart of the `SPX_METRICS` parameter, for HTTP requests only. See [here for more details](#available-parameters). |

_\*: `*` (match all) and subnet masks (e.g. `192.168.1.0/24`) are supported. Subnet masks are IPv4 only, so an IPv6 client address such as `::1` has to be whitelisted as an exact address._

#### Private environment

For your local & private development environment, since there is no need for authentication, you can use this configuration:

```
spx.http_enabled=1
spx.http_key="dev"
spx.http_ip_whitelist="127.0.0.1,::1"
```

And then access the web UI at `http(s)://<your application host>/?SPX_KEY=dev&SPX_UI_URI=/`.

### Available metrics

Here is the list of available metrics to collect. By default only _Wall time_ and _Zend Engine memory usage_ are collected.

| Key (command line) | Name | Description |
| ---- | ---------------- | ------ |
| _wt_ | Wall time | The absolute elapsed time. |
| _ct_ | CPU time | The time spent while running on CPU. |
| _it_ | Idle time | The time spent off-CPU, that means waiting for CPU, I/O completion, a lock acquisition... or explicitly sleeping. |
| _zm_ | Zend Engine memory usage | Equivalent to `memory_get_usage(false)`. |
| _zmac_ | Zend Engine allocation count | Number of memory allocations (i.e. allocated blocks) performed. |
| _zmab_ | Zend Engine allocated bytes<b>*</b> | Number of allocated bytes. |
| _zmfc_ | Zend Engine free count | Number of memory releases (i.e. freed blocks) performed. |
| _zmfb_ | Zend Engine freed bytes<b>*</b> | Number of freed bytes. |
| _zgr_ | Zend Engine GC run count | Number of times the GC (cycle collector) has been triggered (either manually or automatically). |
| _zgb_ | Zend Engine GC root buffer length | Root buffer length, see explanation [here](http://php.net/manual/en/features.gc.collecting-cycles.php). It could be helpful to track pressure on garbage collector. |
| _zgc_ | Zend Engine GC collected cycle count | Total number of collected cycles through all GC runs. |
| _zif_ | Zend Engine included file count | Number of included files. |
| _zil_ | Zend Engine included line count | Number of included lines. |
| _zuc_ | Zend Engine user class count | Number of userland classes. |
| _zuf_ | Zend Engine user function count | Number of userland functions (including userland class/instance methods). |
| _zuo_ | Zend Engine user opcode count | Number of included userland opcodes (sum of all userland file/function/method opcodes). |
| _zo_ | Zend Engine object count | Number of objects currently held by user code. |
| _ze_ | Zend Engine error count | Number of raised PHP errors. |
| _mor_ | Process's own RSS<b>\*\*</b> | The part of the process's memory held in RAM. The shared (with other processes) memory blocks are not taken into account. This metric can be useful to highlight a memory leak within a PHP extension or deeper (e.g. a third-party C library). |
| _io_ | I/O (reads + writes)<b>\*\*</b> | Bytes read or written while performing I/O. |
| _ior_ | I/O (reads)<b>\*\*</b> | Bytes read while performing I/O. |
| _iow_ | I/O (writes)<b>\*\*</b> | Bytes written while performing I/O. |

_\*: Allocated and freed byte counts will not be collected if you use a custom allocator or if you force the libc one through the `USE_ZEND_ALLOC` environment variable set to `0`._

_\*\*: RSS & I/O metrics are not supported on macOS and FreeBSD. On GNU/Linux you should [read this if you use PHP-FPM](#linux-php-fpm--io-stats)._

### Command line reference

#### Available report types

Contrary to web request profiling which only supports the _full_ report type (the one exploitable by the web UI), command line script profiling supports several types of report.
Here is the list:

| Key  | Name  | Description  |
| ---- | ----- | ------------ |
| _fp_ | Flat profile | The flat profile provided by SPX. It is the **default report type** and is directly printed on STDERR. |
| _full_ | Full report | This is the report type for web UI. Reports will be stored in SPX data directory and thus will be available for analysis on web UI side. |
| _trace_ | Trace file | A custom format (human readable text) trace file. |

#### Available parameters

Setting an SPX parameter for a command line script simply means defining an environment variable with the same name, as shown in the [basic usage example](#command-line-script-profiling).

| Name  | Default  | Description  |
| ----- | -------- | ------------ |
| _SPX_ENABLED_ | `0` | Whether to enable SPX profiler (i.e. triggering profiling). When disabled there is no performance impact on your application (except for ZTS PHP where a disabled SPX still adds a little overhead). |
| _SPX_AUTO_START_ | `1` | Whether to enable SPX profiler's automatic start. When automatic start is disabled, you have to start & stop profiling on your own at runtime via the `spx_profiler_start()` & `spx_profiler_stop()` functions. [See here](#handle-long-living--daemon-processes) for more details. |
| _SPX_BUILTINS_ | `0` | Whether to profile internal functions, script compilations, GC runs and request shutdown. |
| _SPX_DEPTH_ | `0` | The stack depth at which profiling must stop (i.e. aggregate measures of deeper calls). 0 (default value) means unlimited. |
| _SPX_SAMPLING_PERIOD_ | `0` | Whether to collect data for the current call stack at regular intervals according to the specified sampling period (`0` means no sampling). The result will usually be less accurate but in some cases it could be far more accurate by not over-evaluating small functions called many times. It is recommended to try sampling (with different periods) if you want to accurately find a time bottleneck. When profiling a long-running & CPU-intensive script, this option will allow you to contain report size and thus keeping it small enough to be exploitable by the [web UI](#web-ui). See [here](#performance-report-size--sampling) for more details. |
| _SPX_METRICS_ | `wt,zm` | Comma separated list of [available metric keys](#available-metrics) to collect. All report types take advantage of multi-metric profiling. |
| _SPX_REPORT_ | `fp` | Selected [report key](#available-report-types). |
| _SPX_FP_FOCUS_ | `wt` | [Metric key](#available-metrics) for flat profile sort. |
| _SPX_FP_INC_ | `0` | Whether to sort functions by inclusive value instead of exclusive value in flat profile. |
| _SPX_FP_REL_ | `0` | Whether to display metric values as relative (i.e. percentage) in flat profile. |
| _SPX_FP_LIMIT_ | `10` | The flat profile size (i.e. top N shown functions). |
| _SPX_FP_LIVE_ | `0` | Whether to enable flat profile live refresh. Since it plays with cursor position through ANSI escape sequences, it uses STDOUT as output, replacing script output (both STDOUT & STDERR). |
| _SPX_FP_COLOR_ | `1` | Whether to enable flat profile color mode. |
| _SPX_TRACE_SAFE_ | `0` | The trace file is by default written in a way to enforce accuracy, but in case of process crash (e.g. segfault) some logs could be lost. If you want to enforce durability (e.g. to find the last event before a crash) you just have to set this parameter to 1. |
| _SPX_TRACE_FILE_ |  | Custom trace file name. If not specified it will be generated in `/tmp` and displayed on STDERR at the end of the script. |

### Web UI

#### Supported browsers

Since the web UI uses advanced JavaScript features, only the following browsers are known to be supported:
- most recent version of any Chromium-based browser.
- most recent version of Firefox.

#### Control panel & report list

This is the home page of the web UI, divided into 2 parts:
- the control panel for setting the profiling setup for your current browser session.
- the profile report list as a sortable table. A click on a row allows to go to the [analysis screen](#analysis-screen) for the corresponding report.

#### Analysis screen

[Click here for a live demo of the analysis screen](https://noisebynorthwest.github.io/php-spx/demo/report.html?key=spx-full-20191229_175636-06d2fe5ee423-3795-233665123)

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/d8a90827d6eb256f49d580de448b6b6fad4119ac/php-spx/doc/as.th.png)

##### Performance, report size & sampling

The analysis screen can nicely handle profile reports with up to several (5+) millions of recorded function calls with Chromium on my i5 @ 3.3GHz / 8GB desktop.
In case you want to profile a long-running, CPU-intensive script which tends to generate giant reports, you can enable sampling mode with the suitable sampling period.
See the _SPX_SAMPLING_PERIOD_ [parameter](#available-parameters) for command line scripts.

##### Metric selector

This is simply a combo box for selecting the currently analyzed metric.

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/d8a90827d6eb256f49d580de448b6b6fad4119ac/php-spx/doc/as-ms.png)

##### Color scheme selector

Clicking on the color scheme mode link, displayed at the top of the screen just after the metric selector, opens a drop-down window allowing you to switch between these 3 modes:
- `auto_category` (default): derived from the function namespace, so that functions sharing the same top-level namespace share the same hue.
- `metric_cost`: derived from the function cost for the currently selected metric.
- `custom_category`: the color of the first category whose pattern list matches the function name. Uncategorized functions are greyed out.

The same drop-down window allows you to define (add/edit/delete) your categories (color, name, pattern list) for the `custom_category` mode (see the screenshot below).

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/d8a90827d6eb256f49d580de448b6b6fad4119ac/php-spx/doc/as-csm.png)

##### Timeline overview

This visualization is the timeline overview of all called functions.
You can change the selected time range, represented by a transparent green rectangle, by simply dragging it horizontally.

Except for wall time, the current metric is also plotted (current value over time) on a foreground layer.

Supported controls:
- horizontal left click drag: shift the selected time range
- resize click on selected time range rectangle: shift one of the selected time range boundaries

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/d8a90827d6eb256f49d580de448b6b6fad4119ac/php-spx/doc/as-ov.png)

##### Timeline focus

This visualization is an interactive timeline which is able to control and keep focus on the selected time range.

Supported controls:
- left click drag: time range shift (horizontal) or depth range shift (vertical)
- middle click vertical drag: time range zoom in/out
- mouse wheel: time range zoom in/out
- hovering a function call to show more details
- double click on a function call: set the current time range as the one of the selected function call

Except for wall time, the current metric is also plotted (current value over time) on a foreground layer.

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/d8a90827d6eb256f49d580de448b6b6fad4119ac/php-spx/doc/as-tl.png)

##### Flat profile

This visualization is the flat profile for the selected time range and the selected metric, displayed as a sortable table.

The `Inc.` and `Exc.` sub-columns respectively correspond to:
- the inclusive resource consumption of the function, including its called functions consumption
- the exclusive resource consumption of the function, excluding its called functions consumption

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/d8a90827d6eb256f49d580de448b6b6fad4119ac/php-spx/doc/as-fp.png)

##### Flame Graph

This visualization, designed by [Brendan Gregg](http://www.brendangregg.com/flamegraphs.html), allows to quickly find the hot code path for the selected time range and the selected metric.
Metrics corresponding to releasable resources (memory, objects in use...) are not supported by this visualization.

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/d8a90827d6eb256f49d580de448b6b6fad4119ac/php-spx/doc/as-fg.png)


##### Function highlighting

You can highlight a function by clicking on one of its spans within the timeline or Flame Graph widgets or its name within the flat profile widget.

![Showcase](https://github.com/NoiseByNorthwest/NoiseByNorthwest.github.io/blob/47d8f8d93fad1e6659c46c47e5aa8f82822454a9/php-spx/doc/as-fh.png)


## Security concern

**SPX is a development and debugging tool. It is not meant to run on a production or any publicly exposed environment.**

Its embedded web UI lists and serves every stored report, so exposing it means exposing your application's internals. An attacker able to reach it could:
- access the web UI and get sensitive information about your application.
- to a lesser extent, mount a DoS attack against your application with a costly profiling setup.

On a staging server or any shared environment, access must be restricted. If your infrastructure does not already do it at a lower layer, before your application is hit and not by the application or PHP framework itself, SPX has to authenticate every client triggering profiling or accessing the web UI.

SPX provides two-factor authentication with these two mandatory locks:
- IP address whitelist, accepting an exact address or an IPv4 subnet mask.
- Fixed secret random key (generated on your own) provided via a request header, cookie or query string parameter.

Thus a client can profile your application via a web request only if **its IP address is whitelisted and its provided key is valid**.

## Notes on accuracy

In tracing mode (the default), SPX may suffer from accuracy issues for time-related metrics when the measured function execution time is:
- close to or lower than the timer's precision,
- close to or lower than SPX's own per-function overhead.

The first issue is mitigated by using the highest-resolution timer available on the platform.
On Linux, FreeBSD, and recent macOS versions, the timer resolution is 1 ns.
On macOS versions prior to 10.12 (Sierra), the timer resolution is only 1 us.

The second issue is mitigated by accounting for SPX's own (wall/cpu) per-function overhead.
SPX subtracts this overhead from the measured execution time after evaluating its constant cost per function call before profiling begins.

Beyond these mitigations, if you want to further improve accuracy, you should:
- prefer the **full** report (the default for HTTP request profiling) over the other report types,
- build SPX with **Zstandard** support. You can verify this with `php -i`, which should display `SPX Zstandard available => yes`,
- avoid collecting additional metrics (everything is optimized for the default metric set `wt,zm`),
- avoid profiling internal functions,
- try sampling mode with different sampling periods,
- experiment with the maximum depth parameter to stop profiling beyond a given call depth.

## Related projects

- [php-spx-mcp](https://github.com/NoiseByNorthwest/php-spx-mcp): an MCP server exposing SPX profiling reports to LLM agents.
- [php-spx-stubs](https://packagist.org/packages/8ctopus/php-spx-stubs): stubs for SPX functions, to be used with [Intelephense](https://www.npmjs.com/package/intelephense) and installed with `composer require --dev 8ctopus/php-spx-stubs`.

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) first. Always open an issue before you start coding, and never open a pull request directly.

## Credits

I have found lot of inspiration and hints reading:
 - [XHProf](https://github.com/phacility/xhprof)
 - [Xdebug](https://github.com/xdebug/xdebug)
 - [PHP](https://github.com/php/php-src)

## License

**SPX** is open source software licensed under the GNU General Public License (GPL-3).
See the [LICENSE][:link-license:] file for more information.

<!-- All external links should be here to avoid duplication and long lines with links -->
[:badge-ci:]:           https://github.com/NoiseByNorthwest/php-spx/actions/workflows/main.yml/badge.svg
[:link-ci:]:            https://github.com/NoiseByNorthwest/php-spx/actions/workflows/main.yml

[:badge-php-versions:]: https://img.shields.io/badge/php-7.0--8.5-blue.svg
[:badge-supported-platforms:]: https://img.shields.io/badge/platform-GNU/Linux%20|%20macOS%20|%20FreeBSD%20-yellow
[:badge-supported-arch:]: https://img.shields.io/badge/architecture-x86--64%20|%20ARM64%20-silver

[:badge-license:]:      https://img.shields.io/github/license/NoiseByNorthwest/php-spx
[:link-license:]:       https://github.com/NoiseByNorthwest/php-spx/blob/master/LICENSE
