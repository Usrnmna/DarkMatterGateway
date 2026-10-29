"""Exercise fail-closed CI selection and real process/report failure propagation."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from ci_report import Report
from ci_scope import FLAG_FOR_JOB, changed_files, gate, plan
from test_host import cpp_suites


def results_for(selected):
    """Represent completed jobs as GitHub's needs context does."""
    results = {"change-scope": {"result": "success", "outputs": {"plan": json.dumps(selected)}}}
    results.update({job: {"result": "success" if selected[flag] else "skipped"}
                    for job, flag in FLAG_FOR_JOB.items()})
    return results


class ScopeTests(unittest.TestCase):
    def test_docs_only_skips_only_expensive_jobs(self):
        selected = plan("pull_request", {}, ["docs/build.md", "README.md"])
        self.assertTrue(selected["python"] and selected["cpp"])
        self.assertFalse(selected["firmware"] or selected["mutations"])
        self.assertEqual(gate(results_for(selected)), [])

    def test_uncertain_and_source_changes_require_deep_checks(self):
        for changed in (None, ["firmware/main/app_main.cpp"], ["tools/test-host.sh"],
                        [".github/workflows/host-checks.yml"], ["unknown-file"]):
            with self.subTest(changed=changed):
                selected = plan("push", {}, changed)
                self.assertTrue(selected["firmware"] and selected["mutations"])

    def test_schedules_are_full_and_cold(self):
        selected = plan("schedule", {}, None)
        self.assertTrue(selected["cold_cache"] and selected["firmware"])
        self.assertFalse(selected["diagnostic"])

    def test_manual_selection_cannot_satisfy_full_gate(self):
        for inputs in ({"suite": "ir"}, {"suite": "full", "compiler": "clang"}):
            selected = plan("workflow_dispatch", {"inputs": inputs}, None)
            self.assertTrue(selected["diagnostic"])
            self.assertTrue(gate(results_for(selected)))
            self.assertEqual(gate(results_for(selected), diagnostic=True), [])

    def test_all_bad_worker_results_fail_the_gate(self):
        selected = plan("push", {}, None)
        for job in FLAG_FOR_JOB:
            for result in ("failure", "cancelled", "skipped", "", None):
                with self.subTest(job=job, result=result):
                    needs = results_for(selected)
                    needs[job]["result"] = result
                    self.assertTrue(gate(needs))

    def test_scope_failure_and_missing_outputs_fail(self):
        needs = results_for(plan("push", {}, None))
        for replacement in ({"result": "failure"}, {"result": "success"},
                            {"result": "success", "outputs": {"plan": "null"}}):
            needs["change-scope"] = replacement
            self.assertTrue(gate(needs))

    def test_incomplete_full_plan_is_rejected(self):
        original = plan("push", {}, None)
        for key, value in (("python", False), ("cpp", "true"), ("repository", False),
                           ("compilers", ["g++"])):
            selected = dict(original, **{key: value})
            self.assertTrue(gate(results_for(selected)))

    def test_missing_worker_and_optional_failure_are_rejected(self):
        needs = results_for(plan("push", {}, ["README.md"]))
        del needs["python-tools"]
        self.assertTrue(gate(needs))
        needs = results_for(plan("push", {}, ["README.md"]))
        needs["firmware-build"]["result"] = "failure"
        self.assertTrue(gate(needs))

    def test_new_branch_and_failed_comparison_run_everything(self):
        self.assertIsNone(changed_files("push", {"before": "0" * 40}))
        with patch("ci_scope.subprocess.run", return_value=subprocess.CompletedProcess([], 128, b"")):
            self.assertIsNone(changed_files("push", {"before": "a" * 40}))

    def test_diff_keeps_both_paths_of_renames(self):
        with patch("ci_scope.subprocess.run", return_value=subprocess.CompletedProcess(
                [], 0, b"firmware/old.cpp\0docs/new.md\0")) as runner:
            self.assertEqual(changed_files("pull_request", {"pull_request": {"base": {"sha": "b" * 40}}}),
                             ["firmware/old.cpp", "docs/new.md"])
            self.assertIn("--no-renames", runner.call_args.args[0])


class ReportingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ci-report-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        environment = patch.dict(os.environ, {"GITHUB_STEP_SUMMARY": str(self.directory / "github-summary.md")})
        environment.start()
        self.addCleanup(environment.stop)

    def test_failure_survives_later_success_and_produces_summary(self):
        report = Report(self.directory / "report", "Failure propagation")
        self.assertFalse(report.run("failure", [sys.executable, "-c", "print('diagnostic marker'); raise SystemExit(7)"]))
        self.assertTrue(report.run("success", [sys.executable, "-c", "pass"]))
        self.assertEqual(report.finish(), 1)
        results = json.loads((report.directory / "results.json").read_text())["results"]
        self.assertEqual(results[0]["returncode"], 7)
        self.assertIn("diagnostic marker", (report.directory / results[0]["log"]).read_text())
        self.assertIn("failed", (self.directory / "github-summary.md").read_text())

    def test_timeout_and_missing_executable_are_failures(self):
        report = Report(self.directory / "report", "Timeout")
        self.assertFalse(report.run("timeout", [sys.executable, "-c", "import time; time.sleep(30)"], timeout=1))
        self.assertFalse(report.run("missing", [str(self.directory / "missing-program")]))
        self.assertEqual([r["returncode"] for r in report.results], [124, 127])
        self.assertEqual(report.finish(), 1)

    def test_empty_or_interrupted_report_cannot_pass(self):
        report = Report(self.directory / "report", "Incomplete")
        self.assertEqual(report.finish(), 1)
        report.results.append({"name": "interrupted", "status": "running", "log": "missing.log"})
        self.assertEqual(report.finish(), 1)

    def test_real_python_failure_is_recorded_with_test_identity(self):
        source = self.directory / "sample"
        source.mkdir()
        (source / "test_failure.py").write_text(
            "import unittest\nclass Failure(unittest.TestCase):\n"
            "    def test_regression(self):\n        self.fail('intentional proof failure')\n")
        output = self.directory / "tests.json"
        result = subprocess.run([sys.executable, str(ROOT / "tools/test_python.py"),
                                 "--start-directory", str(source), "--output", str(output)], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        record = json.loads(output.read_text())["tests"][0]
        self.assertEqual(record["status"], "failed")
        self.assertIn("test_regression", record["test"])
        self.assertIn("intentional proof failure", record["detail"])

    def test_cpp_inventory_preserves_all_variants(self):
        suites = cpp_suites("generated")
        self.assertEqual(len(suites), 14)
        self.assertEqual(len({suite[1] for suite in suites}), 14)
        self.assertTrue({"proflame2-target-disabled", "proflame2-target-unqualified", "terminal-0", "terminal-1"}
                        <= {suite[1] for suite in suites})


if __name__ == "__main__":
    unittest.main()
