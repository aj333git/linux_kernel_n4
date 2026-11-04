#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/rcupdate.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Learning");
MODULE_DESCRIPTION("RCU Process List Traversal Demo");

static int __init rcu_demo_init(void)
{
    struct task_struct *task;
    int count = 0;

    pr_info("RCU Demo: Module Loaded\n");

    /*
     * Enter RCU read-side critical section
     */
    rcu_read_lock();

    /*
     * Traverse process list
     */
    for_each_process(task)
    {
        pr_info(
            "PID=%d | NAME=%s\n",
            task->pid,
            task->comm
        );

        count++;

        if (count >= 10)
            break;
    }

    /*
     * Exit RCU read-side critical section
     */
    rcu_read_unlock();

    pr_info("RCU Demo: Total processes displayed = %d\n",
            count);

    return 0;
}

static void __exit rcu_demo_exit(void)
{
    pr_info("RCU Demo: Module Unloaded\n");
}

module_init(rcu_demo_init);
module_exit(rcu_demo_exit);

