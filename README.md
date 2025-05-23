# Qapla Engine Tester

**Version**: Prerelease 0.1.0  
**Author**: Volker Boehm

A tool for automated testing of UCI-compatible chess engines. It verifies stability, protocol compliance, memory behavior, and time control usage through simulated scenarios and self-play games.

## Background

This tool is designed to test UCI-compatible chess engines. An engine is considered well-tested if it runs reliably under all major GUIs and CLI tools without issues.  
This is a first prerelease version — feedback and bug reports are very welcome!

I'm especially interested in engines with **known bugs**. If you point me to one, I can check whether Qapla would have detected the problem. Bugs **not yet detected** by this tester are of particular interest.

Please use GitHub Issues to report findings, suggestions, or problems.

## Installation

No installation required. Just download and run.

## Usage

You can run the tester with or without command-line arguments. If required parameters are missing, it will prompt for them interactively.

## Features

- Automated function tests (startup, options, move generation)
- Parallel engine startup and shutdown
- Self-play games with varying time controls
- Detailed logging of engine communication and test results
- Configurable via command-line options

## Requirements

- Engine must support the UCI protocol
- Windows or Linux (depending on the build)
- Engine must be directly executable via a path

## Command-line Options

**Important!**: a "=" sign is required between the option and its value. For example, `--engine=/path/to/engine` is correct, while `--engine /path/to/engine` is not.

You can call it without any parameters. In this case, the program will prompt you for the required parameters. Enter accepts the default value. 

| Option                  | Description                                                   | Required | Default |
|-------------------------|---------------------------------------------------------------|----------|---------|
| `--help`                | Shows help and exits                                          | -        | —       |
| `--engine`              | Path to the engine executable                                 | Yes      | —       |
| `--concurrency`         | Maximum number of engines running in parallel                 | Yes      | 20      |
| `--games-number`        | Number of games to play                                       | No       | 20      |
| `--logpath`             | Path to directory for logs                                    | No       | `.`     |
| `--testlevel`           | Test level (0 = all, 1 = basic, 2 = advanced)                 | No       | 0       |

## Example Usage

```bash
QaplaTester --engine=/path/to/engine --concurrency=10
