# Linux Kernel Programming: Understanding RCU (Read-Copy-Update)

![Linux Kernel](https://img.shields.io/badge/Linux-Kernel-black?logo=linux)
![Language](https://img.shields.io/badge/Language-C-blue)
![Topic](https://img.shields.io/badge/Topic-RCU-green)
![Level](https://img.shields.io/badge/Level-Beginner-orange)

## 1. Introduction

RCU stands for **Read-Copy-Update**.

It is a synchronization mechanism used inside the Linux kernel to allow multiple readers to access shared data efficiently while writers update or remove that data.

The primary goal of RCU is:

- Allow readers to access shared data concurrently.
- Avoid making readers wait for writers in the usual RCU read-side pattern.
- Prevent memory from being reclaimed while relevant readers may still access it.
- Improve performance in read-heavy workloads.

### Real-world analogy

Imagine a library containing a student register.

Two people are working:

- Reader: Reading student information.
- Writer: Removing or updating student information.

Suppose the Reader is reading Rahul's information.

At the same time, the Writer wants to remove Rahul's entry.

The Writer can remove Rahul from the register, but cannot immediately destroy the memory containing Rahul's information.

Why?

Because the Reader may still be accessing it.

RCU solves this problem by delaying memory reclamation until the relevant readers have finished.

---

# 2. Understanding Reader and Writer

First, understand an important concept:

**Reader and Writer are roles performed by executing code. They are NOT necessarily different CPUs.**

A Reader can execute on CPU 0.

A Writer can execute on CPU 1.

They can also execute on the same CPU at different times.

### Different CPU example

```text
             CPU 0                    CPU 1
             READER                   WRITER

                |                        |
                |                        |
          rcu_read_lock()                 |
                |                        |
          Read shared data                |
                |                    Remove object
                |                        |
          Continue reading               |
                |                    Retire object
                |                        |
          rcu_read_unlock()               |
                |                        |
                |                  Wait for grace
                |                     period
                |                        |
                |                   Reclaim memory
```

### Same CPU example

```text
                 CPU 0
                   |
             Reader starts
                   |
              Read object
                   |
          Reader gets preempted
                   |
              Writer runs
                   |
             Remove object
                   |
             Reader resumes
                   |
             Finish reading
                   |
             Grace period
                   |
             Reclaim memory
```

NOTE:

Linux supports preemptible RCU, so an RCU reader can be preempted.

---

# 3. What is an RCU Reader?

A Reader accesses data protected by RCU.

Linux provides two important functions:

```c
rcu_read_lock();

    // Access RCU-protected data

rcu_read_unlock();
```

### rcu_read_lock()

Begins an RCU read-side critical section.

It tells the RCU subsystem that the current execution context may access RCU-protected data.

### rcu_read_unlock()

Ends the read-side critical section.

It tells RCU that this reader has finished its protected access.

### Reader flow

```text
       READER STARTS
             |
             |
       rcu_read_lock()
             |
             |
       Access shared data
             |
             |
       Read object fields
             |
             |
       rcu_read_unlock()
             |
             |
       READER FINISHES
```

IMPORTANT:

`rcu_read_lock()` does not lock the entire data structure.

It does not prevent writers from modifying the shared data structure.

That is one of the major differences between RCU and traditional mutex-based synchronization.

---

# 4. What is an RCU Writer?

A Writer modifies or removes shared data.

For example, imagine a linked list:

```text
       SHARED LINKED LIST

       +----+    +----+    +----+    +----+
       | A  | -> | B  | -> | C  | -> | D  |
       +----+    +----+    +----+    +----+
```

Suppose the Writer wants to remove object C.

After removal:

```text
       +----+    +----+    +----+
       | A  | -> | B  | -> | D  |
       +----+    +----+    +----+
```

Object C is no longer reachable through the shared list.

However, an existing Reader may still hold a pointer to C.

Therefore, the Writer must not immediately free C.

This is the central problem that RCU solves.

---

# 5. What is Memory Reclamation?

Memory reclamation means releasing allocated memory so that it can be reused.

There are two separate operations:

1. Removing an object from a shared data structure.
2. Freeing the object's allocated memory.

These operations are NOT necessarily performed at the same time.

### Example

Before removal:

```text
       SHARED LIST

       A ---> B ---> C ---> D

                    |
                    |
                 Memory
               still valid
```

After removal:

```text
       SHARED LIST

       A ---> B ---> D


       Retired Object

       +-------------+
       |      C      |
       +-------------+
       Memory NOT freed yet
```

After the grace period:

```text
       SHARED LIST

       A ---> B ---> D


       Retired object C

       Memory reclaimed
       Available for reuse
```

IMPORTANT:

Reclaiming memory does not destroy physical RAM.

It releases the allocated memory back to the appropriate memory allocator.

---

# 6. What is an RCU Grace Period?

A grace period is an interval during which RCU waits for relevant pre-existing readers to finish their read-side critical sections.

Why is this necessary?

Imagine:

```text
TIME -------------------------------------->

READER:
       rcu_read_lock()
              |
              |
         Read object C
              |
              |
       rcu_read_unlock()


WRITER:
                    Remove object C
                           |
                           |
                    Start grace period
                           |
                           |
                    Wait for readers
                           |
                           |
                    Grace period ends
                           |
                           |
                    Reclaim object C
```

The important rule:

**An object removed from an RCU-protected data structure must not be reclaimed until the relevant grace period has completed.**

A grace period does not mean every reader in the entire system must stop.

It concerns the readers that could still hold references to the retired object under the applicable RCU flavor and protocol.

---

# 7. Who Does What in RCU?

This is one of the most important concepts to remember.

| Operation | Responsibility |
|---|---|
| Read object | Reader |
| Modify shared data | Writer |
| Remove object | Writer |
| Schedule deferred reclamation | Writer |
| Track grace periods | RCU subsystem |
| Execute deferred callback | RCU callback mechanism |
| Release memory | Reclamation callback |

Remember:

**Reader reads. Writer removes. RCU waits. Callback frees.**

---

# 8. RCU Protocol Flow

Complete RCU workflow:

```text
                 RCU PROTOCOL
                       |
                       |
                Shared Data
                       |
              +--------+--------+
              |                 |
              |                 |
           READER             WRITER
              |                 |
              |                 |
        rcu_read_lock()     Update object
              |                 |
              |                 |
        Read object        Remove object
              |                 |
              |                 |
        Read fields         call_rcu()
              |                 |
              |                 |
        rcu_read_unlock()        |
              |                 |
              |           Grace period
              |                 |
              |                 |
              |           Callback executes
              |                 |
              |                 |
              |            Free memory
              |                 |
              +--------+--------+
                       |
                       |
                 RCU COMPLETE
```

---

# 9. RCU vs Mutex

RCU and Mutex are different synchronization mechanisms.

| Feature | RCU | Mutex |
|---|---|---|
| Primary use | Read-heavy workloads | General mutual exclusion |
| Multiple readers | Yes | Depends on lock design |
| Reader blocks writer | Not in the usual RCU read-side pattern | Yes, if holding the same mutex |
| Writer blocks reader | Not in the usual RCU read-side pattern | Yes, if holding the same mutex |
| Memory reclamation | Requires grace-period handling | Requires appropriate lifetime management |
| Performance | Efficient for frequent reads | Locking overhead and contention possible |

RCU is especially useful when reads are much more frequent than updates.

---

# 10. Linux Kernel RCU Demonstration

We will now demonstrate RCU using a Linux Kernel Module.

### Project Structure

```text
rcu-demo/
|
|--- rcu_demo.c
|
|--- Makefile
|
|--- README.md
```


IMPORTANT:

This module demonstrates RCU-protected reading.

It does NOT demonstrate removing a process or reclaiming its memory.

The Linux kernel manages process lifecycle and reclamation through its own internal mechanisms.

---

# 11. Makefile

Create a file named Makefile.

```makefile
obj-m += rcu_demo.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

NOTE:

The indentation before $(MAKE) must be a TAB character.

---

# 12. Commands to Compile and Run

### Step 1: Check kernel version

```bash
uname -r
```

### Step 2: Check kernel build directory

```bash
ls /lib/modules/$(uname -r)/build
```

### Step 3: Compile the module

```bash
make
```

### Step 4: Load the module

```bash
sudo insmod rcu_demo.ko
```

### Step 5: View kernel logs

```bash
sudo dmesg | tail -30
```

Expected output (illustrative):

```text
RCU Demo: Module Loaded

PID=1 | NAME=systemd
PID=2 | NAME=kthreadd
PID=3 | NAME=kworker/0:0
PID=4 | NAME=kworker/0:1

RCU Demo: Total processes displayed = 10
```

Actual output depends on your Linux system.

### Step 6: Unload the module

```bash
sudo rmmod rcu_demo
```

### Step 7: Check unload message

```bash
sudo dmesg | tail -10
```

Expected:

```text
RCU Demo: Module Unloaded
```

### Step 8: Clean build files

```bash
make clean
```

---

# 13. Important RCU Functions

| Function | Purpose |
|---|---|
| rcu_read_lock() | Enter RCU read-side section |
| rcu_read_unlock() | Exit RCU read-side section |
| call_rcu() | Schedule callback after grace period |
| synchronize_rcu() | Wait for an RCU grace period |
| rcu_dereference() | Safely dereference an RCU-protected pointer |
| rcu_assign_pointer() | Publish an RCU-protected pointer |

NOTE:

`call_rcu()` schedules deferred reclamation asynchronously.

`synchronize_rcu()` waits for a grace period synchronously.

These APIs have specific usage requirements and should not be treated as interchangeable.

---

# 14. Common Beginner Questions

### Q1. Are Reader and Writer two different CPUs?

No.

Reader and Writer are execution roles.

They can run on different CPUs or the same CPU.

### Q2. Does rcu_read_lock() stop the Writer?

No.

It does not prevent writers from updating or removing objects.

### Q3. Who removes the object?

The Writer.

### Q4. Who frees the memory?

Usually a deferred callback arranged by the Writer or the relevant subsystem.

### Q5. Who manages the grace period?

The RCU subsystem.

### Q6. What does reclaim mean?

Reclaim means releasing allocated memory so that it can be reused.

### Q7. Can multiple Readers execute simultaneously?

Yes.

RCU is designed to support concurrent readers.

### Q8. Does RCU immediately free removed objects?

No.

It delays reclamation until the required grace period has completed.

### Q9. Is RCU useful for read-heavy workloads?

Yes.

RCU is particularly useful when reads are frequent and updates are relatively infrequent.

### Q10. Does RCU replace every Mutex or Spinlock?

No.

RCU solves specific concurrency and object-lifetime problems.

Other synchronization mechanisms may still be needed.

---

# 15. Important Concepts to Remember

```text
READER
   |
   |-- rcu_read_lock()
   |
   |-- Access shared object
   |
   |-- rcu_read_unlock()


WRITER
   |
   |-- Update or remove object
   |
   |-- Schedule deferred reclamation
   |
   |-- Wait for RCU grace period


RCU SUBSYSTEM
   |
   |-- Tracks readers
   |
   |-- Coordinates grace periods
   |
   |-- Executes callbacks when safe


CALLBACK
   |
   |-- Reclaims retired memory
```

## Final Takeaway

Remember this simple statement:

**Reader reads. Writer updates or removes. RCU waits for the relevant readers. Callback reclaims the memory.**

This is the foundation of understanding Read-Copy-Update in Linux kernel programming.

---

## References

1. Linux Kernel RCU Documentation:
   https://docs.kernel.org/RCU/whatisRCU.html

2. Linux Kernel RCU API:
   https://docs.kernel.org/RCU/

3. Linux Kernel Memory Management:
   https://docs.kernel.org/mm/

4. Linux Kernel Module Programming:
   https://docs.kernel.org/

---

## Learning Roadmap

Recommended topics to learn next:

1. Mutex vs Spinlock
2. Atomic Operations
3. RCU Read-Side Critical Sections
4. RCU Grace Periods
5. Deferred Memory Reclamation
6. Linked Lists with RCU
7. RCU-Based Kernel Data Structures
8. Lock-Free and Wait-Free Programming Concepts

**Happy Kernel Hacking!**

