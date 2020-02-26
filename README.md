## Worker revival

As the parent goes through the list of task, it loops through all the processes to check if the processes are still alive.

If process is both still alive and done its previously assigned task (`task_status == 0 && child_status == 0`), give it a new task.

If the process is dead (`child_status != 0`), parent call `fork()` to create a new child

## Worker termination

Loop through all processes to check

If a process is already dead, nothing needs to be done.

If a process is still alive and not doing a task, assign it a `z0` task so that it exits itself

If a process is alive and doing task, do not kill it yet; 

Keep looping until all tasks have been assigned a `z0` task.


