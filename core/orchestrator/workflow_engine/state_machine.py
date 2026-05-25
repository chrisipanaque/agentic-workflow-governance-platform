from typing import Set, Dict
from .models.state import State

class StateMachine:
    """Finite state machine that validates transitions and prints each change."""

    _transitions: Dict[State, Set[State]] = {
        State.PENDING: {State.PLANNING},
        State.PLANNING: {State.WORKER_ASSIGNED},
        State.WORKER_ASSIGNED: {State.EXECUTING},
        State.EXECUTING: {State.EVALUATING},
        State.EVALUATING: {State.COMPLETED, State.FAILED},
        State.COMPLETED: set(),
        State.FAILED: set(),
    }

    def __init__(self, initial_state: State = State.PENDING):
        self._state = initial_state

    @property
    def state(self) -> State:
        return self._state

    def transition_to(self, new_state: State) -> None:
        """Move to new_state if allowed, print transition, otherwise raise ValueError."""
        if new_state not in self._transitions.get(self._state, set()):
            allowed = [s.name for s in self._transitions.get(self._state, set())]
            raise ValueError(
                f"Illegal transition: {self._state.name} -> {new_state.name}. "
                f"Allowed: {allowed if allowed else 'none'}"
            )
        old_name = self._state.name
        new_name = new_state.name
        self._state = new_state
        print(f"{old_name} → {new_name}")