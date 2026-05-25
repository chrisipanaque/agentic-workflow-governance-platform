#!/usr/bin/env python3
"""CLI for running tasks with the workflow engine."""
import argparse
import json
import sys
from typing import Dict, Any

from .models.task import Task
from .workers.local_worker import LocalWorker
from .engine import WorkflowEngine

# Example task handlers (for demo)
def build_handler(input_data: Dict[str, Any]) -> Dict[str, Any]:
    target = input_data.get("target", "default")
    print(f"[build] Building target '{target}'...")
    return {"status": "built", "artifacts": [f"{target}.out"]}

def test_handler(input_data: Dict[str, Any]) -> Dict[str, Any]:
    suite = input_data.get("suite", "all")
    print(f"[test] Running test suite '{suite}'...")
    return {"passed": True, "failed": 0}

def fail_handler(input_data: Dict[str, Any]) -> None:
    raise RuntimeError("This task always fails")

# Registry of available handlers
HANDLERS = {
    "build": build_handler,
    "test": test_handler,
    "fail": fail_handler,
}

def parse_args():
    parser = argparse.ArgumentParser(description="Run a workflow task")
    parser.add_argument("--task-type", required=True, help="Type of task (e.g. build, test)")
    parser.add_argument("--input", default="{}", help="JSON string of input parameters")
    parser.add_argument("--task-id", default=None, help="Optional task identifier")
    return parser.parse_args()

def main():
    args = parse_args()
    try:
        input_dict = json.loads(args.input)
    except json.JSONDecodeError as e:
        print(f"Error parsing --input JSON: {e}")
        sys.exit(1)

    task = Task(
        id=args.task_id or f"task-{args.task_type}",
        type=args.task_type,
        input=input_dict,
    )

    worker = LocalWorker(HANDLERS)
    engine = WorkflowEngine(worker)

    result = engine.run(task)

    print("\n=== Final Result ===")
    if result.success:
        print(f"Success: {result.output}")
        sys.exit(0)
    else:
        print(f"Failure: {result.error}")
        sys.exit(1)

if __name__ == "__main__":
    main()