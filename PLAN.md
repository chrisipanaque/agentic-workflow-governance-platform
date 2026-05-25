Create a minimal C++ workflow engine.

Requirements:
- Create a Task class
- Task has:
  - integer id
  - string description
  - enum state:
    PENDING
    RUNNING
    COMPLETED
    FAILED

- Create a Worker class
- Worker has executeTask(Task&)
- executeTask:
  - changes state from PENDING -> RUNNING
  - simulates work
  - changes state to COMPLETED

- In main.cpp:
  - create one task
  - create one worker
  - execute task
  - print state transitions to console

Keep code simple and beginner-friendly.
Do not use threading yet.
Do not use external libraries.