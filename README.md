Scope
=====

A tool to display real time state changes for Clockwork programs

sampler - subscribes to an iod or cw zmq channel and optionally republishes messages with some keyword compression

filter - reads stdin and applies regular expressions to determine those that are to be forwarded to stdout

scope - reads machine state change events on stdin 
 and outputs a columnwise oscilloscope showing current state of each machine
 
sampler
-------

Sampler subscribes to a zmq channel from iod or from cw (see github.com/latproc/clockwork) and 
displays a timestamp along with machine state change and property value change records.
To pickup data from a remote machine, the most direct method is:

	sampler --subscribe hostname --subscribe-port port

hostname and port default to localhost and 5556 respectively.

Sampler builds a map of state names and machine names 
and is intended to send only the state and machine id, 
not the complete name, as machines move between states.
The recommended usage is to sample locally, performing
this conversion and then collect remotely:

On the data source host:

	sampler --publish-port 5561 --republish --quiet

On the data collection host:

	sampler --subscribe hostname --subscribe-port 5561 --raw

Note the use of '--raw' on the collection host. This is to prevent the 
collection host from trying to interpret the data again.

