
Notes on starting parasol on OpenStack VM machines.

Task: Establish a cluster of machines with the parasol cluster management
      system installed and running, and with UCSC/kent/lastz commands and
      scripts to perform the typical UCSC lastz/chainNet pipeline of
      processing.

Theory of operation:

1. Start up one machine to perform the function of the parasol 'hub'.
   Add utilities for kent commands, lastz binaries, and parasol administration.
   NFS export a filesystem for common use by the parasol nodes.
   This hub machine can donate extra CPU cores to add to the pool of resources
   for this parasol cluster.  Two CPU cores should be reservered for
   parasol and NFS operations.

2. With the known internal IP address for the parasol 'hub' machine,
   start up some number of instances to perform the functions
   of the parasol 'nodes'.  These nodes use the IP address of the 'hub'
   machine to mount the filesystem from the 'hub' machine.
   Minimal installation of anything else is not required on these
   machines, they can obtain everything they need from the NFS filesystem
   from the 'hub' machine.  And they can report to the 'hub' machine
   their individual characteristics for the parasol administration
   to add them to the pool of resources for this cluster.

3. After all the machine instances are up and running and proven to
   have correct ssh access between the hub and the nodes, the parasol
   server on the hub can be started.

4. The system should now be ready to run the lastz/chainNet process.
   Work on the hub machine, keep the data in the NFS exported filesystem
   so that all nodes can see the same set of files.
