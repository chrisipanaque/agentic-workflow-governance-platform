from dataclasses import dataclass
from enum import Enum
from typing import Optional, Callable, Dict, Any
from datetime import datetime, timedelta

from ..models.task import Task
from ..models.work_result import WorkResult
from .task_queue import TaskQueue, TaskPriority, QueuedTask
from .retry_policy import RetryPolicy


class SchedulePolicy(Enum):
    """Policies for when/how tasks are scheduled."""
    IMMEDIATE = "immediate"          # Execute as soon as possible
    DELAYED = "delayed"              # Execute after a specified delay
    RATE_LIMITED = "rate_limited"    # Spread executions over time
    SCHEDULED = "scheduled"           # Execute at specific time


@dataclass
class ScheduledTask:
    """A task scheduled for execution with timing metadata."""
    queued_task: QueuedTask
    policy: SchedulePolicy
    delay_seconds: float = 0.0  # For DELAYED and RATE_LIMITED policies


class PriorityScheduler:
    """Scheduler that manages task execution with priority and retry logic.
    
    The scheduler:
    - Maintains a priority queue of pending tasks
    - Schedules tasks based on policy (immediate, delayed, rate-limited)
    - Handles retries with exponential backoff
    - Tracks execution history
    """

    def __init__(self, max_concurrent_tasks: int = 1):
        """
        Args:
            max_concurrent_tasks: Maximum number of tasks to execute in parallel.
                                 (Currently limited to 1 for simple sequential execution)
        """
        self.queue = TaskQueue()
        self.max_concurrent_tasks = max_concurrent_tasks
        self.active_tasks: Dict[str, QueuedTask] = {}
        self.completed_tasks: Dict[str, QueuedTask] = {}
        self.failed_tasks: Dict[str, QueuedTask] = {}

    def schedule(
        self,
        task: Task,
        policy: SchedulePolicy = SchedulePolicy.IMMEDIATE,
        priority: TaskPriority = TaskPriority.NORMAL,
        delay_seconds: float = 0.0,
        retry_policy: Optional[RetryPolicy] = None,
    ) -> None:
        """Schedule a task for execution.
        
        Args:
            task: Task to schedule
            policy: When to execute (immediate, delayed, etc.)
            priority: Execution priority
            delay_seconds: Delay before execution (for DELAYED and RATE_LIMITED)
            retry_policy: Retry configuration
        """
        queued = QueuedTask(
            task=task,
            priority=priority,
            retry_policy=retry_policy or RetryPolicy(),
        )

        # Apply scheduling policy
        if policy == SchedulePolicy.IMMEDIATE:
            queued.scheduled_at = None
        elif policy == SchedulePolicy.DELAYED:
            queued.scheduled_at = datetime.now() + timedelta(seconds=delay_seconds)
        elif policy == SchedulePolicy.RATE_LIMITED:
            # Spread tasks over time based on queue size
            queue_size = self.queue.size()
            queued.scheduled_at = datetime.now() + timedelta(
                seconds=delay_seconds * (queue_size + 1)
            )
        elif policy == SchedulePolicy.SCHEDULED:
            # For scheduled, delay_seconds is absolute timestamp (handled by caller)
            queued.scheduled_at = datetime.now() + timedelta(seconds=delay_seconds)

        self.queue.enqueue(task, priority, queued.retry_policy)

    def next_task(self) -> Optional[QueuedTask]:
        """Get the next ready task from the queue."""
        if len(self.active_tasks) >= self.max_concurrent_tasks:
            return None
        return self.queue.dequeue()

    def mark_success(self, task_id: str) -> None:
        """Mark a task as successfully completed."""
        if task_id in self.active_tasks:
            queued = self.active_tasks.pop(task_id)
            queued.mark_completed()
            self.completed_tasks[task_id] = queued
            print(f"Task {task_id} completed successfully")

    def mark_failure(self, task_id: str) -> bool:
        """Mark a task as failed.
        
        Returns True if the task will be retried, False if exhausted.
        """
        if task_id not in self.active_tasks:
            return False

        queued = self.active_tasks.pop(task_id)

        # Try to re-queue with backoff
        if self.queue.requeue_on_failure(queued):
            return True
        else:
            # Max retries exceeded
            self.failed_tasks[task_id] = queued
            print(f"Task {task_id} failed permanently (max retries exceeded)")
            return False

    def get_status(self) -> Dict[str, Any]:
        """Get scheduler status snapshot."""
        return {
            "pending": self.queue.size(),
            "active": len(self.active_tasks),
            "completed": len(self.completed_tasks),
            "failed": len(self.failed_tasks),
            "total": len(self.completed_tasks) + len(self.failed_tasks),
        }

    def __str__(self) -> str:
        status = self.get_status()
        return (
            f"PriorityScheduler(pending={status['pending']}, "
            f"active={status['active']}, completed={status['completed']}, "
            f"failed={status['failed']})"
        )
