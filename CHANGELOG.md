# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [v0.4.17](https://github.com/NoiseByNorthwest/php-spx/compare/v0.4.16...v0.4.17)

### Added
- PHP 8.4 support [#270](https://github.com/NoiseByNorthwest/php-spx/pull/270)
- Subnet mask support for IP matching [#261](https://github.com/NoiseByNorthwest/php-spx/pull/261)
- ZTS PHP beta support [#260](https://github.com/NoiseByNorthwest/php-spx/pull/260)

### Fixed
- Keep scroll position in flatprofile view on repaint [#267](https://github.com/NoiseByNorthwest/php-spx/pull/267)
- Improved confinement check for file access from WEB UI [#255](https://github.com/NoiseByNorthwest/php-spx/pull/255)

## [v0.4.16](https://github.com/NoiseByNorthwest/php-spx/compare/v0.4.15...v0.4.16)

### Added
- Local snapshots of jQuery & jscolor in order to remove the internet access dependency of the Web UI [#217](https://github.com/NoiseByNorthwest/php-spx/issues/217)

### Fixed
- Fixed unconfined server file access from WEB UI [#251](https://github.com/NoiseByNorthwest/php-spx/issues/251)

## [v0.4.15](https://github.com/NoiseByNorthwest/php-spx/compare/v0.4.14...v0.4.15)

### Added
- Added Debian builds to github workflow [#222](https://github.com/NoiseByNorthwest/php-spx/pull/222)
- Added PHP 8.3 support [#230](https://github.com/NoiseByNorthwest/php-spx/pull/230)
- Added PHP 5.4 support [#227](https://github.com/NoiseByNorthwest/php-spx/pull/227)

### Fixed
- Fixed ESM module imports in Web UI [#233](https://github.com/NoiseByNorthwest/php-spx/issues/233)
- Fixed zlib.h path resolution for macOS/Macport [#238](https://github.com/NoiseByNorthwest/php-spx/issues/238)
- Fixed non-portable path expectation in tests [#221](https://github.com/NoiseByNorthwest/php-spx/pull/221)


## [v0.4.14](https://github.com/NoiseByNorthwest/php-spx/compare/v0.4.13...v0.4.14)

### Added
- Added simple search feature
- Added custom metadata storage implementation
- Added `--enable-spx-dev` to compile with debug symbols

### Fixed
- Fixed buffer overflow in str_builder


## [v0.4.13](https://github.com/NoiseByNorthwest/php-spx/compare/v0.4.12...v0.4.13)

### Added
- Github Actions workflow ([#180](https://github.com/NoiseByNorthwest/php-spx/issues/180))
- Added support for php 8.2.0 RC2 [#198](https://github.com/NoiseByNorthwest/php-spx/issues/198)
- Added PHP 8.2 support [#196](https://github.com/NoiseByNorthwest/php-spx/issues/196)

### Changed
- Documentation: Clarified what Inc. and Exc. mean in the table [#193](https://github.com/NoiseByNorthwest/php-spx/issues/193)

### Removed
- Travis CI [#195](https://github.com/NoiseByNorthwest/php-spx/issues/195)


## [v0.4.0](https://github.com/NoiseByNorthwest/php-spx/compare/v0.3.0...v0.4.0)

### Added
- Analysis screen: fix the NaNs in time grid when time goes above 1000s (fixes [#65](https://github.com/NoiseByNorthwest/php-spx/issues/65))
- Add sampling profiling mode ([#69](https://github.com/NoiseByNorthwest/php-spx/pull/69))
- Add memory allocator related metrics ([#70](https://github.com/NoiseByNorthwest/php-spx/pull/70))
- CLI: Add color scheme to flat profile ([#77](https://github.com/NoiseByNorthwest/php-spx/pull/77))
- Add PHP 7.3 support ([#80](https://github.com/NoiseByNorthwest/php-spx/pull/80))

### Changed
- Web UI: remove request-URI based routing ([#76](https://github.com/NoiseByNorthwest/php-spx/pull/76))

### Fixed
- Fix build on macOS 10.11- (fixes [#78](https://github.com/NoiseByNorthwest/php-spx/pull/78))


## [v0.3.0](https://github.com/NoiseByNorthwest/php-spx/compare/v0.2.0...v0.3.0)

### Added
- Improve documentation around web UI requirements (fixes [#54](https://github.com/NoiseByNorthwest/php-spx/issues/54))
- Web UI: Add custom color scheme management (@orls [#46](https://github.com/NoiseByNorthwest/php-spx/pull/46))
- Control panel: output command line prefix according to current setup
- Analysis screen: improve loading performance a bit
- Analysis screen: improve loading progress dialog
- Analysis screen: add cumulative cost computation and fix flat profile
- percentage columns (fixes [#41](https://github.com/NoiseByNorthwest/php-spx/issues/41))
- Analysis screen: display bars in flat profile percentage columns (fixes [#40](https://github.com/NoiseByNorthwest/php-spx/issues/40))
- Add request shutdown tracing
- Add script compilation tracing
- Metrics: add GC run count
- Metrics: add GC collected cycles count
- Metrics: add included file count
- Metrics: add included line count
- Metrics: add user class count
- Metrics: add user function count
- Metrics: add user opcode count

### Fixed
- Fix ownership issue (to stick to PHP coding standard)
- Fix call of non reentrant `exit()` function in a signal handler (fixes [#42](https://github.com/NoiseByNorthwest/php-spx/issues/42))
- Web UI / analysis screen: improved layout


## [v0.2.0](https://github.com/NoiseByNorthwest/php-spx/compare/v0.1.2...v0.2.0)

### Added
- Add web UI ([#14](https://github.com/NoiseByNorthwest/php-spx/pull/14))
- Add cookie based settings ([#3](https://github.com/NoiseByNorthwest/php-spx/issues/3))
- Improve Linux timer precision (1us -> 1ns)
- Improve accuracy by evaluating and then substracting probes overhead

## Changed
- Remove Google Trace Event report type ([#10](https://github.com/NoiseByNorthwest/php-spx/issues/10))
- Remove Callgrind report type ([#11](https://github.com/NoiseByNorthwest/php-spx/issues/11))


## [v0.1.2](https://github.com/NoiseByNorthwest/php-spx/compare/v0.1.2...v0.1.2)

### Added
- Add macOS support ([orls](https://github.com/orls) in [#13](https://github.com/NoiseByNorthwest/php-spx/pull/13))
