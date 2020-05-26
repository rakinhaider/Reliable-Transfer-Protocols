# Reliable Data Transfer Protocols
Implementation of Three Protocols of Reliable Data Transfer

- Stop and Wait
- Go-Back-N
- Selective Repeat

To implement these algorithms a simulated environment is used. Here,

- Event are created and stored in a queue. Events inculde sending message, receiving message, timer timeout.
- At each step, an event is popped from the queue.
- The corresponding actions are performed and necessary events are created.
