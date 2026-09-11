"""Host-only failure record after the parent observes an interrupted UE process.

No Unreal import, process control or gameplay action. Does not rewrite a prior
report. Explicit call only; the parent supplies its actual observed exit result.
"""
import hashlib
import json
from pathlib import Path
import time


def record_interruption(output_directory, *, reason, process_id, process_exit_code=None):
    directory = Path(output_directory)
    source = directory/'checkpoint-reload.json'
    data = source.read_bytes()
    report = json.loads(data)
    assert report.get('qualified') is not True and report.get('status') != 'passed', 'Never relabel or overwrite a completed pass'
    assert isinstance(process_id, int) and process_id > 0
    assert isinstance(reason, str) and reason.strip()
    assert process_exit_code is None or isinstance(process_exit_code, int)
    result = dict(status='failed', qualified=False, reason=reason, detection='Parent observed process termination or interruption',
        process_id=process_id, process_exit_code=process_exit_code,
        recorded_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        last_observation_status=report.get('status'), last_observation_sha256=hashlib.sha256(data).hexdigest(),
        native_callback_count=len(report.get('callbacks', [])), prior_report_unchanged=True,
        qualification='No successful final checkpoint reload has been established.')
    destination = directory/'checkpoint-reload-termination.json'
    with destination.open('x', encoding='utf8') as stream:
        json.dump(result, stream, indent=2)
    return result
