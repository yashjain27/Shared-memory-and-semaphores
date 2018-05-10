# Shared-memory-and-semaphores
Implemented a producer-consumer structure using shared memory and semaphores


Program should copy a file named file1 to another file named f ile2 byte by byte through a shared buffer. 
Main process should fork two processes: one is producer and another is consumer. The main process then pauses waiting
for the signals from its children. The producer’s job is to move the bytes in file1 to a shared
buffer (implemented by shared memory), one byte at a time. Insert random delay between two
movings. The consumer’s job is to move the bytes in the shared buffer to f ile2, one byte at a
time. Also insert random delay between two movings.

Whenever the producer moves an item into the shared buffer, it sends a signal to main
process. Main process enters a ”P” in the monitoring table. Similarly, whenever the consumer
moves an item from the shared buffer, it sends a signal to main process. Main process then
enters a ”C” in the monitoring table. When producer and consumer have copied the entire file1,
the consumer sends a signal to main process. Main process outputs the monitoring table
