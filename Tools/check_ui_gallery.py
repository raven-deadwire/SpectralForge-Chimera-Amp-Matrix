#!/usr/bin/env python3
"""Check gallery file completeness only; does not certify visual acceptance."""
import argparse
from pathlib import Path
import struct

GLOBS = ('PRE-types-*.png', 'POST-native-*.png', 'POST-fx-*.png')
# Inventory emitted by ChimeraUITests: eight five-slot PRE batches,
# three choices in each native POST section, and 6/3/3 POST FX choices.
EXPECTED = (
    tuple(f'PRE-types-{batch}.png' for batch in range(1, 9))
    + tuple(f'POST-native-{section}-{choice}.png'
            for section in range(3) for choice in range(3))
    + tuple(f'POST-fx-{section}-{choice}.png'
            for section, count in ((3, 6), (4, 3), (5, 3))
            for choice in range(count))
)


def check(directory):
    errors = []
    for name in EXPECTED:
        path = Path(directory) / name
        if not path.is_file():
            errors.append(f'missing: {name}')
            continue
        # Recognize a nonempty PNG header without adding an image dependency.
        # Actual PNG encoding remains checked by the JUCE producer.
        with path.open('rb') as stream:
            header = stream.read(24)
        if (len(header) != 24 or header[:8] != b'\x89PNG\r\n\x1a\n'
                or header[8:16] != b'\x00\x00\x00\x0dIHDR'
                or 0 in struct.unpack('>II', header[16:24])):
            errors.append(f'invalid PNG header: {name}')
    if errors:
        raise ValueError('\n'.join(errors))
    return len(EXPECTED)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        count = check(args.directory)
    except (ValueError, OSError) as error:
        parser.exit(1, f'FAIL: pedal/rack gallery file contract\n{error}\n')
    print(f'PASS: pedal/rack gallery file contract ({count} PNG headers; no visual acceptance verdict)')


if __name__ == '__main__':
    main()
