from dataclasses import dataclass, field
from typing import Optional, List
from enum import Enum
from datetime import datetime, timedelta
import heapq

from ..models.task import Task
from .retry_policy import RetryPolicy


class TaskPriority(Enum):
    """Priority levels for task execution."""
    LOW = 3
    NORMAL = 2
    HIGH = 1
    CRITICAL = 0


@dataclass
class QueuedTask:
    """A task that is queued for execution, with metadata."""
    task: Task
    priority: TaskPriority = TaskPriority.NORMAL
    retry_policy: RetryPolicy = field(default_factory=RetryPolicy)
    retry_count: int = 0
    enqueued_at: datetime = field(default_factory=datetime.now)
    scheduled_at: Optional[datetime] = None
    completed_at: Optional[datetime] = None

    def __lt__(self, other: "QueuedTask") -> bool:
        """For priority queue ordering: lower priority value = higher priority."""
        if self.priority.value != other.priority.value:
            return self.priority.value < other.priority.value
        # Tie-breaker: FIFO by enqueue time
        return self.enqueued_at < other.enqueued_at

    def is_ready(self) -> bool:
        """Check if this task is ready to execute (scheduled time has passed)."""
        if self.scheduled_at is None:
            return True
        return datetime.now() >= self.scheduled_at

    def mark_completed(self) -> None:
        """Mark the task as completed."""
        self.completed_at = datetime.now()

    def should_retry(self) -> bool:
        """Check if this task should be retried after failure."""
        return self.retry_policy.should_retry(self.retry_count)

    def reschedule_on_failure(self) -> None:
        """Reschedule the task after a failure, with backoff delay."""
        self.retry_count += 1
        if self.should_retry():
            delay_seconds = self.retry_policy.get_retry_delay(self.retry_count)
            self.scheduled_at = datetime.now() + timedelta(seconds=delay_seconds)
            print(
                f"Task {self.task.id} scheduled for retry #{self.retry_count} "
                f"in {delay_seconds:.1f}s"
            )


class TaskQueue:
    """Priority queue for managing task execution.
    
    Tasks are stored in a priority queue and can be:
    - Enqueued with a priority level
    - Dequeued when ready to execute
    - Re-enqueued on failure with exponential backoff
    
    This is a simple in-memory queue; production would add persistence.
    """

    def __init__(self):
        self._heap: List[QueuedTask] = []
        self._task_map = {}  # task_id → QueuedTask for quick lookup

    def enqueue(
        self,
        task: Task,
        priority: TaskPriority = TaskPriority.NORMAL,
        retry_policy: Optional[RetryPolicy] = None,
    ) -> None:
        """Add a task to the queue.
        
        Args:
            task: The task to enqueue
            priority: Priority level (affects execution order)
            retry_policy: Retry configuration (uses default if None)
        """
        if task.id in self._task_map:
            raise ValueError(f"Task {task.id} already in queue")

        queued = QueuedTask(
            task=task,
            priority=priority,
            retry_policy=retry_policy or RetryPolicy(),
        )
        heapq.heappush(self._heap, queued)
        self._task_map[task.id] = queued
        print(f"Enqueued task {task.id} with priority {priority.name}")

    def dequeue(self) -> Optional[QueuedTask]:
        """Dequeue the next ready task (highest priority, FIFO tie-break).
        
        Returns None if no tasks are ready.
        """
        while self._heap:
            queued = heapq.heappop(self._heap)
            # Remove from map
            self._task_map.pop(queued.task.id, None)

            if queued.is_ready():
                return queued
            else:
                # Re-insert if not ready yet
                heapq.heappush(self._heap, queued)
                return None

        return None

    def requeue_on_failure(self, queued_task: QueuedTask) -> bool:
        """Re-enqueue a task after failure with backoff.
        
        Returns True if re-queued, False if max retries exceeded.
        """
        if not queued_task.should_retry():
            print(f"Task {queued_task.task.id} exhausted retries, not re-queueing")
            return False

        queued_task.reschedule_on_failure()
        heapq.heappush(self._heap, queued_task)
        self._task_map[queued_task.task.id] = queued_task
        return True

    def mark_completed(self, task_id: str) -> None:
        """Mark a task as completed."""
        if task_id in self._task_map:
            self._task_map[task_id].mark_completed()
            del self._task_map[task_id]

    def size(self) -> int:
        """Return the number of tasks currently in the queue."""
        return len(self._heap)

    def pending_tasks(self) -> List[QueuedTask]:
        """Return a snapshot of all pending tasks (not in any particular order)."""
        return list(self._heap)

    def __str__(self) -> str:
        tasks_info = [f"{q.task.id}({q.priority.name})" for q in self._heap]
        return f"TaskQueue({self.size()} tasks: {', '.join(tasks_info)})"
