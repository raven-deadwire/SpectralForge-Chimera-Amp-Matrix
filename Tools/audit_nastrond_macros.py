#!/usr/bin/env python3
"""Audit like-for-like NAM macro measurements; never certify hardware/DI tone.

Compatibility entry point for `nastrond_nam_tool.py audit` with the v1 metric,
the frozen v1 states and target groups derived from the reference manifest.
"""
import sys

import nastrond_nam_tool


if __name__ == '__main__':
    nastrond_nam_tool.main(['audit'] + sys.argv[1:])
