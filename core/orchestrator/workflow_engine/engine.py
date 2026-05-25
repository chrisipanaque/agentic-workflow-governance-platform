from typing import Optional
from .models.task import Task
from .models.work_result import WorkResult
from .models.state import State
from .state_machine import StateMachine
from .workers.base import BaseWorker

class WorkflowEngine:
    """Orchestrates a task through the state machine using a worker."""

    def __init__(self, worker: BaseWorker):
        self.worker = worker

    def run(self, task: Task) -> WorkResult:
        sm = StateMachine(State.PENDING)

        # 1. PLANNING (simple no‑op for now)
        sm.transition_to(State.PLANNING)
        # Optional planning logic could go here (e.g. resolve dependencies)
        # For minimal version, we just proceed.

        # 2. Assign worker
        sm.transition_to(State.WORKER_ASSIGNED)

        # 3. Execute via worker
        sm.transition_to(State.EXECUTING)
        result = self.worker.execute(task)

        # 4. Evaluate result
        sm.transition_to(State.EVALUATING)
        if result.success:
            sm.transition_to(State.COMPLETED)
        else:
            sm.transition_to(State.FAILED)

        return result