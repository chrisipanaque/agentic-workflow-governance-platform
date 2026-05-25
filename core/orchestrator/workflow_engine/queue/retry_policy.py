from dataclasses import dataclass
from enum import Enum
import math


class BackoffStrategy(Enum):
    """Strategy for calculating retry delays."""
    FIXED = "fixed"           # Constant delay between retries
    LINEAR = "linear"         # Linearly increasing delay (delay * attempt)
    EXPONENTIAL = "exponential"  # Exponentially increasing delay (base ** attempt)


@dataclass
class RetryPolicy:
    """Policy governing task retry behavior.
    
    This controls how many times a failed task is retried,
    how long to wait between retries, and other failure handling.
    """
    max_retries: int = 3
    backoff_strategy: BackoffStrategy = BackoffStrategy.EXPONENTIAL
    backoff_base: float = 2.0  # For exponential backoff
    backoff_initial_delay: float = 1.0  # Initial delay in seconds (for exponential/linear)
    max_backoff_delay: float = 3600.0  # Cap on delay (1 hour)

    def get_retry_delay(self, attempt: int) -> float:
        """Calculate the delay (in seconds) before the next retry.
        
        Args:
            attempt: The attempt number (0-indexed, so first retry is attempt=1)
        
        Returns:
            Delay in seconds, capped at max_backoff_delay
        """
        if attempt < 1:
            return 0.0

        if self.backoff_strategy == BackoffStrategy.FIXED:
            delay = self.backoff_initial_delay
        elif self.backoff_strategy == BackoffStrategy.LINEAR:
            delay = self.backoff_initial_delay * attempt
        elif self.backoff_strategy == BackoffStrategy.EXPONENTIAL:
            delay = self.backoff_initial_delay * (self.backoff_base ** (attempt - 1))
        else:
            delay = 0.0

        # Cap at max delay
        return min(delay, self.max_backoff_delay)

    def should_retry(self, retry_count: int) -> bool:
        """Check if a task with retry_count failures should be retried."""
        return retry_count < self.max_retries

    def __str__(self) -> str:
        strategy_name = self.backoff_strategy.value
        return (
            f"RetryPolicy(max_retries={self.max_retries}, "
            f"strategy={strategy_name}, base={self.backoff_base})"
        )
