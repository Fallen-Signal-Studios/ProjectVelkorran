"""Bounded arrival at a complete native path's projected destination."""
import math

def reached_projected_destination(current, requested, path_points, complete):
    if not complete or not path_points:
        return False
    endpoint=path_points[-1]
    values=(*current,*requested,*endpoint)
    if len(current)!=3 or len(requested)!=3 or len(endpoint)!=3 or not all(math.isfinite(v) for v in values):
        return False
    return (math.dist(current[:2],endpoint[:2])<25.
            and math.dist(requested[:2],endpoint[:2])<=100.
            and abs(current[2]-requested[2])<140.)
