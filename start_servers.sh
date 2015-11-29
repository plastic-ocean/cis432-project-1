#!/bin/bash

#uncomment the topolgy you want. The simple two-server topology is uncommented here.

# Change the SERVER variable below to point your server executable.
SERVER=server

SERVER_NAME=`echo $SERVER | sed 's#.*/\(.*\)#\1#g'`

# Generate a simple three-server linear topology
#$SERVER localhost 5020 localhost 5021 &
#$SERVER localhost 5021 localhost 5020 localhost 5022 &
#$SERVER localhost 5022 localhost 5021 &

# Generate a simple three-server triangular topology
#$SERVER localhost 5020 localhost 5021 localhost 5022 &
#$SERVER localhost 5021 localhost 5020 localhost 5022 &
#$SERVER localhost 5022 localhost 5021 localhost 5020 &

# Generate a capital-H shaped topology
#$SERVER localhost 6000 localhost 6001 &
#$SERVER localhost 6001 localhost 6000 localhost 6002 localhost 6003 &
#$SERVER localhost 6002 localhost 6001 &
#$SERVER localhost 6003 localhost 6001 localhost 6005 &
#$SERVER localhost 6004 localhost 6005 &
#$SERVER localhost 6005 localhost 6004 localhost 6003 localhost 6006 &
#$SERVER localhost 6006 localhost 6005 &

# Generate a 3x3 grid topology
$SERVER localhost 6000 localhost 6001 localhost 6003 &
$SERVER localhost 6001 localhost 6000 localhost 6002 localhost 6004 &
$SERVER localhost 6002 localhost 6001 localhost 6005 &
$SERVER localhost 6003 localhost 6000 localhost 6004 localhost 6006 &
$SERVER localhost 6004 localhost 6001 localhost 6003 localhost 6005 localhost 6007 &
$SERVER localhost 6005 localhost 6002 localhost 6004 localhost 6008 &
$SERVER localhost 6006 localhost 6003 localhost 6007 &
$SERVER localhost 6007 localhost 6006 localhost 6004 localhost 6008 &
$SERVER localhost 6008 localhost 6005 localhost 6007 &


echo "Press ENTER to quit"
read
pkill $SERVER_NAME
