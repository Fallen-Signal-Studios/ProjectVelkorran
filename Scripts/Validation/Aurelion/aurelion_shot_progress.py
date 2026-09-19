"""Bound consecutive failed flanks without counting earlier successful combat."""

def record_shot_progress(control, consumed, has_damage_receipt):
    """Return whether another flank is needed; real damage resets the failure run."""
    if has_damage_receipt:
        control['misses'] = control['flanks'] = 0
        return False
    control['misses'] += consumed
    if control['misses'] < 2:
        return False
    control['flanks'] += 1
    control['misses'] = 0
    assert control['flanks'] <= 3, 'Three consecutive native-path flanks failed to produce damage; stop without wasting remaining ammunition'
    return True
