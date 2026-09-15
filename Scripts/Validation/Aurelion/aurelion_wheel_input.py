"""Bounded ordinary-input weapon-wheel selector for the retained Aurelion PIE.

The caller owns IA_WeaponWheel. Call Selector.step(world) on every normal Slate
tick (including paused game ticks), then inject that returned ``held`` value into
the EXISTING wheel action. Always release that action when the caller stops.
No tick is installed here. No widget, item, resource or gameplay state is written.
The editor-only native mouse adapter must be compiled before this helper is used.
"""
import math
import time
import traceback
import unreal


WHEEL_CLASS = '/Game/UI/Narrative/Menus/RadialMenus/WM_WeaponWheel_LR.WM_WeaponWheel_LR_C'
WEAPON_CLASSES = frozenset('/Game/Items/Weapons/WI_{0}.WI_{0}_C'.format(name)
                          for name in ('Cinderline', 'Velkorran', 'Verity', 'Axiom', 'Staccato'))
# The authored radial widget ignores each-axis deltas within1. Ordinary
# PlayerInput scales raw samples;120 clears the observed deadzone while staying
# inside the native mouse adapter's +/-512 per-sample limit.
RAW_MOUSE_MAGNITUDE = 120.
DIRECTIONS = tuple((math.cos(i*math.pi/4.)*RAW_MOUSE_MAGNITUDE, math.sin(i*math.pi/4.)*RAW_MOUSE_MAGNITUDE) for i in range(8))
STEP_CAP_SECONDS = 2.
DIAGNOSTIC_INTERVAL_SECONDS = .25
# Main-hand selection needs one real click; allow a focus click plus bounded retries.
MAINHAND_CLICK_RETRY_SECONDS = .75
MAINHAND_CLICK_LIMIT = 6


def _path(obj):
    return obj.get_path_name() if obj is not None else None


def _xy(value):
    result = [float(value.x), float(value.y)]
    assert all(math.isfinite(v) for v in result), 'Non-finite wheel vector'
    return result


def _same_direction(actual, requested):
    actual_length = math.hypot(*actual)
    requested_length = math.hypot(*requested)
    return (actual_length > .01 and requested_length > .01
            and sum(a*b for a, b in zip(actual, requested)) / (actual_length*requested_length)
            >= math.cos(math.radians(20.)))


def _target_midpoint(bounds_min, bounds_max, index):
    # AngleToSector truncates to integers and tests its arrays in order. Its
    # wrap fallback returns SectorCount, so select inside the real 0..360
    # portion of the desired interval, never at a shared/wrap boundary.
    low = max(0., float(bounds_min[index]))
    high = min(360., float(bounds_max[index]))
    assert high-low > 4., 'Requested wheel sector has no safe interior angle'
    angle = (low+high)*.5
    first_match = next((i for i, (a, b) in enumerate(zip(bounds_min, bounds_max))
                        if a <= int(angle) <= b), None)
    assert first_match == index, 'Requested midpoint is shadowed by another wheel sector'
    # Authored Tick stores (-MouseX, MouseY), and GetCurrentAngle returns
    # 180-degAtan2(LastNonZeroDelta.Y, LastNonZeroDelta.X). Thus an ordinary
    # raw (cos(angle), sin(angle)) points at this angle without widget writes.
    radians = math.radians(angle)
    return angle, (math.cos(radians)*RAW_MOUSE_MAGNITUDE, math.sin(radians)*RAW_MOUSE_MAGNITUDE)


class Selector:
    """One immutable requested weapon, one live pawn, maximum 30 wall seconds.

    ``held, report = selector.step(world)`` requests the normal wheel action's
    next input value. ``done`` + ``report['status']`` distinguish pass/failure.
    Successful selection requires its visible sector to remain correct for 0.3s,
    normal release, and then the actual pawn's wielded list to contain the class.
    Already-wielded input is reported separately and claims no wheel interaction.
    """
    def __init__(self, weapon_class, clock=time.monotonic, *, manual_mainhand=False):
        assert weapon_class in WEAPON_CLASSES, 'Requires an exact existing project-owned weapon class'
        self.manual_mainhand = bool(manual_mainhand)
        self.maximum_seconds = 60. if self.manual_mainhand else 30.
        self.weapon_class = weapon_class
        self.clock = clock
        self.started = None
        self.correct_since = None
        self.release_at = None
        self.widget_path = None
        self.identity = None
        self.native_load_ready = False
        self.done = False
        self.wheel_class = None
        self.last_signature = None
        self.last_diagnostic_at = None
        self.plan = None
        self.plan_identity = None
        self.step_index = 0
        self.step_started = None
        self.step_first_input_frame = None
        self.step_processed_frames = set()
        self.step_last_input_frame = None
        self.report = dict(status='running', phase='await_wheel', requested_class=weapon_class,
                           method='Existing IA_WeaponWheel plus native normal MouseX/MouseY input',
                           direct_widget_item_resource_or_gameplay_writes=False,
                           sector_samples=[], raw_input_frames=0, normal_release_requested=False,
                           selection_verified=False, wield_verified=False,
                           processing_samples=[], input_steps=[], processed_direction_frames=0,
                           diagnostic_interval_seconds=DIAGNOSTIC_INTERVAL_SECONDS,
                           direction_step_cap_seconds=STEP_CAP_SECONDS,
                           raw_mouse_magnitude=RAW_MOUSE_MAGNITUDE,
                           manual_mainhand=self.manual_mainhand, maximum_seconds=self.maximum_seconds,
                           mainhand_clicks=[])
        self.last_click_frame = None

    def _finish(self, passed, reason):
        self.done = True
        if self.report['input_steps'] and 'end_reason' not in self.report['input_steps'][-1]:
            self.report['input_steps'][-1].update(
                ended_elapsed_seconds=self.report.get('elapsed_seconds'),
                end_reason='selection_finished' if passed else 'selector_stopped',
                processed_frames=len(self.step_processed_frames))
        self.report.update(status='passed' if passed else 'failed', reason=reason, held=False)
        return False, self.report

    def stop(self):
        """No persistent mouse input exists; caller must now inject wheel zero."""
        if not self.done:
            return self._finish(False, 'Operator stopped selection; caller must release IA_WeaponWheel')
        return False, self.report

    def _sample_wheel(self, world, pc):
        if self.wheel_class is None:
            self.wheel_class = unreal.load_class(None, WHEEL_CLASS)
            assert self.wheel_class is not None, 'The exact copied weapon-wheel class is missing'
        widgets = [w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world, self.wheel_class, False)
                   if w.get_class().get_path_name() == WHEEL_CLASS and w.get_owning_player() == pc and w.is_activated()]
        assert len(widgets) <= 1, 'Multiple active owned weapon wheels'
        if not widgets:
            return None
        widget = widgets[0]
        assert self.widget_path is None or self.widget_path == _path(widget), 'Weapon wheel was replaced during selection'
        self.widget_path = _path(widget)
        overlay = widget.get_editor_property('Overlay_RadialItems')
        assert overlay is not None, 'Copied wheel has no radial-items overlay'
        children = list(overlay.get_all_children())
        assert 0 < len(children) <= 16, 'Unexpected radial item count'
        # GetItem is a Blueprint-only pure function without generated Python glue.
        items = [child.call_method('GetItem') for child in children]
        classes = [_path(item.get_class()) if item is not None else None for item in items]
        assert classes.count(self.weapon_class) == 1, 'Requested weapon must occur exactly once in the actual wheel'
        sector = int(widget.get_editor_property('SelectedSector'))
        sector_count = int(widget.get_editor_property('SectorCount'))
        bounds_min = [int(v) for v in widget.get_editor_property('SectorListMinBounds')]
        bounds_max = [int(v) for v in widget.get_editor_property('SectorListMaxBounds')]
        assert sector_count == len(items) == len(bounds_min) == len(bounds_max), 'Wheel bounds/items are not initialized consistently'
        assert all(a < b for a, b in zip(bounds_min, bounds_max)), 'Invalid authored wheel sector bounds'
        current_angle = float(widget.get_editor_property('CurrentAngle'))
        display_angle = float(widget.get_editor_property('DisplayAngle'))
        assert math.isfinite(current_angle) and math.isfinite(display_angle), 'Non-finite wheel angle'
        raw_delta = [float(v) for v in pc.get_input_mouse_delta()]
        assert len(raw_delta) == 2 and all(math.isfinite(v) for v in raw_delta), 'Non-finite processed mouse input'
        # An intermediate wheel angle may have no sector. Continue normal input;
        # only a genuinely in-range requested item can qualify for release.
        in_range=0 <= sector < len(items)
        hand = {}
        if self.manual_mainhand:
            main = widget.get_editor_property('MainHandSelectedSlot')
            off = widget.get_editor_property('OffhandHandSelectedSlot')
            item = main.call_method('GetItem') if main is not None and unreal.SystemLibrary.is_valid(main) else None
            current = unreal.GameplayStatics.get_player_pawn(world, 0)
            hand = dict(mainhand_slot=_path(main), mainhand_item=_path(item),
                mainhand_class=_path(item.get_class()) if item is not None else None,
                mainhand_current_owner=item is not None and item.get_outer()==current,
                offhand_slot=_path(off), manual_mainhand_ready=item is not None and item.get_outer()==current
                    and _path(item.get_class())==self.weapon_class and off is None)
        return dict(**hand, widget=_path(widget), sector=sector, sector_in_range=in_range,
                    selected_item=_path(items[sector]) if in_range else None,
                    selected_class=classes[sector] if in_range else None, item_classes=classes,
                    wheel_held=bool(pc.get_editor_property('WeaponWheelHeld')),
                    frame=int(unreal.SystemLibrary.get_frame_count()),
                    gamepad=bool(pc.is_using_gamepad()), raw_mouse_delta=raw_delta,
                    cursor_delta=_xy(widget.get_editor_property('CursorDelta')),
                    last_nonzero_delta=_xy(widget.get_editor_property('LastNonZeroDelta')),
                    current_angle=current_angle, display_angle=display_angle,
                    bounds_min=bounds_min, bounds_max=bounds_max)

    def _prepare_plan(self, sample):
        identity = (tuple(sample['item_classes']), tuple(sample['bounds_min']), tuple(sample['bounds_max']))
        if self.plan is not None:
            assert identity == self.plan_identity, 'Wheel contents/bounds changed during selection'
            return
        self.plan_identity = identity
        target_index = sample['item_classes'].index(self.weapon_class)
        angle, delta = _target_midpoint(sample['bounds_min'], sample['bounds_max'], target_index)
        self.plan = [dict(kind='target_midpoint', angle=angle, delta=delta)]
        self.plan.extend(dict(kind='fallback_direction', angle=i*45., delta=d)
                         for i, d in enumerate(DIRECTIONS))
        self.report['target_sector'] = target_index
        self.report['target_midpoint_degrees'] = angle
        self.report['input_plan'] = self.plan

    def _processing_sample(self, sample, now):
        # Read before injecting the next pair: InputKey only accumulates values;
        # PlayerInput and the UMG Tick must subsequently process them. Separate
        # raw and widget observations expose a stalled stage without guessing
        # viewport focus or confusing analog consumption with processing.
        if self.plan is not None and self.step_started is not None:
            delta = self.plan[self.step_index % len(self.plan)]['delta']
            later_frame = (self.step_first_input_frame is not None
                           and sample['frame'] > self.step_first_input_frame)
            sample['raw_direction_matches'] = later_frame and _same_direction(sample['raw_mouse_delta'], delta)
            sample['widget_direction_matches'] = later_frame and _same_direction(
                sample['last_nonzero_delta'], (-delta[0], delta[1]))
            sample['direction_processed'] = (not sample['gamepad'] and sample['raw_direction_matches']
                                             and sample['widget_direction_matches'])
            if sample['direction_processed'] and sample['frame'] not in self.step_processed_frames:
                self.step_processed_frames.add(sample['frame'])
                self.report['processed_direction_frames'] += 1
        if self.last_diagnostic_at is None or now-self.last_diagnostic_at >= DIAGNOSTIC_INTERVAL_SECONDS:
            self.report['processing_samples'].append(dict(elapsed=round(now-self.started, 3),
                                                           input_step=self.step_index, **sample))
            self.last_diagnostic_at = now

    def _input_step(self, sample, now):
        if self.step_started is not None:
            duration = now-self.step_started
            observed = len(self.step_processed_frames) >= 2 and duration >= .35
            if observed or duration >= STEP_CAP_SECONDS:
                self.report['input_steps'][-1].update(
                    ended_elapsed_seconds=round(now-self.started, 3),
                    end_reason='processed_direction_without_target_selection' if observed else 'two_second_cap',
                    processed_frames=len(self.step_processed_frames))
                self.step_index += 1
                self.step_started = None
        if self.step_started is None:
            self.step_started = now
            self.step_first_input_frame = None
            self.step_last_input_frame = None
            self.step_processed_frames.clear()
            self.report['input_steps'].append(dict(index=self.step_index,
                started_elapsed_seconds=round(now-self.started, 3),
                **self.plan[self.step_index % len(self.plan)]))
        return self.plan[self.step_index % len(self.plan)]

    def step(self, world):
        if self.done:
            return False, self.report
        try:
            now = self.clock()
            if self.started is None:
                self.started = now
            elapsed = now-self.started
            self.report['elapsed_seconds'] = round(elapsed, 3)
            assert elapsed < self.maximum_seconds, 'Normal weapon-wheel selection exceeded its recorded bounded window'
            assert world is not None and '/Aurelion/Maps/UEDPIE_' in world.get_path_name(), 'Requires a real retained Aurelion PIE world'
            assert any(name in world.get_name() for name in ('L_Aurelion_M12', 'L_Aurelion_M13'))
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            assert isinstance(pc, unreal.SovPlayerController) and isinstance(pawn, unreal.SovPlayerCharacterBase)
            assert pawn.is_character_ready() and pawn.is_alive() and pawn.get_health() > 0., 'Ready living player required'
            identity = (_path(world), _path(pc), _path(pawn))
            if self.identity is None:
                self.identity = identity
                self.report['identity'] = identity
            assert identity == self.identity, 'World/controller/pawn changed during selection'
            # The actual GA_Weapon_Wield guard includes asynchronous appearance
            # and equipment loads beyond project-level character readiness.
            pending_load = bool(pawn.is_character_pending_load())
            self.report['actual_character_pending_load'] = pending_load
            if not self.native_load_ready:
                if pending_load:
                    self.report.update(phase='await_narrative_load', held=False)
                    return False, self.report
                self.native_load_ready = True
                self.report.update(phase='await_wheel', native_load_ready_elapsed_seconds=round(elapsed, 3))
            wielded = [_path(w.get_class()) for w in pawn.get_wielded_weapons() if w is not None]
            self.report['actual_wielded_classes'] = wielded
            if self.report['phase'] == 'await_wield':
                released = not bool(pc.get_editor_property('WeaponWheelHeld'))
                self.report['actual_wheel_released'] = released
                if released and self.weapon_class in wielded:
                    self.report['wield_verified'] = True
                    return self._finish(True, 'Requested sector held for 0.3s, normal wheel release and actual weapon wield observed')
                self.report['held'] = False
                return False, self.report
            if elapsed == 0. and self.weapon_class in wielded:
                self.report.update(wield_verified=True, already_wielded=True)
                return self._finish(True, 'Requested weapon was already wielded; no wheel selection claimed')
            sample = self._sample_wheel(world, pc)
            self.report['held'] = True
            if sample is None:
                self.correct_since = None
                return True, self.report
            signature = (sample['widget'], sample['sector'], sample['selected_class'], sample['wheel_held'])
            if signature != self.last_signature:
                self.report['sector_samples'].append(dict(elapsed=round(elapsed, 3), **sample))
                self.last_signature = signature
            self.report['last_wheel'] = sample
            self._prepare_plan(sample)
            self._processing_sample(sample, now)
            if not sample['wheel_held']:
                self.correct_since = None
                return True, self.report
            if sample['selected_class'] == self.weapon_class:
                if self.manual_mainhand and not sample['manual_mainhand_ready']:
                    # CommonUI's real EquipWeaponMainhand click supplies this slot.
                    # Keep normal T held; send an ordinary Slate click and re-observe.
                    # The first click can only focus the viewport, so retry on a later frame.
                    self.correct_since = None
                    self.report.update(phase='await_manual_mainhand',
                        operator_instruction='Automated ordinary Slate left click; no widget handler or equipment setter is called',
                        manual_mainhand_observation={key:sample[key] for key in
                            ('mainhand_slot','mainhand_item','mainhand_class','mainhand_current_owner','offhand_slot','manual_mainhand_ready')})
                    clicks = self.report['mainhand_clicks']
                    if (self.last_click_frame != sample['frame'] and len(clicks) < MAINHAND_CLICK_LIMIT
                            and (not clicks or elapsed-clicks[-1]['elapsed'] >= MAINHAND_CLICK_RETRY_SECONDS)):
                        result = unreal.SovAurelionPIEInputLibrary.inject_aurelion_pie_left_click(world)
                        self.last_click_frame = sample['frame']
                        clicks.append(dict(elapsed=round(elapsed, 3), frame=sample['frame'], routed=bool(result.routed),
                            press_handled=bool(result.press_handled), release_handled=bool(result.release_handled),
                            screen_position=[float(result.screen_position.x), float(result.screen_position.y)],
                            report=result.report))
                        assert result.routed, 'Ordinary main-hand click could not be routed: '+result.report
                    return True, self.report
                if self.manual_mainhand:
                    self.report['manual_mainhand_verified'] = True
                    self.report['manual_mainhand_observation'] = {key:sample[key] for key in
                        ('mainhand_slot','mainhand_item','mainhand_class','mainhand_current_owner','offhand_slot','manual_mainhand_ready')}
                if self.correct_since is None:
                    self.correct_since = now
                if now-self.correct_since >= .3:
                    self.release_at = now
                    self.report.update(phase='await_wield', held=False, selection_verified=True,
                                       normal_release_requested=True, selected_stable_seconds=now-self.correct_since,
                                       release_elapsed_seconds=elapsed)
                    return False, self.report
                # No new raw input while confirming a stable selection. The stock
                # wheel retains its last nonzero direction across ordinary frames.
                return True, self.report
            self.correct_since = None
            step = self._input_step(sample, now)
            # Slate may tick more than once per engine frame. Do not pile up
            # additional raw samples before one normal input frame can run.
            if self.step_last_input_frame == sample['frame']:
                return True, self.report
            dx, dy = step['delta']
            result = unreal.SovAurelionPIEInputLibrary.inject_aurelion_pie_mouse_delta(world, dx, dy)
            row = dict(routed=bool(result.routed), mouse_x_consumed=bool(result.mouse_x_consumed),
                       mouse_y_consumed=bool(result.mouse_y_consumed), controller_path=result.controller_path,
                       report=result.report, direction_index=self.step_index % len(self.plan),
                       step_index=self.step_index, kind=step['kind'], angle=step['angle'],
                       frame=sample['frame'], delta=[dx, dy])
            self.report['last_raw_input'] = row
            assert row['routed'] and row['controller_path'] == _path(pc), 'Ordinary raw mouse input did not reach the exact player owner'
            # Consumption flags can be false for successfully accumulated analog
            # input. Wheel state is observed on the NEXT normal input frame.
            self.report['raw_input_frames'] += 1
            self.step_last_input_frame = sample['frame']
            if self.step_first_input_frame is None:
                self.step_first_input_frame = sample['frame']
            self.report['phase'] = 'select_sector'
            return True, self.report
        except Exception:
            self.report['error'] = traceback.format_exc()
            return self._finish(False, self.report['error'])
