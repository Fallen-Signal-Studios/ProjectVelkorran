"""Reviewed scene-specific camera positions, relative to the unchanged Hero mark."""

def grammar_propagation_shots(station):
    def add(offset): return tuple(a+b for a,b in zip(station,offset))
    # Both cameras stay east of the cylindrical terminal. The old southwest
    # close camera looked through its cap toward the two protagonists.
    target=add((75.,90.,45.))
    return dict(wide=(add((850.,-100.,200.)),target,38.),
                close=(add((650.,120.,110.)),target,50.))
