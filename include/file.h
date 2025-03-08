#ifndef __FILE_H
#define __FILE_H

struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_SOCKET } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;

  struct SocketInfo
  {
      int Type;
      int Desc;
  }Socket;
};


// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  int flags;          // I_BUSY, I_VALID

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[/*NDIRECT+1*/29]; // TODO: make this not specific to fs1
};
#define I_BUSY 0x1
#define I_VALID 0x2

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define TTY0    1
#define TTY1    2
#define LOOP0   3

struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};

#endif
