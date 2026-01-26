#!/usr/bin/env python3
"""SPRT Parameter Optimization Script

Optimizes UCI parameters using binary search with SPRT tournaments.
"""

import argparse
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple


class SPRTOptimizer:
    """Optimizes UCI parameters using SPRT tournaments with binary search."""

    def __init__(
        self,
        exec_path: str,
        settings_file: str,
        parameter_name: str,
        engine_a_name: str,
        engine_b_name: str,
        engine_cmd: str,
        log_file: str,
        min_value: int,
        max_value: int,
        initial_value: int,
        concurrency: int = 16,
    ):
        self.exec_path = Path(exec_path)
        self.settings_file = settings_file
        self.parameter_name = parameter_name
        self.engine_a_name = engine_a_name
        self.engine_b_name = engine_b_name
        self.engine_cmd = engine_cmd
        self.log_file = Path(log_file)
        self.min_value = min_value
        self.max_value = max_value
        self.initial_value = initial_value
        self.concurrency = concurrency

        # Ensure log directory exists
        self.log_file.parent.mkdir(parents=True, exist_ok=True)

        # Results storage
        self.test_results: List[Dict] = []

    def write_log(self, message: str) -> None:
        """Write message to log file with timestamp."""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_message = f"[{timestamp}] {message}\n"
        with open(self.log_file, "a", encoding="utf-8") as f:
            f.write(log_message)

    def test_sprt_value(self, test_value: int, baseline_value: int) -> Dict:
        """Run SPRT test comparing test_value against baseline_value.

        Returns dict with test results including exit code and improvement status.
        """
        self.write_log("")
        self.write_log("=" * 40)
        self.write_log(f"Testing: {test_value} vs {baseline_value} (baseline)")
        self.write_log("=" * 40)

        cmd = [
            str(self.exec_path),
            f"--settingsfile={self.settings_file}",
            f"--concurrency={self.concurrency}",
            "--engine",
            f"name={self.engine_b_name}",
            f"cmd={self.engine_cmd}",
            "gauntlet=false",
            f"option.{self.parameter_name}={baseline_value}",
            "--engine",
            f"name={self.engine_a_name}",
            f"cmd={self.engine_cmd}",
            "gauntlet=true",
            f"option.{self.parameter_name}={test_value}",
        ]

        self.write_log(f"Command: {' '.join(cmd)}")
        self.write_log("")

        # Run command and stream output to terminal
        result = subprocess.run(cmd, capture_output=False, text=True)
        exit_code = result.returncode

        # Map exit code to result
        result_map = {
            0: "NoError",
            14: "H1_Accepted",
            15: "H0_Accepted",
            16: "Undecided",
            10: "EngineError",
            11: "EngineMissbehaviour",
        }
        result_name = result_map.get(exit_code, f"Unknown_{exit_code}")

        self.write_log("")
        self.write_log(f"Result: {result_name} (Exit Code: {exit_code})")

        return {
            "test_value": test_value,
            "baseline_value": baseline_value,
            "exit_code": exit_code,
            "result": result_name,
            "is_improvement": exit_code == 14,
            "is_error": exit_code in [10, 11],
        }

    def optimize(
        self, resume_test_values: List[int] = None, resume_results: List[str] = None
    ) -> int:
        """Run optimization using binary search.

        Returns the optimal parameter value found.
        """
        self.write_log("=" * 40)
        self.write_log("Starting SPRT Optimization")
        self.write_log(f"Parameter: {self.parameter_name}")
        self.write_log(f"Range: {self.min_value} - {self.max_value}")
        self.write_log(f"Initial: {self.initial_value}")

        self.write_log("=" * 45)
        self.write_log(f"SPRT Parameter Optimization: {self.parameter_name}")
        self.write_log("=" * 45)
        self.write_log(f"Initial value: {self.initial_value}")
        self.write_log(f"Search range: {self.min_value} - {self.max_value}")
        self.write_log("")

        # Initialize: best value and two intervals where maximum can be
        best_value = self.initial_value
        interval1_min = self.min_value
        interval1_max = self.initial_value
        interval2_min = self.initial_value
        interval2_max = self.max_value

        # Check for resume
        if resume_test_values and resume_results:
            if len(resume_test_values) != len(resume_results):
                raise ValueError("Resume test values and results must have same length")

            self.write_log("=== RESUMING from previous session ===")
            self.write_log(f"Restoring {len(resume_test_values)} previous test(s)")

            # Replay all previous tests to reconstruct intervals
            for i, (test_val, result) in enumerate(
                zip(resume_test_values, resume_results), 1
            ):
                baseline_val = best_value

                self.write_log(f"  Test {i}: {test_val} vs {baseline_val} → {result}")

                if result == "better":
                    # New best found - update intervals
                    if test_val > best_value:
                        # Was testing upper interval (interval2)
                        interval1_min = best_value
                        interval1_max = test_val
                        interval2_min = test_val
                        # interval2_max unchanged
                    else:
                        # Was testing lower interval (interval1)
                        interval2_min = best_value
                        interval2_max = interval1_max
                        interval1_max = test_val
                        # interval1_min unchanged
                    best_value = test_val
                else:
                    # Not better - shrink interval
                    if test_val > best_value:
                        # Was testing upper interval (interval2)
                        interval2_max = test_val
                    else:
                        # Was testing lower interval (interval1)
                        interval1_min = test_val

            self.write_log(f"Current best: {best_value}")
            self.write_log(
                f"Interval 1: [{interval1_min} - {interval1_max}] "
                f"(size: {interval1_max - interval1_min})"
            )
            self.write_log(
                f"Interval 2: [{interval2_min} - {interval2_max}] "
                f"(size: {interval2_max - interval2_min})"
            )

        test_count = len(resume_test_values) if resume_test_values else 0

        # Main search loop
        while True:
            test_count += 1

            # Calculate interval sizes
            size1 = interval1_max - interval1_min
            size2 = interval2_max - interval2_min

            self.write_log("")
            self.write_log(f"=== Round {test_count} ===")
            self.write_log(f"Best value: {best_value}")
            self.write_log(f"Interval 1: [{interval1_min} - {interval1_max}] (size: {size1})")
            self.write_log(f"Interval 2: [{interval2_min} - {interval2_max}] (size: {size2})")

            # Stop if both intervals too small
            if size1 <= 50 and size2 <= 50:
                self.write_log("Both intervals <= 50. Optimization complete.")
                break

            # Choose larger interval and calculate midpoint
            if size1 > size2:
                test_value = (interval1_min + interval1_max) // 2
                self.write_log(
                    f"Choosing Interval 1 (larger): Testing midpoint {test_value}"
                )
            else:
                test_value = (interval2_min + interval2_max) // 2
                self.write_log(
                    f"Choosing Interval 2 (larger): Testing midpoint {test_value}"
                )

            # Avoid testing best value itself
            if test_value == best_value:
                self.write_log("Midpoint equals best value. Optimization complete.")
                break

            # Run test
            result = self.test_sprt_value(test_value, best_value)

            if result["is_error"]:
                self.write_log("Engine error. Aborting.")
                break

            # Update intervals based on result
            if result["is_improvement"]:
                self.write_log(f"→ BETTER! New best: {test_value}")

                if test_value > best_value:
                    # Was testing upper interval (interval2)
                    interval1_min = best_value
                    interval1_max = test_value
                    interval2_min = test_value
                    # interval2_max unchanged
                else:
                    # Was testing lower interval (interval1)
                    interval2_min = best_value
                    interval2_max = interval1_max
                    interval1_max = test_value
                    # interval1_min unchanged
                best_value = test_value
            else:
                self.write_log("→ Not better")

                if test_value > best_value:
                    # Was testing interval2 - shrink upper bound
                    interval2_max = test_value
                else:
                    # Was testing interval1 - shrink lower bound
                    interval1_min = test_value

        # Final summary
        self.write_log("")
        self.write_log("=" * 45)
        self.write_log("OPTIMIZATION COMPLETE")
        self.write_log("=" * 45)
        self.write_log(f"Optimal value: {best_value}")
        self.write_log("=" * 45)

        return best_value


def main():
    """Main entry point for the optimization script."""
    parser = argparse.ArgumentParser(
        description="Optimize UCI parameters using SPRT tournaments"
    )
    parser.add_argument(
        "--resume-test-values",
        type=int,
        nargs="+",
        default=[],
        help="Test values from previous session to resume from",
    )
    parser.add_argument(
        "--resume-results",
        type=str,
        nargs="+",
        default=[],
        help="Results from previous session (better/notbetter)",
    )
    parser.add_argument(
        "--exec-path",
        type=str,
        default=r".\build\release\qapla-engine-tester.exe",
        help="Path to qapla-engine-tester executable",
    )
    parser.add_argument(
        "--settings-file",
        type=str,
        default=r"test\tournaments\sprt.ini",
        help="Path to settings file",
    )
    parser.add_argument(
        "--parameter",
        type=str,
        default="pstKingEgScale",
        help="UCI parameter name to optimize",
    )
    parser.add_argument(
        "--engine-cmd",
        type=str,
        default=r"C:\Development\Qapla2\x64\Release\Qapla.exe",
        help="Path to engine executable",
    )
    parser.add_argument(
        "--log-file",
        type=str,
        default=r"test\results\optimize-sprt-log.txt",
        help="Path to log file",
    )
    parser.add_argument(
        "--min-value", type=int, default=300, help="Minimum parameter value"
    )
    parser.add_argument(
        "--max-value", type=int, default=1000, help="Maximum parameter value"
    )
    parser.add_argument(
        "--initial-value", type=int, default=500, help="Initial parameter value"
    )
    parser.add_argument(
        "--concurrency", type=int, default=16, help="Number of concurrent games"
    )

    args = parser.parse_args()

    optimizer = SPRTOptimizer(
        exec_path=args.exec_path,
        settings_file=args.settings_file,
        parameter_name=args.parameter,
        engine_a_name="Qapla 0.4.0 A",
        engine_b_name="Qapla 0.4.0 B",
        engine_cmd=args.engine_cmd,
        log_file=args.log_file,
        min_value=args.min_value,
        max_value=args.max_value,
        initial_value=args.initial_value,
        concurrency=args.concurrency,
    )

    optimal_value = optimizer.optimize(
        resume_test_values=args.resume_test_values if args.resume_test_values else None,
        resume_results=args.resume_results if args.resume_results else None,
    )

    print(f"\nOptimal value: {optimal_value}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
