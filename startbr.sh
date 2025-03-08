#!/bin/bash

Device="ens35"

#ifconfig br0 0.0.0.0 promisc down
#brctl delbr br0
brctl addbr br0                     # 添加一座名为 br0 的网桥
brctl addif br0 $Device             # 在 br0 中添加一个接口
brctl stp br0 off                   # 如果只有一个网桥，则关闭生成树协议
brctl setfd br0 1                   # 设置 br0 的转发延迟
brctl sethello br0 1                # 设置 br0 的 hello 时间
ifconfig br0 0.0.0.0 promisc up     # 启用 br0 接口
ifconfig $Device 0.0.0.0 promisc up # 启用网卡接口
dhclient br0                        # 从 dhcp 服务器获得 br0 的 IP 地址
brctl show br0                      # 查看虚拟网桥列表
brctl showstp br0                   # 查看 br0 的各接口信息

#ifconfig tap0 0.0.0.0 promisc down
#tunctl -d tap0
tunctl -t tap0 -u root              # 创建一个 tap0 接口，只允许 root 用户访问
brctl addif br0 tap0                # 在虚拟网桥中增加一个 tap0 接口
ifconfig tap0 0.0.0.0 promisc up    # 启用 tap0 接口
brctl showstp br0