from abc import ABC, abstractmethod
from ..models.task import Task
from ..models.work_result import WorkResult

class BaseWorker(ABC):
    """Abstract worker that executes a task and returns a WorkResult."""

    @abstractmethod
    def execute(self, task: Task) -> WorkResult:
        """Execute the given task and return the result."""
        pass