"""Evidence integrity and signal checks; these do not run OBS or Chimera DSP."""
import copy
import importlib.util
import json
from pathlib import Path
import re
import struct
import tempfile
import unittest
import wave

SPEC = importlib.util.spec_from_file_location("obs_testkit", Path(__file__).parents[1] / "Tools/obs_testkit.py")
kit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(kit)


class OBSTestKitTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.plugin = self.root / "Synthetic.vst3"
        self.plugin.mkdir()
        (self.plugin / "fixture.bin").write_bytes(b"test fixture, not a plugin")
        self.metadata = self.root / "build.json"
        self.metadata.write_text(json.dumps({"source_sha": "a" * 40, "version": "test-only"}))
        self.env = dict(obs_version="test-only", atkaudio_version="test-only",
                        os_arch="synthetic", audio_path="synthetic; no host")

    def initialize(self):
        output = self.root / "session"
        return output, kit.initialize(output, self.plugin, self.metadata, self.env)

    def evidence(self, output):
        path = output / "evidence" / "test-note.txt"
        path.write_text("Synthetic test evidence; no host executed.")
        return {"path": "evidence/test-note.txt", "sha256": kit.digest(path)}

    def test_initial_session_is_blocked_and_paths_are_relative(self):
        output, session = self.initialize()
        summary = kit.validate(session, output)
        self.assertEqual(summary["counts"]["BLOCKED"], 13)
        self.assertFalse(summary["host_compatibility_certified"])
        self.assertNotIn(str(self.root), json.dumps(session))

    def test_channel_routing_and_marker_positions(self):
        files = kit.signals(self.root / "signals")
        self.assertEqual(len(files), 2)
        with wave.open(str(self.root / "signals/stereo-routing.wav")) as wav:
            self.assertEqual((wav.getnchannels(), wav.getframerate(), wav.getnframes()), (2, 48000, 384000))
            frames = list(struct.iter_unpack("<hh", wav.readframes(wav.getnframes())))
        self.assertTrue(all(l == r == 0 for l, r in frames[:48000]))
        self.assertTrue(any(l != 0 for l, r in frames[48000:96000]))
        self.assertTrue(all(r == 0 for l, r in frames[48000:96000]))
        self.assertTrue(all(l == 0 for l, r in frames[144000:192000]))
        self.assertTrue(any(r != 0 for l, r in frames[144000:192000]))
        self.assertEqual(frames[240000], (3277, 3277))
        self.assertEqual(frames[288000], (3277, 3277))
        self.assertEqual(frames[240001], (0, 0))

    def test_low_bursts_have_silent_gaps_and_limited_level(self):
        kit.signals(self.root / "signals")
        with wave.open(str(self.root / "signals/low-a-b-bursts.wav")) as wav:
            self.assertEqual(wav.getnchannels(), 1)
            values = [v[0] for v in struct.iter_unpack("<h", wav.readframes(wav.getnframes()))]
        self.assertLessEqual(max(map(abs, values)), 3277)
        self.assertTrue(any(values[:24000]))
        self.assertFalse(any(values[24000:48000]))

    def test_pass_without_evidence_rejected_without_changing_file(self):
        output, _ = self.initialize()
        path = output / "session.json"
        original = path.read_bytes()
        with self.assertRaises(ValueError):
            kit.record(path, "scan_load", "PASS", "claim with no evidence", [])
        self.assertEqual(path.read_bytes(), original)

    def test_missing_and_duplicate_cases_rejected(self):
        output, session = self.initialize()
        for cases in (session["cases"][:-1], session["cases"] + [session["cases"][0]]):
            modified = copy.deepcopy(session)
            modified["cases"] = cases
            with self.assertRaises(ValueError):
                kit.validate(modified, output)

    def test_tampered_evidence_is_rejected(self):
        output, session = self.initialize()
        item = self.evidence(output)
        kit.record(output / "session.json", "scan_load", "PASS", "synthetic test", [item["path"]])
        session = json.loads((output / "session.json").read_text())
        kit.validate(session, output)
        (output / item["path"]).write_text("changed")
        with self.assertRaises(ValueError):
            kit.validate(session, output)

    def test_path_escape_and_absolute_evidence_rejected(self):
        output, _ = self.initialize()
        for name in ("../build.json", "evidence/../../build.json", str(self.metadata), "session.json"):
            with self.assertRaises(ValueError):
                kit.evidence_file(output, name)

    def test_symlink_evidence_rejected(self):
        output, _ = self.initialize()
        try:
            (output / "evidence/link").symlink_to(self.metadata)
        except OSError:
            self.skipTest("platform does not allow unprivileged symlinks")
        with self.assertRaises(ValueError):
            kit.evidence_file(output, "evidence/link")

    def test_one_hour_duration_required_and_all_pass_still_needs_review(self):
        output, session = self.initialize()
        item = self.evidence(output)
        for case in session["cases"]:
            case.update(result="PASS", note="synthetic fixture", evidence=[item])
        soak = next(x for x in session["cases"] if x["id"] == "one_hour_recording")
        for duration in (None, 3599, float("nan"), float("inf"), True):
            soak["duration_seconds"] = duration
            with self.assertRaises(ValueError):
                kit.validate(session, output)
        soak["duration_seconds"] = 3600
        result = kit.validate(session, output)
        self.assertEqual(result["result"], "REVIEW_REQUIRED")
        self.assertFalse(result["host_compatibility_certified"])

    def test_failure_has_priority_over_blocked(self):
        output, session = self.initialize()
        session["cases"][0].update(result="FAIL", evidence=[self.evidence(output)])
        self.assertEqual(kit.validate(session, output)["result"], "FAIL")

    def test_bundle_changes_change_fingerprint(self):
        first = kit.fingerprint(self.plugin)
        (self.plugin / "fixture.bin").write_bytes(b"changed")
        self.assertNotEqual(first, kit.fingerprint(self.plugin))
        (self.plugin / "extra.bin").write_bytes(b"extra")
        self.assertEqual(len(kit.fingerprint(self.plugin)["files"]), 2)

    def test_bad_identity_and_existing_directory_rejected(self):
        self.metadata.write_text('{"source_sha":"main","version":"test-only"}')
        with self.assertRaises(ValueError):
            self.initialize()
        self.metadata.write_text(json.dumps({"source_sha": "a" * 40, "version": "test-only"}))
        self.initialize()
        with self.assertRaises(FileExistsError):
            self.initialize()

    def chimera_session(self):
        output = self.root / "filter-session"
        env = dict(self.env)
        del env["atkaudio_version"]
        env["chimera_obs_version"] = "test-only"
        return output, kit.initialize(output, self.plugin, self.metadata, env, host="chimera-obs")

    @staticmethod
    def realtime_line(name, seconds, unprocessed=0, when="session total"):
        clock = f"{seconds // 3600}:{seconds // 60 % 60:02d}:{seconds % 60:02d}"
        return (f"12:00:00.000: [chimera-obs] '{name}': {when}: {clock} of audio, 168750 blocks, Chimera load average "
                f"3.1% peak 12.5%, 20 ms windows over real time 0, input gaps 2 (resumed after silence 1), "
                f"unprocessed blocks {unprocessed}")

    def test_log_pattern_matches_the_filter_format_string(self):
        # The pattern must follow the module's blog() format in plugin-main.cpp.
        source = (Path(__file__).parents[1] / "OBS/src/plugin-main.cpp").read_text(encoding="utf-8")
        body = source[source.index("void logStats("):source.index("void reportTask(")]
        literal = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', body.split("blog(", 1)[1].split(",\n         f->name")[0]))
        values = iter(["Guitar DI", "session total", "1:00:00", "168750", "3.1", "12.5", "0", "2", "1", "0"])
        line = re.sub(r"%(?:\.1f|llu|s)", lambda _: next(values), literal).replace("%%", "%")
        match = kit.REALTIME_LINE.search("[chimera-obs] " + line)
        self.assertIsNotNone(match, line)
        self.assertEqual(match["filter"], "Guitar DI")

    def test_chimera_filter_session_needs_its_own_version_field(self):
        output, session = self.chimera_session()
        self.assertEqual(session["scope"], "obs-chimera-filter-manual")
        self.assertEqual(kit.validate(session, output)["counts"]["BLOCKED"], 13)
        env = dict(self.env)
        with self.assertRaises(ValueError):
            kit.initialize(self.root / "other", self.plugin, self.metadata, env, host="chimera-obs")

    def test_obs_log_keeps_only_realtime_lines(self):
        output, _ = self.chimera_session()
        log = self.root / "obs.log"
        log.write_text("\ufeff" + "\n".join([
            "11:00:00.000: [chimera-obs] 'Guitar DI': loaded SpectralForge Chimera 1.3.2 from C:\\Users\\private\\x.vst3",
            self.realtime_line("Guitar DI", 600, when="realtime"),
            "11:30:00.000: stream key and other OBS lines",
            self.realtime_line("Guitar DI", 3720)]), encoding="utf-8")
        record = kit.realtime_log(log, output, "evidence/realtime.json")
        stored = (output / "evidence/realtime.json").read_text(encoding="utf-8")
        self.assertNotIn("private", stored)
        self.assertNotIn("stream key", stored)
        self.assertEqual(len(record["entries"]), 2)
        self.assertEqual(record["latest_per_filter"][0]["audio_seconds"], 3720)
        for bad in ("evidence/realtime.json", "../realtime.json", "evidence/realtime.txt", str(self.root / "x.json")):
            with self.assertRaises(ValueError):
                kit.realtime_log(log, output, bad)
        empty = self.root / "empty.log"
        empty.write_text("no filter lines")
        with self.assertRaises(ValueError):
            kit.realtime_log(empty, output, "evidence/none.json")

    def test_filter_hour_pass_needs_an_hour_of_processed_filter_audio(self):
        output, _ = self.chimera_session()
        path = output / "session.json"
        log = self.root / "obs.log"
        for name, seconds, unprocessed in (("short", 1800, 0), ("muted", 3700, 4), ("hour", 3700, 0)):
            log.write_text(self.realtime_line("Guitar DI", seconds, unprocessed), encoding="utf-8")
            kit.realtime_log(log, output, f"evidence/{name}.json")
        note = self.evidence(output)["path"]
        for evidence in ([note], ["evidence/short.json"], ["evidence/muted.json"]):
            with self.assertRaises(ValueError):
                kit.record(path, "one_hour_recording", "PASS", "synthetic", evidence, 3700)
        kit.record(path, "one_hour_recording", "PASS", "synthetic", [note, "evidence/hour.json"], 3700)


if __name__ == "__main__":
    unittest.main()
