# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.5.0] - 2026-01-18

### Added

- **Per-instance engine logging**: New `--each logmode=one|each` CLI option
  - `logmode=one` (default): All engines log to a single file
  - `logmode=each`: Each engine instance creates separate log files

- **Fixed:** Log file now includes sprt decision.


