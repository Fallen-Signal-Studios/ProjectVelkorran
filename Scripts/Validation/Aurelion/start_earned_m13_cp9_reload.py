"""Console entry for the public CP9 reload after a passed retained route.

Run only in that same Unreal Editor Python session. The imported observer
checks the earned M13 report and requests one public checkpoint load.
"""
import os
from pathlib import Path
from uuid import uuid4

import probe_m13_native_checkpoint_reload as cp9

output = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / ('CP9Reload-' + uuid4().hex[:8])
run = cp9.start(output)
print('AURELION_CP9_RELOAD_STARTED', output, run.report['status'])
