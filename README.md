Scope
=====

A tool to display real time state changes for Clockwork programs

sampler - subscribes to a zmq channel and optionally republishes messages with some keyword compression

filter - reads stdin and applies regular expressions to determine those that are to be forwarded to stdout

scope - reads machine state change events on stdin 
 and outputs a columnwise oscilloscope showing current state of each machine
 