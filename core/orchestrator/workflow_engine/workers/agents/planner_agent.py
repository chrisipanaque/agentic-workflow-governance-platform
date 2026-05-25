from dataclasses import dataclass, field
from typing import List, Dict, Any
from ...models.task import Task
from ...models.work_result import WorkResult
from ..base import BaseWorker


@dataclass
class Step:
    """Represents a single step in a task execution plan."""
    id: str
    description: str
    action: str  # e.g. "build", "test", "deploy"
    input: Dict[str, Any] = field(default_factory=dict)
    dependencies: List[str] = field(default_factory=list)  # ids of steps this depends on


@dataclass
class Plan:
    """Represents a complete execution plan for a task."""
    task_id: str
    steps: List[Step] = field(default_factory=list)

    def is_valid(self) -> bool:
        """Check if the plan has no circular dependencies."""
        # Simple check: each step only depends on earlier steps
        step_ids = [s.id for s in self.steps]
        for step in self.steps:
            for dep in step.dependencies:
                if dep not in step_ids:
                    return False
        return True


class PlannerAgent(BaseWorker):
    """Agent that breaks a task into a sequence of executable steps.
    
    The planner analyzes the task type and input to create a step-by-step
    execution plan. This is a controlled function (not autonomous) that
    uses simple heuristics based on task type.
    """

    def execute(self, task: Task) -> WorkResult:
        """Break the task into a plan of steps."""
        try:
            plan = self._plan(task)
            if not plan.is_valid():
                return WorkResult(
                    success=False,
                    error="Generated plan has circular dependencies or invalid refs"
                )
            return WorkResult(success=True, output=plan)
        except Exception as e:
            return WorkResult(success=False, error=str(e))

    def _plan(self, task: Task) -> Plan:
        """Internal method to generate a plan based on task type."""
        plan = Plan(task_id=task.id)

        # Simple heuristics for common task types
        if task.type == "build":
            plan.steps = self._plan_build(task)
        elif task.type == "test":
            plan.steps = self._plan_test(task)
        elif task.type == "deploy":
            plan.steps = self._plan_deploy(task)
        else:
            # Default: single-step plan
            plan.steps = [
                Step(
                    id="step-1",
                    description=f"Execute {task.type}",
                    action=task.type,
                    input=task.input,
                )
            ]

        return plan

    def _plan_build(self, task: Task) -> List[Step]:
        """Plan a build task: compile → link → package."""
        target = task.input.get("target", "default")
        return [
            Step(
                id="build-compile",
                description="Compile source files",
                action="build",
                input={"phase": "compile", "target": target},
            ),
            Step(
                id="build-link",
                description="Link object files",
                action="build",
                input={"phase": "link", "target": target},
                dependencies=["build-compile"],
            ),
            Step(
                id="build-package",
                description="Package artifacts",
                action="build",
                input={"phase": "package", "target": target},
                dependencies=["build-link"],
            ),
        ]

    def _plan_test(self, task: Task) -> List[Step]:
        """Plan a test task: unit → integration."""
        suite = task.input.get("suite", "all")
        return [
            Step(
                id="test-unit",
                description="Run unit tests",
                action="test",
                input={"suite": "unit"},
            ),
            Step(
                id="test-integration",
                description="Run integration tests",
                action="test",
                input={"suite": "integration"},
                dependencies=["test-unit"],
            ),
        ]

    def _plan_deploy(self, task: Task) -> List[Step]:
        """Plan a deploy task: validate → stage → release."""
        environment = task.input.get("environment", "staging")
        return [
            Step(
                id="deploy-validate",
                description="Validate deployment package",
                action="deploy",
                input={"phase": "validate"},
            ),
            Step(
                id="deploy-stage",
                description="Stage to environment",
                action="deploy",
                input={"phase": "stage", "environment": environment},
                dependencies=["deploy-validate"],
            ),
            Step(
                id="deploy-release",
                description="Release to production",
                action="deploy",
                input={"phase": "release", "environment": environment},
                dependencies=["deploy-stage"],
            ),
        ]
