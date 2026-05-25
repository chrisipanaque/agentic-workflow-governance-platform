from typing import Dict, Callable, Any
from .base import BaseWorker
from ..models.task import Task
from ..models.work_result import WorkResult

# Type for a handler function that takes task input dict and returns output or raises exception
TaskHandler = Callable[[Dict[str, Any]], Any]

class LocalWorker(BaseWorker):
    """Worker that executes tasks in-process using a registry of handler functions."""

    def __init__(self, handlers: Dict[str, TaskHandler]):
        """
        handlers: dict mapping task type (e.g. "build") to a callable that
                  accepts the task input dict and returns the result.
        """
        self.handlers = handlers

    def execute(self, task: Task) -> WorkResult:
        handler = self.handlers.get(task.type)
        if handler is None:
            return WorkResult(
                success=False,
                error=f"No handler registered for task type '{task.type}'"
            )
        try:
            output = handler(task.input)
            return WorkResult(success=True, output=output)
        except Exception as e:
            return WorkResult(success=False, error=str(e))