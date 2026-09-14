"""Retry only the terminal E4A input pilot, preserving all actual gameplay state."""
import os
from pathlib import Path
import continue_aurelion_e4a_input as pilot

previous = pilot._RUN
assert previous and previous.done and previous.report['status'] == 'failed'
assert 'Normal weapon-wheel selection exceeded' in previous.report.get('error', '')
output = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'E4AInputRetry'
assert not output.exists(), 'Preserve prior evidence'
# Production Run.initialize rechecks actual journal, protagonist, roster, phase,
# resources and command ownership. This does not restore a save or change them.
pilot.start(output)
