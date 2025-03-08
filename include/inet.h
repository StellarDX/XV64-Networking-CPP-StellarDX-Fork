#pragma once

#ifndef NETWORK_H
#define NETWORK_H

int ARPRequest(unsigned int IP);

void SetIPAddress(int DevIndex, unsigned int Addr, unsigned int Mask);
void DelIPAddress(int DevIndex);
void ShowIPAddress();
void PrintARPTable();
void RTAddStatic(unsigned int Destination, unsigned int Gateway, unsigned int GenMask, int Device);
void RTPrint();
//void RTDelete();
void Ping(unsigned int IP);

unsigned StringToIPHex(const char* Str, _Bool* OK);

#endif // NETWORK_H
