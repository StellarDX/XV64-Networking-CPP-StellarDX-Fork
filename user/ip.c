#include "types.h"
#include "unix/stdio.h"
#include "unix/stdlib.h"
#include "string.h"
#include "syscalls.h"
#include "unix/stdint.h"
#include "inet.h"

unsigned StringToIPHex(const char* Str, _Bool* OK)
{
    char* StartPoint = (char*)Str, *EndPoint;
    union{uint32_t i; uint8_t b[4];} Result;
    long Temp = 0;
    Result.i = 0;

    for (int i = 0; i < 4; ++i)
    {
        Temp = ustrtol(StartPoint, &EndPoint, 10);
        if ((Temp < 0 || Temp > 255) ||
            (StartPoint == EndPoint) ||
            ((i == 3 && *EndPoint != '\0') || (i != 3 && *EndPoint != '.')))
        {
            if (OK) {*OK = 0;}
            return -1;
        }
        Result.b[i] = Temp;
        StartPoint = EndPoint + 1;
    }

    if (OK) {*OK = 1;}
    return Result.i;
}

void addr(int argc, char *argv[])
{
    if (!strncmp(argv[2], "add", -1))
    {
        _Bool OK = 1;
        uint32_t IP = StringToIPHex(argv[3], &OK);
        if (!OK) {procexit();}
        uint32_t Mask = StringToIPHex(argv[4], &OK);
        if (!OK) {procexit();}
        int DeviceIndex = -1;
        for (int i = 5; i < argc; ++i)
        {
            if (!strncmp(argv[i], "dev", -1) && i + 1 < argc)
            {
                DeviceIndex = atoi(argv[i + 1]);
                ++i;
            }
        }
        if (DeviceIndex < 0) {procexit();}
        SetIPAddress(DeviceIndex, IP, Mask);
    }
    else if (!strncmp(argv[2], "del", -1))
    {
        int DeviceIndex = -1;
        for (int i = 3; i < argc; ++i)
        {
            if (!strncmp(argv[i], "dev", -1) && i + 1 < argc)
            {
                DeviceIndex = atoi(argv[i + 1]);
                ++i;
            }
        }
        if (DeviceIndex < 0) {procexit();}
        DelIPAddress(DeviceIndex);
    }
    else {ShowIPAddress();}
}

void route(int argc, char *argv[])
{
    uint32_t IP = 0;
    uint32_t Mask = 0;
    uint32_t Gate = 0;
    int DeviceIndex = -1;
    if (!strncmp(argv[2], "add", -1))
    {
        int ExtraStart = 4;
        if (strncmp(argv[3], "default", -1))
        {
            _Bool OK = 1;
            IP = StringToIPHex(argv[3], &OK);
            if (!OK) {procexit();}
            Mask = StringToIPHex(argv[4], &OK);
            if (!OK) {procexit();}
            ExtraStart = 5;
        }
        for (int i = ExtraStart; i < argc; ++i)
        {
            if (!strncmp(argv[i], "via", -1) && i + 1 < argc)
            {
                _Bool OK = 1;
                Gate = StringToIPHex(argv[i + 1], &OK);
                if (!OK) {procexit();}
                ++i;
            }
            if (!strncmp(argv[i], "dev", -1) && i + 1 < argc)
            {
                DeviceIndex = atoi(argv[i + 1]);
                ++i;
            }
        }
        RTAddStatic(IP, Gate, Mask, DeviceIndex);
    }
    else {RTPrint();}
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf(R"(Usage: ip [ OPTIONS ] OBJECT { COMMAND | help }
       ip [ -force ] -batch filename
where  OBJECT := { address | addrlabel | fou | help | ila | ioam | l2tp | link |
                   macsec | maddress | monitor | mptcp | mroute | mrule |
                   neighbor | neighbour | netconf | netns | nexthop | ntable |
                   ntbl | route | rule | sr | stats | tap | tcpmetrics |
                   token | tunnel | tuntap | vrf | xfrm }
       OPTIONS := { -V[ersion] | -s[tatistics] | -d[etails] | -r[esolve] |
                    -h[uman-readable] | -iec | -j[son] | -p[retty] |
                    -f[amily] { inet | inet6 | mpls | bridge | link } |
                    -4 | -6 | -M | -B | -0 |
                    -l[oops] { maximum-addr-flush-attempts } | -echo | -br[ief] |
                    -o[neline] | -t[imestamp] | -ts[hort] | -b[atch] [filename] |
                    -rc[vbuf] [size] | -n[etns] name | -N[umeric] | -a[ll] |
                    -c[olor]}
)");
    }
    if (!strncmp(argv[1], "a", -1) || !strncmp(argv[1], "addr", -1) || !strncmp(argv[1], "address", -1))
    {
        addr(argc, argv);
    }
    else if (!strncmp(argv[1], "r", -1) || !strncmp(argv[1], "route", -1))
    {
        route(argc, argv);
    }
    return procexit();
}
