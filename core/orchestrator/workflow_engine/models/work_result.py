from dataclasses import dataclass
from typing import Any, Optional

@dataclass
class WorkResult:
    """Result returned by a worker after executing a task."""
    success: bool
    output: Any = None
    error: Optional[str] = None