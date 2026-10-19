"""Only bounded calibration numbers may leave the USB coordinator."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from run_gemini_device import STATUS


class EnrollmentStatusTests(unittest.TestCase):
    def test_calibration_numbers_are_accepted(self):
        self.assertIsNotNone(STATUS.fullmatch('GEM ENROLL CAL rms=1600 threshold=4800 ms=1100 gap=700'))
        self.assertIsNotNone(STATUS.fullmatch('GEM ENROLL CAL NONE'))
        self.assertIsNotNone(STATUS.fullmatch('GEM ENROLL result=NONE'))
        self.assertIsNotNone(STATUS.fullmatch('GEM BUILD enrollment=3'))

    def test_extra_text_and_unbounded_values_are_rejected(self):
        for value in ('GEM ENROLL CAL rms=quiet threshold=4800 ms=1100 gap=700',
                      'GEM ENROLL CAL rms=1600 threshold=4800 ms=1100 gap=700 extra=data',
                      'GEM ENROLL CAL rms=160000 threshold=4800 ms=1100 gap=700',
                      'GEM BUILD enrollment=3 extra=data'):
            with self.subTest(value=value):
                self.assertIsNone(STATUS.fullmatch(value))


if __name__ == '__main__':
    unittest.main()
