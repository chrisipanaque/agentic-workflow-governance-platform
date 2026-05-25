from enum import Enum, auto

class State(Enum):
    PENDING = auto()
    PLANNING = auto()
    WORKER_ASSIGNED = auto()   # optional but helpful for debugging
    EXECUTING = auto()
    EVALUATING = auto()
    COMPLETED = auto()
    FAILED = auto()