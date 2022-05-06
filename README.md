# BerkeleySharedMemoryFIFO

[![DOI](https://zenodo.org/badge/218393925.svg)](https://zenodo.org/badge/latestdoi/218393925)

Shared Memory FIFO Library

This library provides a shared memory implementation of a FIFO.
Its operation is similar to that of POSIX pipes.

This code requires linking with `-pthreads -lrt` and compiling/linking with `--std=C11` or higher.

## License
This code is availible under the BSD License.

## Citing This Software:
If you would like to reference this software, please cite Christopher Yarp's Ph.D. thesis.

*At the time of writing, the GitHub CFF parser does not properly generate thesis citations.  Please see the bibtex entry below.*

```bibtex
@phdthesis{yarp_phd_2022,
	title = {High Speed Software Radio on General Purpose CPUs},
	school = {University of California, Berkeley},
	author = {Yarp, Christopher},
	year = {2022},
}
```
