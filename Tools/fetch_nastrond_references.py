#!/usr/bin/env python3
"""Acquire the exact public reference inputs; verify SHA256, never redistribute.

Save outside the repository. T3K attribution and source qualifications are in
docs/reference/nastrond-nam-manifest.json. Downloads need no account credentials.
Compatibility entry point for `nastrond_nam_tool.py fetch`.
"""
import sys

import nastrond_nam_tool


if __name__ == '__main__':
    nastrond_nam_tool.main(['fetch'] + sys.argv[1:])
