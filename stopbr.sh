#!/bin/bash

ifconfig tap0 0.0.0.0 promisc down
tunctl -d tap0
ifconfig br0 0.0.0.0 promisc down
brctl delbr br0