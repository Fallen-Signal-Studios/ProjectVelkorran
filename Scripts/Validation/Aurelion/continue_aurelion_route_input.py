"""Chain existing input drivers; no input injection or gameplay writes here."""
import importlib
import json
import os
from pathlib import Path
import time
import traceback
import unreal
from continue_aurelion_e1_input import replace_report_with_retry

_RUN = None
STAGES = ('continue_aurelion_e2_input', 'continue_aurelion_e3_entry_input',
          'continue_aurelion_e3_rescue_input', 'continue_aurelion_e4_entry_input',
          'continue_aurelion_e4a_input', 'continue_aurelion_e4b_input',
          'continue_aurelion_m13_entry_input', 'continue_aurelion_m13_input')


class Run:
    def __init__(self, output_directory):
        self.out = Path(output_directory)
        self.out.mkdir(parents=True, exist_ok=True)
        self.done = False
        self.handle = None
        self.child = self.module = None
        self.index = -1
        self.started = time.monotonic()
        self.report = dict(status='running', stages=[], pie_left_running=True,
                           scope='Existing E1 pass through E2, E3 rescue, E4 phases, native M13 travel and separate departures; only passed child reports qualify their stages')

    def write(self):
        self.report['elapsed_seconds'] = round(time.monotonic()-self.started, 3)
        temp = self.out/'route-input-chain.tmp'
        temp.write_text(json.dumps(self.report, indent=2, default=str), encoding='utf8')
        replace_report_with_retry(temp, self.out/'route-input-chain.json')

    def next_stage(self):
        self.index += 1
        name = STAGES[self.index]
        self.module = importlib.import_module(name)
        assert self.module._RUN is None or self.module._RUN.done, 'Another driver is active: '+name
        self.child = self.module.start(self.out/name)
        assert self.module._RUN is self.child, 'Driver did not publish its actual run'
        self.report['stages'].append(dict(module=name, status='running', output=str(self.child.out)))
        self.write()

    def finish(self, passed, reason):
        if self.done:
            return
        self.done = True
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        # Only the child owns and releases its inputs. Never stop PIE or another run.
        if self.child is not None and not self.child.done and self.module._RUN is self.child:
            self.module.stop()
        self.report.update(status='passed' if passed else 'failed', reason=reason)
        self.write()
        unreal.log('Aurelion route input chain: '+self.report['status']+': '+reason)

    def tick(self, _delta):
        if self.done:
            return
        try:
            assert self.module._RUN is self.child, 'Active child identity changed'
            if not self.child.done:
                return
            assert self.child.handle is None, 'Completed child input callback remains registered'
            status = self.child.report.get('status')
            self.report['stages'][-1].update(status=status, reason=self.child.report.get('reason'))
            if status != 'passed':
                self.finish(False, 'Child stopped without a pass: '+STAGES[self.index])
            elif self.index+1 == len(STAGES):
                self.finish(True, 'All eight continuation drivers passed through native separate departures. Explicit checkpoint reload, packaged execution and rendered presentation require their own evidence.')
            else:
                self.next_stage()
        except Exception:
            self.finish(False, traceback.format_exc())


def start(output_directory=None):
    global _RUN
    assert _RUN is None or _RUN.done, 'Route chain is already active'
    e1 = importlib.import_module('continue_aurelion_e1_input')
    assert e1._RUN is not None and e1._RUN.done and e1._RUN.handle is None, 'E1 input callback must be retired'
    assert e1._RUN.report.get('status') == 'passed', 'A real E1 pass is required'
    output = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    assert not (output/'route-input-chain.json').exists(), 'Use a new output directory'
    _RUN = Run(output)
    try:
        _RUN.next_stage()
        _RUN.handle = unreal.register_slate_post_tick_callback(_RUN.tick)
    except Exception:
        _RUN.finish(False, traceback.format_exc())
    return _RUN


def stop():
    if _RUN is not None and not _RUN.done:
        _RUN.finish(False, 'Stopped by operator; current child released its inputs')


if __name__ == '__main__':
    start()
