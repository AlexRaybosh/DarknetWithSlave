
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <fcntl.h>
#include <libgen.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <locale.h>
#include <alloca.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <chrono>
#include <thread>

#include "utils.h"


#define BUF_SIZE 1024

namespace utils {



std::string errno_string() {
	auto z=strerror_l(errno, uselocale((locale_t)0));
	return z?std::string(z):"";
}
void errno_string(std::string& str) {
	str=errno_string();
}

void errno_exception(const std::string& msg) {
	auto e=errno_string();
	throw std::runtime_error(msg+": "+e);
}

extern "C" {
	char ** environ;
}
struct ARGS {
	std::vector<std::string> args;
	ARGS() {
		std::string pp="/proc/"+std::to_string(getpid())+"/cmdline";
		auto cmdArgs=slurpBinFile(pp.c_str());
		std::string current;
		for (auto c : cmdArgs) {
			if (c) current+=c;
			else {
				args.emplace_back(std::move(current));
				current="";

			}
		}
		if (current.size()>0) {
			args.emplace_back(std::move(current));
		}
	}
};
ARGS _args;

const std::vector<std::string>& getArgs() {
	return _args.args;
}





int sh(const char* cmd, std::string *out, std::string* err) {
	pid_t pid;
	int cout_pipe[2];
	int cerr_pipe[2];
	if (pipe(cout_pipe) || pipe(cerr_pipe)) {
		errno_exception("Failed to open pipes for "+std::string(cmd));
	}
	posix_spawn_file_actions_t action;
	posix_spawn_file_actions_init(&action);
	std::shared_ptr<posix_spawn_file_actions_t> closeAction(&action,posix_spawn_file_actions_destroy);
	posix_spawn_file_actions_addclose(&action, cout_pipe[0]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[0]);
	posix_spawn_file_actions_adddup2(&action, cout_pipe[1],1);
	posix_spawn_file_actions_adddup2(&action, cerr_pipe[1],2);
	posix_spawn_file_actions_addclose(&action, cout_pipe[1]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[1]);

	const char* argv[]={"sh", "-c", cmd, NULL};
	if (posix_spawn(&pid, "/bin/sh", &action,NULL, (char**)argv, environ)!=0) {
		errno_exception("posix_spawn failed for "+std::string(cmd));
	}
	::close(cout_pipe[1]); // cout_pipe[0] is the read end of the parent
	::close(cerr_pipe[1]);
	int flags1=::fcntl(cout_pipe[0],F_GETFL, 0);
	int flags2=::fcntl(cerr_pipe[0],F_GETFL, 0);
	::fcntl(cout_pipe[0],F_SETFL, flags1 | O_NONBLOCK);
	::fcntl(cerr_pipe[0],F_SETFL, flags2 | O_NONBLOCK);

	std::map<int,std::string*> m= { {cout_pipe[0],out}, {cerr_pipe[0],err} };

	for (;;) {
		if (m.size()==0) break;
		int fdmax=-1;
		fd_set read_fds;
		FD_ZERO(&read_fds);
		for (auto it=m.begin(), end=m.end(); it!=end;++it) {
			int fd=it->first;
			if (fd > fdmax) fdmax=fd;
			FD_SET(fd,&read_fds);
		}
		if (select(fdmax+1,&read_fds,NULL,NULL,NULL)==-1) {
			if (errno==EINTR) continue;
			errno_exception("select failed");
		}
		for (auto it=m.begin(); it!=m.end();) {
			bool close=false;
			int fd=it->first;
			std::string* log=it->second;
			if (FD_ISSET(fd,&read_fds)) {
				char buffer[BUF_SIZE];
				int length=sizeof(buffer);
				int nb=::read(fd, buffer, length);
				if (log && nb > 0) log->append(buffer,buffer+nb);
				if (nb <= 0 ) {
					FD_CLR(fd, &read_fds);
					::close(fd);
					close=true;
				}
			}
			if (close)
				m.erase(it++);
			else
				++it;


		}
	}
	for (;;) {
		int status;
		pid_t reported_pid=::waitpid(pid, &status, 0);
		if (reported_pid==-1) {
			if (errno==EINTR) continue;
			errno_exception("waitpid failed for "+std::string(cmd));
		}
		if (reported_pid==pid)
			return status;

	}
}




void dumpToFile(const std::string& fileName, const void* buf, size_t len) {
	FD fd(::creat(fileName.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH));
	if (!fd) errno_exception("Failed to open "+fileName);
	size_t left=len;
	char* ptr=(char*)(buf);
	while (left>0) {
		auto w=::write((int)fd,ptr,left);
		if (w<0) {
			if (errno==EINTR) continue;
			errno_exception("failed on writing to "+fileName);
		} else {
			left-=w;
			ptr+=w;
		}
	}
}

bool isFileSystemObject(const std::string& name) {
	struct stat st;
	return (0==::stat(name.c_str(), &st));
}

bool isRegularFile(const std::string& name, ssize_t* ps) {
	struct stat st;
	int sr=::stat(name.c_str(), &st);
	if (sr==0) {
		if (ps) *ps=st.st_size;
		return (st.st_mode & S_IFMT)==S_IFREG;
	}
	if (ps) *ps=-1;
	return false;
}
bool isDirectory(const std::string& name) {
	struct stat st;
	return (0==::stat(name.c_str(), &st)) && (st.st_mode & S_IFMT)==S_IFDIR;
}

void splitDirBasename(const std::string& name, std::string& dir,std::string& base) {
	char* buf=(char*)alloca(name.length()+2);
	::strcpy(buf,name.c_str());
	dir = ::dirname(buf);
	::strcpy(buf,name.c_str());
	base=basename(buf);
}

void mkdir_p(const std::string& name) {
	if (name.length()==0) throw std::runtime_error("can't create empty directory");
	if (isDirectory(name)) return;
	if (isFileSystemObject(name)) throw std::runtime_error(name+" already exists");
	std::string dir, base;
	splitDirBasename(name,dir,base);
	if (!isDirectory(dir)) {
		mkdir_p(dir);
	}
	FD dirFD(::open(dir.c_str(), O_DIRECTORY | O_RDONLY));
	if (!dirFD) errno_exception("Failed to open: "+dir);
	if (0!=::mkdirat((int)dirFD, base.c_str(),S_IRWXU | S_IRWXG | S_IRWXO))
		errno_exception("Failed to create sub-directory "+base+" under "+dir);
}
std::string slurpTextFile(const std::string& fileName) {
	FD fd(::open(fileName.c_str(), O_RDONLY));
	if (!fd) errno_exception(std::string("Failed to open: ")+fileName);
	ssize_t es=-1;
	isRegularFile(fileName,&es);
	std::string str;
	if (es>0) str.reserve(es);
	char buf[BUF_SIZE];
	for (;;) {
		ssize_t n=read(fd.fd, buf, sizeof(buf));
		if (n==0) break;
		if (n==-1) {
			if (errno==EINTR) continue;
			errno_exception("Failed to read from: "+std::string(fileName));
		}
		str.append(buf, buf+n);
	}
	return std::move(str);
}

 std::vector<char> slurpBinFile(const std::string&  fileName) {
	FD fd(::open(fileName.c_str(), O_RDONLY));
	if (!fd) errno_exception(std::string("Failed to open: ")+fileName);
	ssize_t es=-1;
	isRegularFile(fileName,&es);
	std::vector<char> v;
	if (es>0) v.reserve(es);
	char buf[BUF_SIZE];
	for (;;) {
		ssize_t n=read(fd.fd, buf, sizeof(buf));
		if (n==0) break;
		if (n==-1) {
			if (errno==EINTR) continue;
			errno_exception("Failed to read from: "+std::string(fileName));
		}
		v.insert(v.end(),buf, buf+n);
	}
	return std::move(v);
}

uint64_t currentTimeMilliseconds() {
	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	return millis;
}
uint64_t currentTimeMicroseconds() {
	std::chrono::time_point<std::chrono::system_clock> now =std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto micro = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
	return micro;
}



}

