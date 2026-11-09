#!/usr/bin/env python3
"""Entry kept for the previous command. The alignment run is align_vad.py."""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
raise SystemExit(subprocess.call([sys.executable, str(root / "sim" / "vad" / "align_vad.py")]))
