from dataclasses import dataclass, field
from typing import Any, Dict

@dataclass
class Task:
    """Pure data object representing a unit of work."""
    id: str
    type: str           # e.g. "build", "test", "deploy"
    input: Dict[str, Any] = field(default_factory=dict)
    metadata: Dict[str, Any] = field(default_factory=dict)  # retries, timeout, etc.