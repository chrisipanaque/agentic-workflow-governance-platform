from dataclasses import dataclass
from typing import Optional, List

from ...models.task import Task
from ...models.work_result import WorkResult
from ..base import BaseWorker
from .executor_agent import ExecutionLog


@dataclass
class EvaluationScore:
    """Result of evaluation: quality assessment and feedback."""
    score: float  # 0.0 to 1.0
    passed: bool  # Whether output meets minimum quality threshold
    feedback: str
    issues: List[str] = None  # Specific problems found


class EvaluatorAgent(BaseWorker):
    """Agent that validates and scores the output of an execution plan.
    
    The evaluator uses rule-based heuristics to assess whether the
    execution results meet quality criteria. This is a controlled function
    that makes deterministic decisions based on configurable rules.
    """

    def __init__(self, min_score: float = 0.7):
        """
        Args:
            min_score: Minimum quality score (0.0-1.0) required to pass evaluation.
        """
        self.min_score = min_score

    def execute(self, task: Task) -> WorkResult:
        """Evaluate a task result.
        
        Expects task.input to be an ExecutionLog (from ExecutorAgent).
        """
        try:
            if isinstance(task.input, dict) and "executed_steps" in task.input:
                # Handle dict-serialized ExecutionLog
                log = self._deserialize_log(task.input)
            elif isinstance(task.input, ExecutionLog):
                log = task.input
            else:
                return WorkResult(
                    success=False,
                    error="EvaluatorAgent expects task.input to be an ExecutionLog or dict"
                )

            score = self._evaluate(log)
            return WorkResult(success=score.passed, output=score)
        except Exception as e:
            return WorkResult(success=False, error=str(e))

    def _evaluate(self, log: ExecutionLog) -> EvaluationScore:
        """Apply evaluation rules to the execution log."""
        issues = []
        base_score = 1.0

        # Rule 1: All steps must succeed
        if log.failed_step_id:
            base_score *= 0.5
            issues.append(f"Execution failed at step: {log.failed_step_id}")

        # Rule 2: No step output should have errors
        for step_id, result in log.executed_steps.items():
            if not result.success:
                base_score *= 0.8
                issues.append(f"Step {step_id} returned error: {result.error}")

        # Rule 3: Output should be non-empty (if applicable)
        has_output = any(
            result.output is not None and result.output != {}
            for result in log.executed_steps.values()
        )
        if not has_output:
            base_score *= 0.9
            issues.append("No meaningful output produced")

        # Clamp score to [0, 1]
        final_score = max(0.0, min(1.0, base_score))
        passed = final_score >= self.min_score

        feedback = self._generate_feedback(final_score, passed, issues)

        return EvaluationScore(
            score=final_score,
            passed=passed,
            feedback=feedback,
            issues=issues if issues else None,
        )

    def _generate_feedback(self, score: float, passed: bool, issues: List[str]) -> str:
        """Generate human-readable feedback based on evaluation."""
        if passed:
            return f"Execution passed validation (score: {score:.2f})"
        else:
            issue_list = "; ".join(issues) if issues else "Unknown issues"
            return f"Execution failed validation (score: {score:.2f}). Issues: {issue_list}"

    def _deserialize_log(self, log_dict) -> ExecutionLog:
        """Convert dict representation back to ExecutionLog."""
        executed_steps = {}
        for step_id, result_dict in log_dict.get("executed_steps", {}).items():
            result = WorkResult(
                success=result_dict["success"],
                output=result_dict.get("output"),
                error=result_dict.get("error"),
            )
            executed_steps[step_id] = result
        return ExecutionLog(
            plan_id=log_dict.get("plan_id", "unknown"),
            executed_steps=executed_steps,
            failed_step_id=log_dict.get("failed_step_id"),
        )
