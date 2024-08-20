#include <iostream>
#include <utils.h>
#include <jsonutils.h>
#include <fstream>
#include <list>
#include <vector>
#include <memory>

#include <string.h>
#include <unistd.h>

#include <alloca.h>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sstream>
#include <future>
#include <optional>


void readFull(int fd, void* buf, size_t size) {
	size_t pos=0;
	size_t errCnt=0;
	while (pos<size) {
		ssize_t ret=read(fd, static_cast<char*>(buf)+pos,size-pos);
		if (ret==-1) {
			if (EAGAIN==errno || EINTR==errno) {
				if (++errCnt>100) throw std::runtime_error("Too many errors reading from "+std::to_string(fd)+": "+utils::errno_string());
				continue;
			} else {
				throw std::runtime_error("error reading from "+std::to_string(fd)+": "+utils::errno_string());
			}
		}
		if (ret==0) std::runtime_error("unexpected end of file");
		pos+=ret;
	}
}
void writeFull(int fd, const void* buf, size_t size) {
	size_t pos=0;
	size_t errCnt=0;
	while (pos<size) {
		ssize_t ret=write(fd, static_cast<const char*>(buf)+pos,size-pos);
		if (ret==-1) {
			if (EAGAIN==errno || EINTR==errno) {
				if (++errCnt>100) throw std::runtime_error("Too many errors writing to "+std::to_string(fd)+": "+utils::errno_string());
				continue;
			} else {
				throw std::runtime_error("error writing to "+std::to_string(fd)+": "+utils::errno_string());
			}
		}
		if (ret==0) std::runtime_error("unexpectedly can't write");
		pos+=ret;
	}
}

json::jobjptr readPacket(int fd) {
	uint32_t sizeRaw;
	readFull(fd, &sizeRaw,4);
	uint32_t size=ntohl(sizeRaw);
	if (size>512*1024) throw std::runtime_error("packet size is too large: "+std::to_string(size));
	char* buf=(char*)alloca(size);
	readFull(fd,buf,size);
	size_t consumed;
	json::jptr j=json::parseArrayOrObject(buf, buf+size, &consumed);
	if (consumed!=size) {
		std::stringstream ss;
		ss<<j;
		std::cerr<<"Unexpected extra length in packet: "+std::to_string(size-consumed)<<" after "<<ss.str()<<std::endl;
	}
	if (!j->isJsonObject()) {
		std::stringstream ss;
		ss<<json::Pretty(j);
		throw std::runtime_error("will only accept json objects, got: "+ss.str());
	}
	return j->getAsJsonObject();
}
void writePacket(int fd, const json::jptr& j) {
	if (!j) throw std::runtime_error("Will not write null json object");
	std::string s=j->toString();
	uint32_t sizeRaw=htonl((uint32_t)s.size());
	writeFull(fd,&sizeRaw,4);
	writeFull(fd,s.c_str(),s.size());
}


int main(int argc, char **argv) {
	if (argc!=2) {
		std::cerr<<"usage "<<argv[0]<<" <imagefile>"<<std::endl;
	}
	std::string testImageFile=argv[1];

	std::string dirout, err;
	std::string dirin = "echo -n `dirname \"" + std::string(argv[0]) + "\"`";
	if (utils::sh(dirin.c_str(), &dirout, &err)) {
		std::cout << "Oops: " << err << std::endl;
		std::cerr<<"dirname utility didn't work on: "<<dirin<<std::endl;
		return 1;
	}
	std::string darkslave= dirout + "/darkslave";
	std::string w=dirout + "/yolov3-tiny.weights";
	std::string c=dirout + "/cfg/yolov3-tiny.cfg";
	std::string n=dirout + "/data/coco.names";
	std::string l=dirout + "/data/labels";

    int fd1[2]; // (read,write)  parent->child, parent writes fd1[1], child reads fd1[0]
    int fd2[2]; // (read, write) parent<-child, child writes fd2[1], parent reads fd2[0]
    if (pipe(fd1) == -1) {
        std::cerr<<"Pipe Failed "<<utils::errno_string()<<std::endl;
        return 1;
    }
    if (pipe(fd2) == -1) {
        std::cerr<<"Pipe Failed "<<utils::errno_string()<<std::endl;
        return 1;
    }
    int parentOut=fd1[1];
    int parentIn=fd2[0];

    int childOut=fd2[1];
    int childIn=fd1[0];



	pid_t child=fork();
	if (child<0) {
		std::cerr<<utils::errno_string()<<std::endl;
		return 1;
	}
	if (child==0) {
		close(parentOut);
		close(parentIn);

		dup2(childIn, 0);
		dup2(childOut,1);

		execl(darkslave.c_str(), darkslave.c_str(), "-c", c.c_str(), "-w", w.c_str(), "-n", n.c_str(), "-l", l.c_str(), NULL );
		std::cerr<<"error to exec : "<<utils::errno_string()<<std::endl;
		return 1;
	}
	close(childOut);
	close(childIn);


	err.clear();
	std::string baseout;
	std::string basein = "echo -n `basename \"" + testImageFile + "\" .jpg`";
	if (utils::sh(basein.c_str(), &baseout, &err)) {
		std::cout << "Oops: " << err << std::endl;
		std::cerr<<"basename utility didn't work on: "+basein;
		return 1;
	}
	std::string outFile = baseout + "-predictions.jpg";

	try {
		for (int i=0 ; i<100; ++i) {
			json::jobjptr testInput=json::makeJsonObject();
			testInput->add("file",testImageFile);

			testInput->add("outFile",outFile);
			testInput->add("thresh", 0.25);
			testInput->add("hier_thresh", 0.5);
			testInput->add("nms", 0.45);

			std::cerr<<"Sending: "<<json::Pretty(testInput)<<std::endl;
			auto start=utils::currentTimeMilliseconds();

			writePacket(parentOut, testInput);
			json::jobjptr  out=readPacket(parentIn);

			auto end=utils::currentTimeMilliseconds();
			std::cerr<<"Got back: "<<json::Pretty(out)<<"\nin "<<(end-start)<< " ms."<<std::endl;
		}
	} catch (const std::exception &ex) {
		std::cout << "Failed with: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}

