"""Retry ordinary inputs after a terminal hold timeout; retain native encounter state."""
import os
from pathlib import Path
import importlib
import continue_aurelion_e4b_input as pilot

previous = pilot._RUN
assert previous and previous.done and previous.report['status'] == 'failed'
assert 'Native contextual hold did not complete' in previous.report.get('error', '')
output = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'E4BFocusRetry'
assert not output.exists()
# Load diagnostic changes only after the previous driver released input/delegates.
importlib.reload(pilot)
# start() rechecks actual journal, attempt, roster, resources and ownership.
pilot.start(output)
