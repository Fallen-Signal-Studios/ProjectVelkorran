"""Reviewed scene-specific camera positions, relative to the unchanged Hero mark."""

def quarantine_shots(station):
    def add(offset): return tuple(a+b for a,b in zip(station,offset))
    # Stay inside the ramp and north of the quarantine gates. Side shots from
    # outside the narrow walkway look through its architecture.
    target=add((75.,0.,35.))
    return dict(wide=(add((0.,520.,180.)),target,26.),
                close=(add((-150.,350.,100.)),target,32.))

def grammar_propagation_shots(station):
    def add(offset): return tuple(a+b for a,b in zip(station,offset))
    # Both cameras stay east of the cylindrical terminal. The old southwest
    # close camera looked through its cap toward the two protagonists.
    target=add((75.,90.,45.))
    return dict(wide=(add((850.,-100.,200.)),target,38.),
                close=(add((650.,120.,110.)),target,50.))

def fifth_witness_shots(station):
    def add(offset): return tuple(a+b for a,b in zip(station,offset))
    # Tarrik's mark is west/north of Selene, unlike the generic partner mark.
    # Look across both marks from the east so the close cut retains the speaker.
    target=add((-75.,90.,35.))
    return dict(wide=(add((600.,-350.,180.)),target,26.),
                close=(add((550.,260.,100.)),target,32.))
