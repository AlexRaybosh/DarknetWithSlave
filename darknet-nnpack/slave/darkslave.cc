#include <iostream>
#include <utils.h>
#include <jsonutils.h>
#include <fstream>
#include <list>
#include <vector>
#include <memory>

#define NNPACK 1
#include <darknet.h>

#include <data.h>
#include <option_list.h>
#include <network.h>
#include <nnpack.h>

#include <dirent.h>
#include <string.h>
#include <unistd.h>

#include <utils.h>
#include <jsonutils.h>
#include <alloca.h>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sstream>
#include <future>
#include <optional>

extern "C" {
list* read_data_cfg(char *filename);
image load_image_thread(char *filename, int w, int h, int c, pthreadpool_t threadpool);
image** load_alphabet_dir(const char *dir);
void draw_detections(image im, detection *dets, int num, float thresh, char **names, image **alphabet, int classes);
void save_image_to_jpeg_file(image im, const char *name);
void save_image_png(image im, const char *name);
}

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


static std::vector<std::string> readLines(const std::string &filename);
static int countCpus();

json::jobjptr process(const json::jobjptr& input,network *net,const std::vector<std::string>& modelNames,const char **names,image **alphabet) {
	std::string* filePtr=json::getStringPtr(input, "file");
	if (filePtr==nullptr) throw std::runtime_error("No input \"file\" property in "+input->toString());
	image im = load_image_thread((char*) filePtr->c_str(), 0, 0, net->c, net->threadpool);
	image sized = letterbox_image_thread(im, net->w, net->h,net->threadpool);
	layer l = net->layers[net->n - 1];
	float *X = sized.data;
	network_predict(net, X);
	int nboxes = 0;
	std::optional<double> othresh=json::getDouble(input,"thresh");
	std::optional<double> ohier_thresh=json::getDouble(input,"hier_thresh");
	std::optional<double> onms=json::getDouble(input,"nms");

	float thresh = othresh?othresh.value():0.24;
	float hier_thresh = ohier_thresh?ohier_thresh.value():0.5;
	float nms = onms?onms.value():0.45;

	std::string* outFile=json::getStringPtr(input, "outFile");

	detection *dets = get_network_boxes(net, im.w, im.h, thresh, hier_thresh, 0, 1, &nboxes);
	json::jobjptr result = json::makeJsonObject();
	json::jobjptr detections = result->addJsonObject("detections");

	if (nms) {
		do_nms_sort(dets, nboxes, l.classes, nms);
	}
	result->add("image.w", im.w);
	result->add("image.h", im.h);
	for (int i = 0; i < nboxes; i++) {
		for (int j = 0; j < l.classes; ++j) {
			if (dets[i].prob[j] > thresh) {

				const auto& modelName=modelNames[j];
				json::jptr mo=detections->get(modelName);
				if (!mo) {
					mo=detections->addJsonArray(modelName);
				}
				auto d=mo->getAsJsonArray()->addJsonObject();

	            box& b = dets[i].bbox;
	            int left  = (b.x-b.w/2.)*im.w;
	            int right = (b.x+b.w/2.)*im.w;
	            int top   = (b.y-b.h/2.)*im.h;
	            int bot   = (b.y+b.h/2.)*im.h;
	            if(left < 0) left = 0;
	            if(right > im.w-1) right = im.w-1;
	            if(top < 0) top = 0;
	            if(bot > im.h-1) bot = im.h-1;

				d->add("box",i);
				d->add("box.x",dets[i].bbox.x);
				d->add("box.y",dets[i].bbox.y);
				d->add("box.h",dets[i].bbox.h);
				d->add("box.w",dets[i].bbox.w);

				d->add("image.left",left);
				d->add("image.right",right);
				d->add("image.top",top);
				d->add("image.bottom",bot);

				d->add("probability",dets[i].prob[j]);
			}
		}
	}
	if (outFile!=nullptr) {
		draw_detections(im, dets, nboxes, thresh, (char**) names, alphabet,l.classes);
		if (utils::endsWith(*outFile, ".jpg")) {
			std::string out(outFile->begin(), outFile->end()-4);
			save_image_to_jpeg_file(im, out.c_str());
		} else if (utils::endsWith(*outFile, ".png")) {
			std::string out(outFile->begin(), outFile->end()-4);
			save_image_png(im, out.c_str());

		} else {
			throw std::runtime_error("Only .jpg/.png output images are supported, got: "+*outFile);
		}
	}

	free_detections(dets, nboxes);
	free_image(im);
	free_image(sized);
	return result;
}

void runIt(int outfd, const std::string &namesFileName,	const std::string &confFileName, const std::string &weightsFileName, bool testMode, const std::string &testImageFile, const char *labelsDir) {
	std::vector<std::string> modelNames = readLines(namesFileName);
	const char **names = (const char**) alloca(modelNames.size() * sizeof(void*));
	for (size_t i = 0; i < modelNames.size(); ++i) {
		names[i] = modelNames[i].c_str();
	}

	image **alphabet = nullptr;
	if (labelsDir) {
		if (utils::isDirectory(labelsDir))
			alphabet = load_alphabet_dir(labelsDir);
		else
			std::cerr << "Invalid labels directory: '" << labelsDir
					<< "', wont be used." << std::endl;
	}

	network *net = load_network((char*) confFileName.c_str(),
			(char*) weightsFileName.c_str(), 0);
	set_batch_network(net, 1);
	nnp_initialize();
	net->threadpool = pthreadpool_create(countCpus());

	if (testMode) {
		int pipefd[2];
		if (pipe(pipefd)) throw std::runtime_error("error creating a pipe : "+utils::errno_string());
		utils::FD readFd=pipefd[0];
		utils::FD writeFd=pipefd[1];
		json::jobjptr testInput=json::makeJsonObject();
		testInput->add("file",testImageFile);
		std::string baseout, err;
		std::string basein = "echo -n `basename \"" + testImageFile + "\" .jpg`";
		if (utils::sh(basein.c_str(), &baseout, &err)) {
			std::cout << "Oops: " << err << std::endl;
			return throw std::runtime_error("basename utility didn't work on: "+basein);
		}
		std::string outFile = baseout + "-predictions.jpg";
		testInput->add("outFile",outFile);
		testInput->add("thresh", 0.25);
		testInput->add("hier_thresh", 0.5);
		testInput->add("nms", 0.45);

		auto writer=[](int fd,json::jobjptr j) {
			writePacket(fd, j);
		};
		auto wf=std::async(std::launch::async,writer,writeFd.fd,testInput);
		json::jobjptr input=readPacket(readFd.fd);
		std::cout<<"Got generated test input packet over the pipe: "<<json::Pretty(input)<<std::endl;
		wf.get();

		auto start=utils::currentTimeMilliseconds();
		json::jobjptr outPacket=process(input,net,modelNames,names,alphabet);

		std::cout<<"Produced output packet: "<<json::Pretty(outPacket)<<"\nin "<<(utils::currentTimeMilliseconds()-start)<< " ms."<<std::endl;

		pthreadpool_destroy(net->threadpool);
		nnp_deinitialize();
		free_network(net);
		return;
	} else {
		int inFd=0;
		for (;;) {
			json::jobjptr inPacket=readPacket(inFd);
			json::jobjptr outPacket=process(inPacket,net,modelNames,names,alphabet);
			writePacket(outfd, outPacket);
		}
	}
}

int usage(const char *prg) {
	std::cout << "Usage: " << prg
			<< " -t <file> (optional, test image file) -c <file> (net config file, e.g. yolov3-tiny.cfg) "
					"-n <file> (names file, e.g. coco.names) -w <file> (weights file, e.g. yolov3-tiny.weights) -l <labels_dir>(optional, for labels)"
			<< std::endl;
	return 1;
}
int main(int argc, char **argv) {

	utils::FD outFd = dup(1);
	if (outFd < 0) {
		std::cerr << "Failed to clone standard output" << std::endl;
		return 1;
	}
	if (dup2(2, 1) < 0) {
		std::cerr << "Failed to redirect standard output to error" << std::endl;
		return 1;
	}

	for (int i=0;i<argc;++i) {
		std::cerr<<std::to_string(i)<<" - "<<argv[i]<<std::endl;
	}

	bool testMode = false;
	std::string confFileName, namesFileName, weightsFileName, testImageFile;
	const char *labelsDir = nullptr;

	opterr = 0;
	int opt;
	while ((opt = getopt(argc, argv, "t:c:n:w:l:")) != -1) {
		switch (opt) {
		case 't':
			testMode = true;
			testImageFile = optarg ? optarg : "";
			break;
		case 'c':
			confFileName = optarg ? optarg : "";
			break;
		case 'n':
			namesFileName = optarg ? optarg : "";
			break;
		case 'w':
			weightsFileName = optarg ? optarg : "";
			break;
		case 'l':
			labelsDir = optarg ? optarg : nullptr;
			break;
		default: /* '?' */
			return usage(argv[0]);
		}
	}
	if (confFileName.empty() || namesFileName.empty()
			|| weightsFileName.empty())
		return usage(argv[0]);

	std::cout << "Using net configuration file: " << confFileName << std::endl;
	std::cout << "Using names file: " << namesFileName << std::endl;
	std::cout << "Using weights file: " << weightsFileName << std::endl;
	if (testMode) {
		std::cout << "Using test image file: " << testImageFile << std::endl;
	}

	try {
		runIt(outFd.fd, namesFileName, confFileName, weightsFileName, testMode,
				testImageFile, labelsDir);
	} catch (const std::exception &ex) {
		std::cout << "Failed with: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}

static std::vector<std::string> readLines(const std::string &filename) {
	std::vector<std::string> ret;
	std::ifstream file(filename);
	if (!file.is_open())
		throw std::runtime_error(std::string("Error accessing ") + filename);
	std::string line;
	while (std::getline(file, line))
		ret.emplace_back(std::move(line));
	file.close();
	return std::move(ret);
}

static int cpuName(const struct dirent *e) {
	return 0 == strncmp("cpu", e->d_name, 3) && strlen(e->d_name) > 3
			&& e->d_name[3] >= '0' && e->d_name[3] <= '9';
}
static int countCpus() {
	//const char* dir="/sys/bus/cpu/devices";
	const char *dir = "/sys/devices/system/cpu";
	struct dirent **namelist;
	int num = scandir(dir, &namelist, cpuName, NULL);
	if (num == -1) {
		return 1;
	}
	for (int i = 0; i < num; ++i) {
		free(namelist[i]);
	}
	free(namelist);
	return num;
}

