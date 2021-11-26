#include "unix/poll.h"
#include "unix/stdint.h"
#include "syscalls.h"

int32_t poll(struct pollfd fds[], nfds_t nfds, int32_t timeout) {
	//See: UNIX Systems Programming for SVR4 (1e), page 149
	//   & POSIX Base Definitions, Issue 6, page 858

	nfds_t selected = 0; //count of fds with non-zero revents on completion
	for(nfds_t i = 0; i < nfds; i++) {
		if(fds[i].fd < 0) {
			continue;
		}

		fds[i].revents = 0; //clear revents

		//check events, if no requested events are set
		uint16_t events = fds[i].events;
		if((events & POLLIN) == POLLIN) {

		}
		sleep(timeout);

		//set POLLHUP, POLLERR, & POLLNVAL in events, even if not requested

		//TODO: update

		if(fds[i].revents != 0) {
			selected++;
		}
	}
	return selected;
}
