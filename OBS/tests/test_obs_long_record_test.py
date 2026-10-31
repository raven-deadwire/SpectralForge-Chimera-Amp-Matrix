#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Regression checks for recording analysis; no OBS or audio device required."""
import contextlib
import io
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

import obs_long_record_test as recording

REAL_RUN = subprocess.run


def failed_decoder(_command, **kwargs):
    # A real child exits nonzero after printing plausible progress and a partial
    # PCM sample. Forward the caller's options, including check, unchanged.
    return REAL_RUN([sys.executable, "-c",
                     "import sys; sys.stdout.write('abc'); "
                     "sys.stderr.write('pts_time:30 time=01:01:00.00\\nConversion failed!\\n'); "
                     "sys.exit(1)"], **kwargs)


class RecordingFailureTests(unittest.TestCase):
    def test_duration_rejects_failed_decode_even_after_full_hour_progress(self):
        with patch.object(recording.subprocess, "run", side_effect=failed_decoder):
            with self.assertRaises(subprocess.CalledProcessError):
                recording.duration(Path("broken.mkv"))

    def test_failed_audio_decoder_stops_before_unpacking_partial_samples(self):
        with patch.object(recording.subprocess, "run", side_effect=failed_decoder):
            with self.assertRaises(subprocess.CalledProcessError):
                recording.audio_segment(Path("broken.mkv"), 30, 60)

    def test_successful_decoder_with_partial_sample_is_not_silently_trimmed(self):
        result = subprocess.CompletedProcess([], 0, struct.pack("<f", 0.5) + b"x", b"pts_time:30")
        with patch.object(recording.subprocess, "run", return_value=result):
            with self.assertRaises(struct.error):
                recording.audio_segment(Path("partial.mkv"), 30, 60)

    def test_no_audio_frames_have_a_specific_failure(self):
        result = subprocess.CompletedProcess([], 0, b"", b"")
        with patch.object(recording.subprocess, "run", return_value=result):
            with self.assertRaisesRegex(RuntimeError, "no audio frames"):
                recording.audio_segment(Path("empty.mkv"), 30, 60)

    def test_silent_start_and_end_stop_at_marker_check(self):
        for start in (30.0, 3595.26):
            with self.subTest(start=start):
                flashes = [start + 2 * i for i in range(30)]
                with patch.object(recording, "video_onsets", return_value=flashes), \
                        patch.object(recording, "audio_segment", return_value=(start, [0.0] * 48000)):
                    with self.assertRaisesRegex(RuntimeError, "only 0 audio/video markers paired.*0 bursts"):
                        recording.sync(Path("silent.mkv"), start)

    def test_cli_returns_failure_and_cleans_up_for_decoder_and_silence_errors(self):
        errors = (subprocess.CalledProcessError(1, "ffmpeg"), struct.error("partial sample"),
                  RuntimeError("only 0 audio/video markers paired (30 flashes, 0 bursts)"))
        with tempfile.TemporaryDirectory() as directory:
            for error in errors:
                with self.subTest(error=type(error).__name__):
                    layout = Mock(config=Path(directory) / "config")
                    argv = ["obs_long_record_test.py", "--module", "module", "--vst3", "plugin",
                            "--work", directory]
                    with patch.object(sys, "argv", argv), patch.dict(os.environ, {"DISPLAY": ":test"}), \
                            patch.object(recording.app, "Layout", return_value=layout), \
                            patch.object(recording, "run", side_effect=error), \
                            contextlib.redirect_stderr(io.StringIO()) as stderr:
                        self.assertEqual(recording.main(), 1)
                    self.assertIn("FAIL", stderr.getvalue())
                    self.assertNotIn("Traceback", stderr.getvalue())
                    layout.uninstall.assert_called_once_with(False)


class RealFfmpegTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.markers = Path(cls.temporary.name) / "markers.mkv"
        recording.write_markers(cls.markers, 124)

    def test_marker_decode_duration_and_sync(self):
        self.assertGreaterEqual(recording.duration(self.markers), 123.9)
        for start in (30, 60):
            offset, spread, pairs = recording.sync(self.markers, start)
            self.assertLess(abs(offset), 0.002)
            self.assertLess(spread, 0.002)
            self.assertGreaterEqual(pairs, 28)
            self.assertGreater(recording.burst_level(self.markers, start), 0)

    def test_silent_recording_is_rejected_with_real_ffmpeg(self):
        silent = Path(self.temporary.name) / "silent.mkv"
        REAL_RUN([recording.app.FFMPEG, "-v", "error", "-y", "-i", str(self.markers),
                  "-af", "volume=0", "-c:v", "copy", "-c:a", "flac", str(silent)], check=True)
        with self.assertRaisesRegex(RuntimeError, "only 0 audio/video markers paired.*0 bursts"):
            recording.sync(silent, 30)


if __name__ == "__main__":
    unittest.main()
